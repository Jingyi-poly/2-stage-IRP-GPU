#include "ScenarioUtils.h"

// 全局日志统计map（定义）
std::map<std::string, LogStats> g_log_stats;
std::mutex g_log_mutex;
std::string g_log_file_path = "batch_run_log.txt";

// 记录函数调用开始
std::chrono::high_resolution_clock::time_point logFunctionStart(const std::string& function_name) {
    return std::chrono::high_resolution_clock::now();
}

// 记录函数调用结束并更新统计
void logFunctionEnd(const std::string& function_name, std::chrono::high_resolution_clock::time_point start_time) {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double time_ms = duration.count() / 1000.0;  // 转换为毫秒
    
    std::lock_guard<std::mutex> lock(g_log_mutex);
    
    // 更新统计信息
    g_log_stats[function_name].total_time_ms += time_ms;
    g_log_stats[function_name].call_count++;
    g_log_stats[function_name].avg_time_ms = g_log_stats[function_name].total_time_ms / g_log_stats[function_name].call_count;
    
    // 清空文件并重新写入所有统计信息
    std::ofstream log_file(g_log_file_path, std::ios::trunc);
    if (log_file.is_open()) {
        for (const auto& pair : g_log_stats) {
            const auto& stats = pair.second;
            log_file << "Function: " << pair.first 
                     << " | Total Time: " << stats.total_time_ms << " ms"
                     << " | Call Count: " << stats.call_count
                     << " | Avg Time: " << stats.avg_time_ms << " ms"
                     << std::endl;
        }
        log_file.close();
    }
} 
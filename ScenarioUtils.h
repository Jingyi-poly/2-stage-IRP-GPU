#ifndef SCENARIO_UTILS_H
#define SCENARIO_UTILS_H

#include <vector>
#include <thread>
#include <functional>
#include <iostream>
#include <algorithm>
#include <string>
#include <map>
#include <typeinfo>
#include <chrono>
#include <fstream>
#include <mutex>
#include "ThreadPool.h"

// 前向声明
class Individu;
class Population;
class Genetic;

// 简单的日志统计结构
struct LogStats {
    double total_time_ms = 0.0;  // 总耗时（毫秒）
    int call_count = 0;          // 调用次数
    double avg_time_ms = 0.0;    // 平均耗时
};

// 全局日志统计map（声明）
extern std::map<std::string, LogStats> g_log_stats;
extern std::mutex g_log_mutex;
extern std::string g_log_file_path;

// 记录函数调用开始
std::chrono::high_resolution_clock::time_point logFunctionStart(const std::string& function_name);

// 记录函数调用结束并更新统计
void logFunctionEnd(const std::string& function_name, std::chrono::high_resolution_clock::time_point start_time);

/**
 使用线程池的批处理函数，使用方式：
 BatchRunFunc([&a, &b, &c, &d](int s) {
    do something...
 }, total_threads, batch_size); 
 */
template<typename TaskFunc>
void BatchRunFunc(TaskFunc&& taskFunc, int nb_scenario, int batch_size = 40, bool trace = false) {
    // 获取函数类型名作为函数名
    std::string func_name = typeid(TaskFunc).name();
    // 记录函数调用开始
    auto start_time = logFunctionStart(func_name);
    
    // 性能优化：当场景数量很小时，直接使用顺序执行
    // 阈值可以根据实际测试调整，这里设置为4
    const int SEQUENTIAL_THRESHOLD = 20;
    
    if (g_thread_pool == nullptr || nb_scenario <= SEQUENTIAL_THRESHOLD) {
        if (trace && g_thread_pool == nullptr) {
            std::cout << "ThreadPool disabled - using gRPC-only sequential execution mode" << std::endl;
        }
        if (trace && nb_scenario <= SEQUENTIAL_THRESHOLD && g_thread_pool != nullptr) {
            std::cout << "Scenarios count (" << nb_scenario << ") <= threshold (" << SEQUENTIAL_THRESHOLD
                      << "), using sequential execution for better performance" << std::endl;
        }
        
        // 顺序执行
        for (int s = 0; s < nb_scenario; s++) {
            taskFunc(s);
        }
        // 记录函数调用结束
        logFunctionEnd(func_name, start_time);
        return;
    }

    if (trace) {
        std::cout << "Starting batch processing with " << batch_size << " scenarios per batch..." << std::endl;
        std::cout << "Total scenarios: " << nb_scenario << std::endl;
    }
    
    // 分批处理场景
    for (int batch_start = 0; batch_start < nb_scenario; batch_start += batch_size) {
        int batch_end = std::min(batch_start + batch_size, nb_scenario);
        int current_batch_size = batch_end - batch_start;
        
        if (trace) {
            std::cout << "Processing batch " << (batch_start / batch_size + 1) 
                      << " (scenarios " << batch_start << " to " << (batch_end - 1) << ")" << std::endl;
        }
        
        std::vector<std::future<void>> futures;
        
        // 提交任务到线程池
        for (int s = batch_start; s < batch_end; s++) {
            futures.push_back(g_thread_pool->enqueue([taskFunc, s]() {
                taskFunc(s);
            }));
        }
        
        // 等待当前批次的所有任务完成
        for (auto& future : futures) {
            future.wait();
        }
        
        if (trace) {
            std::cout << "Batch " << (batch_start / batch_size + 1) << " completed." << std::endl;
        }
    }
    
    // 记录函数调用结束
    logFunctionEnd(func_name, start_time);
}

#endif // SCENARIO_UTILS_H 
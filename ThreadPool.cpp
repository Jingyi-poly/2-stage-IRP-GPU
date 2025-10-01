#include "ThreadPool.h"

// 全局线程池指针
ThreadPool* g_thread_pool = nullptr;

// 初始化线程池
void initThreadPool(size_t num_threads) {
    if (g_thread_pool != nullptr) {
        std::cout << "ThreadPool already initialized!" << std::endl;
        return;
    }
    std::cout << "Initializing ThreadPool with " << num_threads << " threads..." << std::endl;
    g_thread_pool = new ThreadPool(num_threads);
}

// 销毁线程池
void destroyThreadPool() {
    if (g_thread_pool != nullptr) {
        delete g_thread_pool;
        g_thread_pool = nullptr;
        std::cout << "ThreadPool destroyed." << std::endl;
    }
} 
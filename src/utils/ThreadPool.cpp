#include "irp/utils/ThreadPool.h"

// Global thread pool pointer (for inventory optimization)
ThreadPool* g_thread_pool = nullptr;

// byOptimizededicatedThread poolPointer(gRPCmodebelow/underUse)
ThreadPool* g_routing_thread_pool = nullptr;

// InitializeThread pool
void initThreadPool(size_t num_threads) {
  if (g_thread_pool != nullptr) {
    std::cout << "ThreadPool already initialized!" << std::endl;
    return;
  }
  std::cout << "Initializing ThreadPool with " << num_threads << " threads..." << std::endl;
  g_thread_pool = new ThreadPool(num_threads);
}

// DestroyThread pool
void destroyThreadPool() {
  if (g_thread_pool != nullptr) {
    delete g_thread_pool;
    g_thread_pool = nullptr;
    std::cout << "ThreadPool destroyed." << std::endl;
  }
}

// InitializebyOptimizeThread pool
void initRoutingThreadPool(size_t num_threads) {
  if (g_routing_thread_pool != nullptr) {
    std::cout << "Routing ThreadPool already initialized!" << std::endl;
    return;
  }
  std::cout << "Initializing Routing ThreadPool with " << num_threads << " threads..." << std::endl;
  g_routing_thread_pool = new ThreadPool(num_threads);
}

// DestroybyOptimizeThread pool
void destroyRoutingThreadPool() {
  if (g_routing_thread_pool != nullptr) {
    delete g_routing_thread_pool;
    g_routing_thread_pool = nullptr;
    std::cout << "Routing ThreadPool destroyed." << std::endl;
  }
} 
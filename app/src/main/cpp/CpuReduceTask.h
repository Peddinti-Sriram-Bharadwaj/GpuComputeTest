#pragma once

#include "ComputeTask.h"    // We must implement this interface
#include "VulkanContext.h"  // For LOGI/LOGE macros
#include <vector>
#include <thread>
#include <numeric>
#include <chrono>
#include <pthread.h>      // For pthread_barrier_t

// We'll test with 1 million elements
// const size_t CPU_DATA_SIZE = 1024 * 1024;

class CpuReduceTask : public ComputeTask {
public:
    CpuReduceTask(size_t n);
    ~CpuReduceTask();

    // --- ComputeTask Interface ---
    // (We'll just log things, no Vulkan)
    void init() override;
    long long dispatch() override;
    void cleanup() override;

private:
    // The function each thread will run
    void reduceThread(size_t threadId);

    int m_numThreads;
    size_t m_n;

    // Our data buffers
    std::vector<float> m_data;
    std::vector<float> m_threadPartialSums;

    // The POSIX barrier for synchronization
    pthread_barrier_t m_barrier;
};
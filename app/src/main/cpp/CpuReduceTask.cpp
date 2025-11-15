#include "CpuReduceTask.h"
#include <cmath> // For log2

// --- Constructor / Destructor ---

// THIS IS THE FIX: The constructor must match the .h file
CpuReduceTask::CpuReduceTask(size_t n) : m_n(n) {
    m_numThreads = (int)std::thread::hardware_concurrency();
    if (m_numThreads == 0) m_numThreads = 4; // Fallback

    LOGI("CpuReduceTask created. N=%zu, Threads=%d", m_n, m_numThreads);

    // Initialize the barrier to wait for 'm_numThreads' threads
    pthread_barrier_init(&m_barrier, nullptr, m_numThreads);
}

CpuReduceTask::~CpuReduceTask() {
    pthread_barrier_destroy(&m_barrier);
    LOGI("CpuReduceTask destroyed");
}

// --- ComputeTask Interface Implementation ---

void CpuReduceTask::init() {
    LOGI("CpuReduceTask::init() - Allocating %zu floats...", m_n);
    m_data.resize(m_n); // Use m_n
    std::fill(m_data.begin(), m_data.end(), 1.0f);
    m_threadPartialSums.resize(m_numThreads);

    LOGI("CpuReduceTask::init() complete.");
}

void CpuReduceTask::cleanup() {
    m_data.clear();
    m_threadPartialSums.clear();
    LOGI("CpuReduceTask::cleanup() complete.");
}

long long CpuReduceTask::dispatch() {
    LOGI("CpuReduceTask::dispatch() starting for N=%zu...", m_n);

    auto startTime = std::chrono::high_resolution_clock::now();

    // --- 1. Launch Threads ---
    std::vector<std::thread> threads;
    for (int i = 0; i < m_numThreads; ++i) {
        threads.emplace_back(&CpuReduceTask::reduceThread, this, i);
    }

    // --- 2. Wait for all threads to finish ---
    for (auto& t : threads) {
        t.join();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // --- 3. Verify Result ---
    float result = m_threadPartialSums[0];
    float expected = (float)m_n; // Use m_n

    LOGI("--- CPU (N=%zu) ---", m_n);
    LOGI("Result: %.0f (Expected: %.0f)", result, expected);
    if (abs(result - expected) < 0.01f) {
        LOGI("SUCCESS");
    } else {
        LOGE("FAILED");
    }
    LOGI("Time: %lld microseconds", (long long)duration.count());

    return duration.count();
}


// --- The Core Threading Logic ---

void CpuReduceTask::reduceThread(size_t threadId) {

    // --- 1. Local Reduction (Phase 1) ---
    size_t dataPerThread = m_n / m_numThreads; // Use m_n
    size_t start = threadId * dataPerThread;
    size_t end = (threadId == m_numThreads - 1) ? m_n : start + dataPerThread; // Use m_n

    float sum = 0.0f;
    for (size_t i = start; i < end; ++i) {
        sum += m_data[i];
    }
    m_threadPartialSums[threadId] = sum;

    // --- 2. Tree Reduction (Phase 2) ---
    pthread_barrier_wait(&m_barrier);

    for (size_t s = m_numThreads / 2; s > 0; s >>= 1) {
        if (threadId < s) {
            m_threadPartialSums[threadId] += m_threadPartialSums[threadId + s];
        }
        pthread_barrier_wait(&m_barrier);
    }
}
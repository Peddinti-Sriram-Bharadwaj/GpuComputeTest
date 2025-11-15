#include "CpuReduceTask.h"
#include <cmath> // For log2

// --- Constructor / Destructor ---

CpuReduceTask::CpuReduceTask() {
    // Get the number of hardware cores
    m_numThreads = (int)std::thread::hardware_concurrency();
    if (m_numThreads == 0) m_numThreads = 4; // Fallback

    LOGI("CpuReduceTask created. Using %d threads.", m_numThreads);

    // Initialize the barrier to wait for 'm_numThreads' threads
    pthread_barrier_init(&m_barrier, nullptr, m_numThreads);
}

CpuReduceTask::~CpuReduceTask() {
    pthread_barrier_destroy(&m_barrier);
    LOGI("CpuReduceTask destroyed");
}

// --- ComputeTask Interface Implementation ---

void CpuReduceTask::init() {
    LOGI("CpuReduceTask::init() - Allocating memory...");

    // 1. Allocate and fill the main data buffer
    m_data.resize(CPU_DATA_SIZE);

    // Fill with 1.0, 1.0, 1.0, ...
    std::fill(m_data.begin(), m_data.end(), 1.0f);

    // 2. Allocate buffer for partial sums (one per thread)
    m_threadPartialSums.resize(m_numThreads);

    LOGI("CpuReduceTask::init() complete. %zu floats allocated.", CPU_DATA_SIZE);
}

void CpuReduceTask::cleanup() {
    // Clear the vectors to free memory
    m_data.clear();
    m_threadPartialSums.clear();
    LOGI("CpuReduceTask::cleanup() complete.");
}

void CpuReduceTask::dispatch() {
    LOGI("CpuReduceTask::dispatch() starting...");

    // Start the timer
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

    // Stop the timer
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // --- 3. Verify Result ---
    // The final result is in m_threadPartialSums[0]
    float result = m_threadPartialSums[0];
    float expected = (float)CPU_DATA_SIZE; // (since all data was 1.0)

    LOGI("CPU Reduce Result: %.0f", result);
    LOGI("Expected Result:   %.0f", expected);

    if (abs(result - expected) < 0.01f) {
        LOGI("--- CPU REDUCE SUCCESS ---");
    } else {
        LOGE("--- CPU REDUCE FAILED ---");
    }

    LOGI("CPU execution time: %lld microseconds", (long long)duration.count());
}


// --- The Core Threading Logic ---

void CpuReduceTask::reduceThread(size_t threadId) {

    // --- 1. Local Reduction (Phase 1) ---
    // Each thread calculates its own partial sum

    // Determine the chunk of data this thread is responsible for
    size_t dataPerThread = CPU_DATA_SIZE / m_numThreads;
    size_t start = threadId * dataPerThread;

    // The last thread takes any remaining elements
    size_t end = (threadId == m_numThreads - 1) ? CPU_DATA_SIZE : start + dataPerThread;

    float sum = 0.0f;
    for (size_t i = start; i < end; ++i) {
        sum += m_data[i];
    }

    // Store this thread's partial sum
    m_threadPartialSums[threadId] = sum;

    // --- 2. Tree Reduction (Phase 2) ---
    // All threads must finish their local sum before we combine them
    pthread_barrier_wait(&m_barrier);

    // We use a tree structure to combine the 'm_numThreads' partial sums
    // This is the same logic as our GPU shader, but on the CPU.
    // Example: 8 threads
    // Step 1: 4 threads (0, 1, 2, 3) add sums from (4, 5, 6, 7)
    // Step 2: 2 threads (0, 1) add sums from (2, 3)
    // Step 3: 1 thread (0) adds sum from (1)

    for (size_t s = m_numThreads / 2; s > 0; s >>= 1) {
        if (threadId < s) {
            // Add the value from the "other half"
            m_threadPartialSums[threadId] += m_threadPartialSums[threadId + s];
        }

        // Wait for all threads to finish this step before
        // proceeding to the next level of the tree
        pthread_barrier_wait(&m_barrier);
    }

    // After the loop, the final sum is in m_threadPartialSums[0].
    // Thread 0 is the only one that needs to do anything with it,
    // but in our case, it just exits, and the `dispatch()` function reads it.
}
#include <jni.h>
#include <string>
#include <stdexcept>
#include <android/asset_manager_jni.h> // <-- NEW: Include for assets

// Include our architecture
#include "VulkanContext.h"
#include "ComputeTask.h"
#include "VectorAddTask.h"
#include "LocalReduceTask.h"
#include "CpuReduceTask.h"
#include "GpuTreeReduceTask.h"

// --- Global Pointers ---
VulkanContext* g_context = nullptr;
ComputeTask* g_task = nullptr;
AAssetManager* g_assetManager = nullptr; // <-- NEW: Global asset manager


// --- Task Factory ---
enum class TaskID {
    VECTOR_ADD,
    LOCAL_REDUCE,
    CPU_REDUCE,
    GPU_TREE_REDUCE
};

ComputeTask* createTask(TaskID id, uint32_t n) {
    switch (id) {
        case TaskID::CPU_REDUCE:
            return new CpuReduceTask(n);
        case TaskID::GPU_TREE_REDUCE:
            if (g_assetManager == nullptr) {
                LOGE("AssetManager is null, cannot create GpuTask");
                return nullptr;
            }
            return new GpuTreeReduceTask(g_assetManager, n);
        default:
            return nullptr;
    }
}

// --- NEW JNI Function: Called from onCreate ---
extern "C" JNIEXPORT void JNICALL
Java_com_example_gpucomputetest_MainActivity_initJNI(
        JNIEnv* env,
        jobject /* this */,
        jobject assetManager) { // <-- NEW

    LOGI("--- initJNI(): Storing AssetManager ---");
    g_assetManager = AAssetManager_fromJava(env, assetManager);
    if (g_assetManager == nullptr) {
        LOGE("Failed to get AAssetManager");
    }
}


// --- JNI Entry Point: Called when the app starts ---
#include <jni.h>
#include <string>
#include <stdexcept>
#include <android/asset_manager_jni.h>
#include <vector>
#include <sstream> // <-- ADD THIS

// ... (includes for tasks) ...
// ... (globals, TaskID enum, createTask, initJNI are unchanged) ...

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_gpucomputetest_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {

    std::string resultMessage = "Iterative test finished. Check Logcat.";

    // --- Define the experiment ---
    const int NUM_ITERATIONS = 100;
    const uint32_t N = 1048576; // Our 1M element test size

    try {
        // --- 1. Init Vulkan (once) ---
        LOGI("--- Initializing Vulkan Context ---");
        g_context = VulkanContext::getInstance();
        g_context->init();

        // --- 2. WARMUP RUN ---
        // Warm up the GPU with one run
        LOGI("--- STARTING WARMUP RUN ---");
        ComputeTask* warmupTask = createTask(TaskID::GPU_TREE_REDUCE, N);
        if(warmupTask) {
            warmupTask->init();
            warmupTask->dispatch();
            warmupTask->cleanup();
            delete warmupTask;
        }
        LOGI("--- WARMUP COMPLETE ---");


        // --- 3. TIMED ITERATIVE (HYBRID) TEST ---
        LOGI("--- STARTING TIMED ITERATIVE TEST (100 runs) ---");

        // We re-create the task to start fresh
        ComputeTask* iterativeTask = createTask(TaskID::GPU_TREE_REDUCE, N);
        iterativeTask->init();

        long long totalIterativeTime = 0;

        // Start the timer for the whole loop
        auto startTime = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_ITERATIONS; i++) {
            // dispatch() waits for the GPU to finish each time.
            // This simulates a "hybrid" app that needs the result
            // back on the CPU before doing the next step.

            ((GpuTreeReduceTask*)iterativeTask)->reset();
            long long singleRunTime = iterativeTask->dispatch();
            totalIterativeTime += singleRunTime;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        long long totalLoopTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

        iterativeTask->cleanup();
        delete iterativeTask;

        LOGI("--- ITERATIVE TEST COMPLETE ---");

        // --- 4. FORMAT AND LOG FINAL TABLE ---
        std::stringstream ss;
        ss << "\n\n--- ITERATIVE (HYBRID) WORKLOAD RESULTS ---\n";
        ss << "Total iterations: " << NUM_ITERATIONS << "\n";
        ss << "Problem Size (N): " << N << "\n";
        ss << "Total time for " << NUM_ITERATIONS << " syncs (CPU clock): " << totalLoopTime << " µs\n";
        ss << "Average time per sync (dispatch() result): " << (totalIterativeTime / NUM_ITERATIONS) << " µs\n";
        ss << "--- END OF RESULTS ---\n\n";

        LOGI("%s", ss.str().c_str());

    } catch (const std::exception& e) {
        LOGE("!!! FATAL ERROR: %s", e.what());
        resultMessage = "Error: " + std::string(e.what());
    }

    return env->NewStringUTF(resultMessage.c_str());
}

// --- JNI Cleanup: Called when the app is destroyed ---
extern "C" JNIEXPORT void JNICALL
Java_com_example_gpucomputetest_MainActivity_cleanup(
        JNIEnv* env,
        jobject /* this */) {

    LOGI("--- Cleaning up compute resources ---");
    // g_task is no longer global, so we just clean the context
    if (g_context != nullptr) {
        g_context->cleanup();
    }
    LOGI("--- Cleanup complete ---");
}
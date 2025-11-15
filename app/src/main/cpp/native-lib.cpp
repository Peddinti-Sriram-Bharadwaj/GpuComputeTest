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

    std::string resultMessage = "Experiment finished. Check Logcat.";

    std::vector<uint32_t> testSizes = {
            256 * 1,    // 256
            256 * 4,    // 1,024
            256 * 16,   // 4,096
            256 * 64,   // 16,384
            256 * 128,  // 32,768
            256 * 256,  // 65,536
            256 * 512,  // 131,072
            256 * 1024, // 262,144
            256 * 2048, // 524,288
            256 * 4096  // 1,048,576
    };

    try {
        // --- 1. Init Vulkan (once) ---
        LOGI("--- Initializing Vulkan Context ---");
        g_context = VulkanContext::getInstance();
        g_context->init();

        // --- 2. WARMUP RUNS ---
        LOGI("--- STARTING WARMUP RUNS ---");
        for (uint32_t n : testSizes) {
            // Run CPU
            ComputeTask* taskCpu = createTask(TaskID::CPU_REDUCE, n);
            if(taskCpu) {
                taskCpu->init();
                taskCpu->dispatch(); // Run but ignore result
                taskCpu->cleanup();
                delete taskCpu;
            }
            // Run GPU
            ComputeTask* taskGpu = createTask(TaskID::GPU_TREE_REDUCE, n);
            if(taskGpu) {
                taskGpu->init();
                taskGpu->dispatch(); // Run but ignore result
                taskGpu->cleanup();
                delete taskGpu;
            }
        }
        LOGI("--- WARMUP COMPLETE ---");

        // --- 3. TIMED RUNS ---
        LOGI("--- STARTING TIMED BENCHMARKS ---");

        std::vector<long long> cpuTimes;
        std::vector<long long> gpuTimes;

        // --- Run CPU Tests ---
        for (uint32_t n : testSizes) {
            ComputeTask* task = createTask(TaskID::CPU_REDUCE, n);
            task->init();
            cpuTimes.push_back(task->dispatch()); // Store result
            task->cleanup();
            delete task;
        }

        // --- Run GPU Tests ---
        for (uint32_t n : testSizes) {
            ComputeTask* task = createTask(TaskID::GPU_TREE_REDUCE, n);
            task->init();
            gpuTimes.push_back(task->dispatch()); // Store result
            task->cleanup();
            delete task;
        }

        // --- 4. FORMAT AND LOG FINAL TABLE ---
        std::stringstream ss;
        ss << "\n\n--- FINAL BENCHMARK RESULTS ---\n";
        ss << "N (Elements),CPU_Time_us,GPU_Time_us\n";
        for (size_t i = 0; i < testSizes.size(); ++i) {
            ss << testSizes[i] << "," << cpuTimes[i] << "," << gpuTimes[i] << "\n";
        }
        ss << "--- END OF RESULTS ---\n\n";

        // Log the entire table in one go
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
#include <jni.h>
#include <string>
#include <stdexcept>
#include <android/asset_manager_jni.h>
#include <vector>
#include <sstream> // For logging the final table

// --- Include all our tasks ---
#include "VulkanContext.h"
#include "ComputeTask.h"
#include "VectorAddTask.h"        // (For factory)
#include "LocalReduceTask.h"      // (For factory)
#include "CpuReduceTask.h"
//#include "GpuTreeReduceTask.h"    // (For factory)
#include "GpuOptimizedReduceTask.h"

// --- Global Pointers ---
VulkanContext* g_context = nullptr;
AAssetManager* g_assetManager = nullptr;


// --- Task Factory ---
enum class TaskID {
    VECTOR_ADD,
    LOCAL_REDUCE,
    CPU_REDUCE,
    GPU_TREE_REDUCE,
    GPU_OPTIMIZED_REDUCE
};

// This factory can now create any task we've built
ComputeTask* createTask(TaskID id, uint32_t n) {
    switch (id) {
        case TaskID::CPU_REDUCE:
            return new CpuReduceTask(n);

//        case TaskID::GPU_TREE_REDUCE:
//            if (g_assetManager == nullptr) {
//                LOGE("AssetManager is null, cannot create GpuTask");
//                return nullptr;
//            }
//            return new GpuTreeReduceTask(g_assetManager, n);

        case TaskID::GPU_OPTIMIZED_REDUCE:
            if (g_assetManager == nullptr) {
                LOGE("AssetManager is null, cannot create GpuTask");
                return nullptr;
            }
            return new GpuOptimizedReduceTask(g_assetManager, n);

            // --- These are not used in this experiment, but the factory can build them ---
        case TaskID::VECTOR_ADD:
        case TaskID::LOCAL_REDUCE:
        default:
            return nullptr;
    }
}

// --- JNI Function: Called from onCreate to pass AssetManager ---
extern "C" JNIEXPORT void JNICALL
Java_com_example_gpucomputetest_MainActivity_initJNI(
        JNIEnv* env,
        jobject /* this */,
        jobject assetManager) {

    LOGI("--- initJNI(): Storing AssetManager ---");
    g_assetManager = AAssetManager_fromJava(env, assetManager);
    if (g_assetManager == nullptr) {
        LOGE("Failed to get AAssetManager");
    }
}


// --- JNI Entry Point: Runs our experiment ---
extern "C" JNIEXPORT jstring JNICALL
Java_com_example_gpucomputetest_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {

    std::string resultMessage = "Optimized benchmark finished. Check Logcat.";

    // The full list of 10 sizes
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
            // Warm up CPU
            ComputeTask* taskCpu = createTask(TaskID::CPU_REDUCE, n);
            if(taskCpu) {
                taskCpu->init();
                taskCpu->dispatch(); // Run but ignore result
                taskCpu->cleanup();
                delete taskCpu;
            }
            // Warm up GPU
            ComputeTask* taskGpu = createTask(TaskID::GPU_OPTIMIZED_REDUCE, n);
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

        // --- Run GPU (OPTIMIZED) Tests ---
        for (uint32_t n : testSizes) {
            ComputeTask* task = createTask(TaskID::GPU_OPTIMIZED_REDUCE, n);
            task->init();
            gpuTimes.push_back(task->dispatch()); // Store result
            task->cleanup();
            delete task;
        }

        // --- 4. FORMAT AND LOG FINAL TABLE ---
        std::stringstream ss;
        ss << "\n\n--- FINAL BENCHMARK RESULTS (CPU vs. GPU Optimized) ---\n";
        ss << "N (Elements),CPU_Time_us,GPU_Optimized_Time_us\n";
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
    if (g_context != nullptr) {
        g_context->cleanup();
    }
    LOGI("--- Cleanup complete ---");
}
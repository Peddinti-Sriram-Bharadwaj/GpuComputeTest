#include <jni.h>
#include <string>
#include <stdexcept>
#include <android/asset_manager_jni.h> // <-- NEW: Include for assets

// Include our architecture
#include "VulkanContext.h"
#include "ComputeTask.h"
#include "VectorAddTask.h"
#include "LocalReduceTask.h"

// --- Global Pointers ---
VulkanContext* g_context = nullptr;
ComputeTask* g_task = nullptr;
AAssetManager* g_assetManager = nullptr; // <-- NEW: Global asset manager


// --- Task Factory ---
enum class TaskID {
    VECTOR_ADD,
    LOCAL_REDUCE
};

ComputeTask* createTask(TaskID id) {
    if (g_assetManager == nullptr) {
        throw std::runtime_error("AssetManager is not initialized");
    }

    switch (id) {
        case TaskID::VECTOR_ADD:
            // Pass the asset manager to the task
            return new VectorAddTask(g_assetManager); // <-- MODIFIED

        case TaskID::LOCAL_REDUCE:
            return new LocalReduceTask(g_assetManager);
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
extern "C" JNIEXPORT jstring JNICALL
Java_com_example_gpucomputetest_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {

    std::string resultMessage;

    try {
        // 1. Initialize Vulkan Singleton
        LOGI("--- Initializing Vulkan Context ---");
        g_context = VulkanContext::getInstance();
        g_context->init();

        // 2. Create the task using our factory
        LOGI("--- Creating Compute Task ---");
        g_task = createTask(TaskID::LOCAL_REDUCE);
        if (g_task == nullptr) {
            throw std::runtime_error("Failed to create task");
        }

        // 3. Initialize the task (creates buffers, pipeline)
        LOGI("--- Initializing Compute Task ---");
        g_task->init();

        // 4. Run the task
        LOGI("--- Dispatching Compute Task ---");
        g_task->dispatch();

        // 5. Set success message
        resultMessage = "Vulkan Compute Task (Vector Add) Finished Successfully.";

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

    if (g_task != nullptr) {
        g_task->cleanup();
        delete g_task;
        g_task = nullptr;
    }

    if (g_context != nullptr) {
        g_context->cleanup();
    }

    LOGI("--- Cleanup complete ---");
}
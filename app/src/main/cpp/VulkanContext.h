#pragma once

#include <vulkan/vulkan.h>
#include <android/log.h>

// --- Logging Macros ---
#define LOG_TAG "GpuCompute"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class VulkanContext {
public:
    // --- Singleton Access ---
    static VulkanContext* getInstance();

    // --- Deleted copy/move ---
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

    // --- Public API ---
    void init();
    void cleanup();

    // --- Getters for Vulkan Handles ---
    VkDevice getDevice() { return m_device; }
    VkPhysicalDevice getPhysicalDevice() { return m_physicalDevice; }
    VkQueue getQueue() { return m_queue; }
    VkCommandPool getCommandPool() { return m_commandPool; }
    uint32_t getComputeQueueFamilyIndex() { return m_computeQueueFamilyIndex; }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    // --- Private Singleton Constructor ---
    VulkanContext();
    ~VulkanContext();

    // --- Member Variables (Consistent Naming) ---
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    uint32_t m_computeQueueFamilyIndex = -1;

    // --- Private Helpers ---
    void createInstance();
    void pickPhysicalDevice();
    void findComputeQueueFamily();
    void createLogicalDeviceAndQueue();
    void createCommandPool(); // <-- FIX: Renamed from createCommonPool

    static VulkanContext* s_instance;
};
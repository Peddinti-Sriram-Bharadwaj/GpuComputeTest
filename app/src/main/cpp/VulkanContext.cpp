#include "VulkanContext.h"
#include <vector>

// --- Singleton ---
VulkanContext* VulkanContext::s_instance = nullptr;

VulkanContext* VulkanContext::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new VulkanContext();
    }
    return s_instance;
}

VulkanContext::VulkanContext() {
    LOGI("VulkanContext created");
}

VulkanContext::~VulkanContext() {
    LOGI("VulkanContext destroyed");
}

// --- Public API ---

void VulkanContext::init() {
    LOGI("Initializing VulkanContext...");
    try {
        createInstance();
        pickPhysicalDevice();
        findComputeQueueFamily();
        createLogicalDeviceAndQueue();
        createCommandPool();
        LOGI("VulkanContext initialized successfully.");
    } catch (const std::exception& e) {
        LOGE("Vulkan init failed: %s", e.what());
    }
}

void VulkanContext::cleanup() {
    LOGI("Cleaning up VulkanContext...");
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    }
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }
}

// --- Helper: Find Memory Type ---
uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOGE("Failed to find suitable memory type!");
    throw std::runtime_error("Failed to find suitable memory type!");
}


// --- Private Init Helpers ---

void VulkanContext::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "GpuComputeTest";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // We don't need any extensions or validation layers for this simple compute app
    createInfo.enabledExtensionCount = 0;
    createInfo.ppEnabledExtensionNames = nullptr;
    createInfo.enabledLayerCount = 0;

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        LOGE("Failed to create Vulkan instance!");
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
    LOGI("Vulkan Instance created.");
}

void VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOGE("Failed to find GPUs with Vulkan support!");
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    m_physicalDevice = devices[0];

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
    LOGI("Using GPU: %s", deviceProperties.deviceName);

    // --- ADD THIS BLOCK ---
    // Check for timestamp support
    if (deviceProperties.limits.timestampComputeAndGraphics) {
        // This is the number of nanoseconds per 'tick'
        m_timestampPeriod = deviceProperties.limits.timestampPeriod;
        LOGI("GPU timestamp support found. Period: %f ns/tick", m_timestampPeriod);
    } else {
        LOGW("GPU timestamp support NOT found. Profiling will be 0.");
        m_timestampPeriod = 0.0f;
    }
    // --- END OF BLOCK ---
}

void VulkanContext::findComputeQueueFamily() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        // Find a queue family that *only* supports compute, for best performance
        if ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            m_computeQueueFamilyIndex = i;
            LOGI("Found dedicated compute queue at index %d", i);
            return;
        }
    }

    // If not found, fall back to any queue that supports compute
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            m_computeQueueFamilyIndex = i;
            LOGI("Found general compute queue at index %d", i);
            return;
        }
    }

    if (m_computeQueueFamilyIndex == -1) {
        LOGE("Failed to find a compute queue family!");
        throw std::runtime_error("Failed to find a compute queue family!");
    }
}

void VulkanContext::createLogicalDeviceAndQueue() {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_computeQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;

    // We don't need any special device features for this project
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    if (vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device) != VK_SUCCESS) {
        LOGE("Failed to create logical device!");
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(m_device, m_computeQueueFamilyIndex, 0, &m_queue);
    LOGI("Logical device and queue created.");
}

void VulkanContext::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_computeQueueFamilyIndex;
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT lets us reset/rerecord command buffers
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        LOGE("Failed to create command pool!");
        throw std::runtime_error("Failed to create command pool!");
    }
    LOGI("Command pool created.");
}
#include "BaseComputeTask.h"
#include <stdexcept>

// --- Includes for assets ---
#include <android/asset_manager.h>

// --- Constructor ---
BaseComputeTask::BaseComputeTask(AAssetManager* assetManager) {
    m_context = VulkanContext::getInstance();
    m_assetManager = assetManager; // Store the asset manager
    if (m_assetManager == nullptr) {
        throw std::runtime_error("AAssetManager is null in BaseComputeTask");
    }
}

BaseComputeTask::~BaseComputeTask() {
}

// --- Template Method Implementation ---

void BaseComputeTask::init() {
    LOGI("BaseComputeTask::init() starting...");

    createBuffers();
    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSet();

    // --- Load pre-compiled shader ---
    std::string shaderPath = getShaderPath();
    if (shaderPath.empty()) {
        throw std::runtime_error("Shader path not provided by subclass");
    }
    VkShaderModule shaderModule = loadShaderModule(shaderPath);
    // ---

    // Create Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkCreatePipelineLayout(m_context->getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // Create Compute Pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main"; // Entry point

    if (vkCreateComputePipelines(m_context->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline!");
    }

    // Shader module can be destroyed after pipeline creation
    vkDestroyShaderModule(m_context->getDevice(), shaderModule, nullptr);
    LOGI("BaseComputeTask::init() finished.");
}

void BaseComputeTask::cleanup() {
    LOGI("BaseComputeTask::cleanup() starting...");

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_context->getDevice(), m_pipeline, nullptr);
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_context->getDevice(), m_pipelineLayout, nullptr);
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context->getDevice(), m_descriptorPool, nullptr);
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_context->getDevice(), m_descriptorSetLayout, nullptr);
    }
    LOGI("BaseComputeTask::cleanup() finished.");
}


// --- Load Pre-compiled SPIR-V Shader ---
VkShaderModule BaseComputeTask::loadShaderModule(const std::string& shaderPath) {
    LOGI("Loading pre-compiled shader: %s", shaderPath.c_str());

    // 1. Read SPIR-V file from assets
    AAsset* file = AAssetManager_open(m_assetManager, shaderPath.c_str(), AASSET_MODE_BUFFER);
    if (file == nullptr) {
        LOGE("Failed to open shader asset: %s", shaderPath.c_str());
        throw std::runtime_error("Failed to open shader asset");
    }

    size_t fileSize = AAsset_getLength(file);
    const char* fileContent = (const char*)AAsset_getBuffer(file);
    if (fileContent == nullptr) {
        AAsset_close(file);
        throw std::runtime_error("Failed to read shader asset buffer");
    }

    // 2. Copy SPIR-V data (it's already compiled binary)
    std::vector<char> spirvCode(fileContent, fileContent + fileSize);
    AAsset_close(file);

    // 3. Validate that size is a multiple of 4 (SPIR-V requirement)
    if (spirvCode.size() % 4 != 0) {
        LOGE("Shader file size is not a multiple of 4 bytes: %zu", spirvCode.size());
        throw std::runtime_error("Invalid SPIR-V file size");
    }

    // 4. Create VkShaderModule
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(spirvCode.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_context->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    LOGI("Shader loaded successfully.");
    return shaderModule;
}


void BaseComputeTask::createBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize size,
                                   VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    VkDevice device = m_context->getDevice();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_context->findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkCommandBuffer BaseComputeTask::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_context->getCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_context->getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void BaseComputeTask::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue queue = m_context->getQueue();
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(m_context->getDevice(), m_context->getCommandPool(), 1, &commandBuffer);
}

void BaseComputeTask::addBufferBarrier(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                       VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                                       VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage,
                         0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void BaseComputeTask::createStagingBuffer(VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize size, const void* initialData) {
    VkDevice device = m_context->getDevice();

    // 1. Create a temporary staging buffer (CPU-visible)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    BaseComputeTask::createBuffer(stagingBuffer, stagingBufferMemory, size,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // 2. Map it and copy the initial data into it
    void* mappedData;
    vkMapMemory(device, stagingBufferMemory, 0, size, 0, &mappedData);
    memcpy(mappedData, initialData, (size_t)size);
    vkUnmapMemory(device, stagingBufferMemory);

    // 3. Create the final destination buffer (GPU-only)
    BaseComputeTask::createBuffer(buffer, memory, size,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // 4. Use a one-time command buffer to copy from staging to device buffer
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);

    // 5. Clean up temporary staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
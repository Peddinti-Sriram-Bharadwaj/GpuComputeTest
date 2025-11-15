#include "VectorAddTask.h"
#include <vector>
#include <stdexcept>

// --- MODIFIED: Constructor calls base constructor ---
VectorAddTask::VectorAddTask(AAssetManager* assetManager)
        : BaseComputeTask(assetManager) {
    LOGI("VectorAddTask created");
}

VectorAddTask::~VectorAddTask() {
    LOGI("VectorAddTask destroyed");
}

void VectorAddTask::cleanup() {
    LOGI("VectorAddTask::cleanup()");
    cleanupBuffers();
    BaseComputeTask::cleanup();
}

void VectorAddTask::cleanupBuffers() {
    // ... (this function is unchanged)
    VkDevice device = m_context->getDevice();

    if (m_bufferA != VK_NULL_HANDLE) vkDestroyBuffer(device, m_bufferA, nullptr);
    if (m_bufferB != VK_NULL_HANDLE) vkDestroyBuffer(device, m_bufferB, nullptr);
    if (m_bufferC != VK_NULL_HANDLE) vkDestroyBuffer(device, m_bufferC, nullptr);

    if (m_bufferMemoryA != VK_NULL_HANDLE) vkFreeMemory(device, m_bufferMemoryA, nullptr);
    if (m_bufferMemoryB != VK_NULL_HANDLE) vkFreeMemory(device, m_bufferMemoryB, nullptr);
    if (m_bufferMemoryC != VK_NULL_HANDLE) vkFreeMemory(device, m_bufferMemoryC, nullptr);
}

// --- "Fill-in-the-blank" Implementations ---

// --- MODIFIED: Just return the path from assets ---
std::string VectorAddTask::getShaderPath() {
    // This path is relative to app/src/main/assets/
    return "shaders/vector_add.spv";
}
// ---

void VectorAddTask::createDescriptorSetLayout() {
    // ... (this function is unchanged)
    std::vector<VkDescriptorSetLayoutBinding> bindings(3);

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_context->getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}

void VectorAddTask::createBuffers() {
    // ... (this function is unchanged)
    VkDeviceSize bufferSize = sizeof(float) * NUM_ELEMENTS;

    std::vector<float> dataA(NUM_ELEMENTS);
    std::vector<float> dataB(NUM_ELEMENTS);
    for (uint32_t i = 0; i < NUM_ELEMENTS; i++) {
        dataA[i] = (float)i;
        dataB[i] = (float)i * 2.0f;
    }

    createDeviceLocalBuffer(m_bufferA, m_bufferMemoryA, bufferSize, dataA.data());
    createDeviceLocalBuffer(m_bufferB, m_bufferMemoryB, bufferSize, dataB.data());

    BaseComputeTask::createBuffer(m_bufferC, m_bufferMemoryC, bufferSize,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void VectorAddTask::createDescriptorPool() {
    // ... (this function is unchanged)
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 3;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(m_context->getDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void VectorAddTask::createDescriptorSet() {
    // ... (this function is unchanged)
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkAllocateDescriptorSets(m_context->getDevice(), &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set!");
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites(3);

    VkDescriptorBufferInfo bufferInfoA{};
    bufferInfoA.buffer = m_bufferA;
    bufferInfoA.offset = 0;
    bufferInfoA.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo bufferInfoB{};
    bufferInfoB.buffer = m_bufferB;
    bufferInfoB.offset = 0;
    bufferInfoB.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo bufferInfoC{};
    bufferInfoC.buffer = m_bufferC;
    bufferInfoC.offset = 0;
    bufferInfoC.range = VK_WHOLE_SIZE;

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = m_descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfoA;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &bufferInfoB;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = m_descriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &bufferInfoC;

    vkUpdateDescriptorSets(m_context->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}


// --- The Core Dispatch Logic ---
long long VectorAddTask::dispatch() {
    // ... (this function is unchanged)
    VkDevice device = m_context->getDevice();
    VkCommandPool commandPool = m_context->getCommandPool();
    VkQueue queue = m_context->getQueue();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VkDeviceSize bufferSize = sizeof(float) * NUM_ELEMENTS;

    BaseComputeTask::createBuffer(stagingBuffer, stagingBufferMemory, bufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

    uint32_t workgroupCount = (NUM_ELEMENTS + 255) / 256;
    vkCmdDispatch(commandBuffer, workgroupCount, 1, 1);

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = m_bufferC;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 1, &barrier, 0, nullptr);

    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, m_bufferC, stagingBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer!");
    }

    vkQueueWaitIdle(queue);

    void* mappedData;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &mappedData);

    float* results = (float*)mappedData;

    bool success = true;
    for (int i = 0; i < 5; i++) {
        float expected = (float)i + ((float)i * 2.0f);
        LOGI("Result[%d]: %.2f (Expected: %.2f)", i, results[i], expected);
        if (abs(results[i] - expected) > 0.01f) {
            success = false;
        }
    }

    if (success) {
        LOGI("--- VECTOR ADD SUCCESS ---");
    } else {
        LOGE("--- VECTOR ADD FAILED ---");
    }

    vkUnmapMemory(device, stagingBufferMemory);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    return 0;
}


// --- Private Helper for Data Upload ---
void VectorAddTask::createDeviceLocalBuffer(VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize size, const void* initialData) {
    // ... (this function is unchanged)
    VkDevice device = m_context->getDevice();
    VkCommandPool commandPool = m_context->getCommandPool();
    VkQueue queue = m_context->getQueue();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    BaseComputeTask::createBuffer(stagingBuffer, stagingBufferMemory, size,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* mappedData;
    vkMapMemory(device, stagingBufferMemory, 0, size, 0, &mappedData);
    memcpy(mappedData, initialData, (size_t)size);
    vkUnmapMemory(device, stagingBufferMemory);

    BaseComputeTask::createBuffer(buffer, memory, size,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
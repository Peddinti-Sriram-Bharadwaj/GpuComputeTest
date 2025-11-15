#include "LocalReduceTask.h"
#include <vector>
#include <stdexcept>
#include <numeric> // For std::iota

LocalReduceTask::LocalReduceTask(AAssetManager* assetManager)
        : BaseComputeTask(assetManager) {
    LOGI("LocalReduceTask created");
}

LocalReduceTask::~LocalReduceTask() {
    LOGI("LocalReduceTask destroyed");
}

void LocalReduceTask::cleanup() {
    LOGI("LocalReduceTask::cleanup()");
    cleanupBuffers();
    BaseComputeTask::cleanup();
}

void LocalReduceTask::cleanupBuffers() {
    VkDevice device = m_context->getDevice();
    if (m_bufferIn != VK_NULL_HANDLE) vkDestroyBuffer(device, m_bufferIn, nullptr);
    if (m_bufferOut != VK_NULL_HANDLE) vkDestroyBuffer(device, m_bufferOut, nullptr);
    if (m_bufferMemoryIn != VK_NULL_HANDLE) vkFreeMemory(device, m_bufferMemoryIn, nullptr);
    if (m_bufferMemoryOut != VK_NULL_HANDLE) vkFreeMemory(device, m_bufferMemoryOut, nullptr);
}

// --- "Fill-in-the-blank" Implementations ---

std::string LocalReduceTask::getShaderPath() {
    // Load our new, manually-compiled shader
    return "shaders/local_reduce.spv";
}

void LocalReduceTask::createDescriptorSetLayout() {
    // This is simpler: only 2 buffers
    std::vector<VkDescriptorSetLayoutBinding> bindings(2);

    // Binding 0: Input Buffer (read-only)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Output Buffer (write-only)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_context->getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}

void LocalReduceTask::createBuffers() {
    // --- 1. Create Input Buffer ---
    VkDeviceSize inBufferSize = sizeof(float) * NUM_ELEMENTS;
    std::vector<float> inData(NUM_ELEMENTS);

    // Fill with 1.0, 2.0, 3.0, ... 256.0
    std::iota(inData.begin(), inData.end(), 1.0f);

    // Helper to create a staging buffer, copy, and create device-local
    // (This is a simplified version of your VectorAddTask helper)
    createStagingBuffer(m_bufferIn, m_bufferMemoryIn, inBufferSize, inData.data());


    // --- 2. Create Output Buffer ---
    // It only needs to hold a single float
    VkDeviceSize outBufferSize = sizeof(float);
    BaseComputeTask::createBuffer(m_bufferOut, m_bufferMemoryOut, outBufferSize,
            // Needs to be a source for copying back to CPU
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void LocalReduceTask::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 2; // 2 buffers

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(m_context->getDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void LocalReduceTask::createDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkAllocateDescriptorSets(m_context->getDevice(), &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set!");
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites(2);

    // Need to define these outside the writes
    VkDescriptorBufferInfo bufferInfoIn{};
    bufferInfoIn.buffer = m_bufferIn;
    bufferInfoIn.offset = 0;
    bufferInfoIn.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo bufferInfoOut{};
    bufferInfoOut.buffer = m_bufferOut;
    bufferInfoOut.offset = 0;
    bufferInfoOut.range = VK_WHOLE_SIZE;

    // Write for Buffer In (Binding 0)
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = m_descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfoIn;

    // Write for Buffer Out (Binding 1)
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &bufferInfoOut;

    vkUpdateDescriptorSets(m_context->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}


void LocalReduceTask::dispatch() {
    VkDevice device = m_context->getDevice();
    VkCommandPool commandPool = m_context->getCommandPool();
    VkQueue queue = m_context->getQueue();

    // --- 1. Create Staging Buffer (for readback) ---
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VkDeviceSize bufferSize = sizeof(float); // Only one float!

    BaseComputeTask::createBuffer(stagingBuffer, stagingBufferMemory, bufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // --- 2. Allocate and Record Command Buffer ---
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    // --- Record commands ---
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

    // Launch just ONE workgroup
    vkCmdDispatch(commandBuffer, 1, 1, 1);

    // --- 3. Add Barrier and Copy Result ---
    addBufferBarrier(commandBuffer, m_bufferOut,
                     VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Copy the single float result
    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, m_bufferOut, stagingBuffer, 1, &copyRegion);

    // --- 4. End Recording and Submit ---
    endSingleTimeCommands(commandBuffer);

    // --- 5. Read Back and Verify ---
    void* mappedData;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &mappedData);

    float result = *(float*)mappedData;

    // The sum of 1 to 256 is (n * (n+1)) / 2
    // (256 * 257) / 2 = 32896
    float expected = 32896.0f;

    LOGI("Local Reduce Result: %.0f", result);
    LOGI("Expected Result:     %.0f", expected);

    if (abs(result - expected) < 0.01f) {
        LOGI("--- LOCAL REDUCE SUCCESS ---");
    } else {
        LOGE("--- LOCAL REDUCE FAILED ---");
    }

    vkUnmapMemory(device, stagingBufferMemory);

    // --- 6. Cleanup ---
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
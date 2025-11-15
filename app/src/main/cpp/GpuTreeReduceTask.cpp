#include "GpuTreeReduceTask.h"
#include <vector>
#include <stdexcept>
#include <numeric>
#include <cmath>

GpuTreeReduceTask::GpuTreeReduceTask(AAssetManager* assetManager, uint32_t n)
        : BaseComputeTask(assetManager), m_n(n) {
    LOGI("GpuTreeReduceTask created. N=%u", m_n);
    // Get the timestamp period from the context
    m_gpuTimestampPeriod = m_context->getTimeStampPeriod();
}

GpuTreeReduceTask::~GpuTreeReduceTask() {
    LOGI("GpuTreeReduceTask destroyed");
}

void GpuTreeReduceTask::cleanup() {
    LOGI("GpuTreeReduceTask::cleanup()");
    cleanupBuffers();

    // --- NEW: Clean up the query pool ---
    if (m_queryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(m_context->getDevice(), m_queryPool, nullptr);
    }
    // ---

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_context->getDevice(), m_descriptorPool, 1, &m_descriptorSetA_to_B);
        vkFreeDescriptorSets(m_context->getDevice(), m_descriptorPool, 1, &m_descriptorSetB_to_A);
    }

    BaseComputeTask::cleanup();
}

void GpuTreeReduceTask::cleanupBuffers() {
    // ... (this function is unchanged)
    VkDevice device = m_context->getDevice();
    if (m_bufferA != VK_NULL_HANDLE) vkDestroyBuffer(device, m_bufferA, nullptr);
    if (m_bufferB != VK_NULL_HANDLE) vkDestroyBuffer(device, m_bufferB, nullptr);
    if (m_bufferMemoryA != VK_NULL_HANDLE) vkFreeMemory(device, m_bufferMemoryA, nullptr);
    if (m_bufferMemoryB != VK_NULL_HANDLE) vkFreeMemory(device, m_bufferMemoryB, nullptr);
}

// --- Overridden init() ---
void GpuTreeReduceTask::init() {
    LOGI("GpuTreeReduceTask::init() starting...");

    // 1. Run most of the BaseComputeTask's init steps manually
    createBuffers();
    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSet();

    // 2. Compile the shader (using our manual .spv loader)
    std::string shaderPath = getShaderPath();
    if (shaderPath.empty()) {
        throw std::runtime_error("Shader path not provided by subclass");
    }
    VkShaderModule shaderModule = loadShaderModule(shaderPath);

    // 3. Create Pipeline Layout (with Push Constants)
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushData);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_context->getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout with push constants!");
    }

    // 4. Create Compute Pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";

    if (vkCreateComputePipelines(m_context->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline!");
    }

    vkDestroyShaderModule(m_context->getDevice(), shaderModule, nullptr);

    // --- 5. NEW: Create the Query Pool ---
    if (m_gpuTimestampPeriod > 0) { // Only if timestamps are supported
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = 2; // One for start, one for end

        if (vkCreateQueryPool(m_context->getDevice(), &queryPoolInfo, nullptr, &m_queryPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create query pool!");
        }
        LOGI("Query pool created for profiling.");
    }
    // ---

    LOGI("GpuTreeReduceTask::init() finished.");
}


// --- "Fill-in-the-blank" Implementations ---
// (These are all unchanged from Phase 4)

std::string GpuTreeReduceTask::getShaderPath() {
    return "shaders/tree_reduce.spv";
}

void GpuTreeReduceTask::createDescriptorSetLayout() {
    // ... (unchanged)
    std::vector<VkDescriptorSetLayoutBinding> bindings(2);
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
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

long long GpuTreeReduceTask::dispatch() {
    VkDevice device = m_context->getDevice();
    // VkQueue queue = m_context->getQueue(); // Unused variable, removed

    // --- 1. Get CPU-side timer ---
    auto startTime = std::chrono::high_resolution_clock::now();

    // --- 2. Allocate Command Buffer ---
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    // --- 3. Reset Query Pool ---
    if (m_queryPool != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(commandBuffer, m_queryPool, 0, 2);
    }
    // ---

    // Bind the pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    // --- 4. Write START Timestamp ---
    if (m_queryPool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPool, 0);
    }
    // ---

    PushData pushData{};

    // --- 5. Pass 1: Local Reduce ---
    pushData.passType = 0;
    pushData.numElements = m_n;
    vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushData), &pushData);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSetA_to_B, 0, nullptr);
    uint32_t numWorkgroups = m_n / WORKGROUP_SIZE;
    vkCmdDispatch(commandBuffer, numWorkgroups, 1, 1);

    // --- 6. Pass 2...N: Tree Reduce Loop ---
    uint32_t elementsToProcess = numWorkgroups;
    bool readFromB_writeToA = true;

    while (elementsToProcess > 1) {
        // *** CRITICAL BARRIER ***
        addBufferBarrier(commandBuffer,
                         readFromB_writeToA ? m_bufferB : m_bufferA,
                         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // --- Configure this pass ---
        pushData.passType = 1;
        pushData.numElements = elementsToProcess;
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushData), &pushData);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1,
                                readFromB_writeToA ? &m_descriptorSetB_to_A : &m_descriptorSetA_to_B,
                                0, nullptr);

        numWorkgroups = (uint32_t)std::ceil((float)(elementsToProcess / 2.0f) / (float)WORKGROUP_SIZE);
        vkCmdDispatch(commandBuffer, numWorkgroups, 1, 1);

        elementsToProcess = (uint32_t)std::ceil((float)elementsToProcess / 2.0f);
        readFromB_writeToA = !readFromB_writeToA;
    }

    // --- 7. Read Back Result ---
    VkBuffer finalBuffer = readFromB_writeToA ? m_bufferB : m_bufferA; // The fix
    VkDeviceMemory finalMemory = readFromB_writeToA ? m_bufferMemoryB : m_bufferMemoryA;

    // *** FINAL BARRIER ***
    // Wait for shader writes to be visible to the HOST (CPU)
    addBufferBarrier(commandBuffer, finalBuffer,
                     VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

    // --- 8. Write END Timestamp ---
    if (m_queryPool != VK_NULL_HANDLE) {
        // We record this *after* the barrier, so it includes all compute work.
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, 1);
    }
    // ---

    // --- 9. NO MORE STAGING BUFFER ---
    // We don't need vkCmdCopyBuffer or a staging buffer
    // ...

    // --- 10. End Recording and Submit ---
    endSingleTimeCommands(commandBuffer); // This includes vkQueueWaitIdle()

    // --- 11. Get CPU-side timer ---
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // --- 12. Get GPU Timestamp Results (with error checking) ---
    if (m_queryPool != VK_NULL_HANDLE) {
        // We need 2 results (start, end), 64-bits each
        uint64_t timestamps[2] = {0, 0};

        // This copies the results from the GPU pool to our CPU variable
        VkResult result = vkGetQueryPoolResults(device, m_queryPool, 0, 2,
                                                sizeof(timestamps), timestamps, sizeof(uint64_t),
                                                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

        if (result == VK_SUCCESS) {
            // Calculate the time in nanoseconds
            uint64_t startTimeNs = timestamps[0] * m_gpuTimestampPeriod;
            uint64_t endTimeNs = timestamps[1] * m_gpuTimestampPeriod;
            uint64_t gpuDurationNs = endTimeNs - startTimeNs;

            // Convert to microseconds
            double gpuDurationMicroseconds = (double)gpuDurationNs / 1000.0;

            LOGI("--- GPU PROFILING ---");
            LOGI("GPU-Only Execution Time: %.3f microseconds", gpuDurationMicroseconds);

        } else if (result == VK_NOT_READY) {
            LOGW("--- GPU PROFILING FAILED ---");
            LOGW("vkGetQueryPoolResults returned VK_NOT_READY. Results not available.");
        } else {
            LOGW("--- GPU PROFILING FAILED ---");
            LOGW("vkGetQueryPoolResults failed with error code: %d", result);
        }
    }
    // ---

    // --- 13. Verify (Read directly from the final buffer) ---
    void* mappedData;
    vkMapMemory(device, finalMemory, 0, sizeof(float), 0, &mappedData);

    float result = *(float*)mappedData;
    float expected = (float)m_n;

    LOGI("--- VERIFICATION (N=%u) ---", m_n);
    LOGI("Result: %.0f (Expected: %.0f)", result, expected);

    if(abs(result - expected) < 0.01f) {
        LOGI("SUCCESS");
    } else {
        LOGE("FAILED");
    }

    vkUnmapMemory(device, finalMemory);

    // --- 14. Cleanup (No staging buffer) ---
    LOGI("CPU-side timer (incl. stall): %lld microseconds", (long long)duration.count());

    return duration.count();
}

void GpuTreeReduceTask::createDescriptorPool() {
    // ... (unchanged)
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 4;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 2;
    if (vkCreateDescriptorPool(m_context->getDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void GpuTreeReduceTask::createDescriptorSet() {
    // ... (unchanged)
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    if (vkAllocateDescriptorSets(m_context->getDevice(), &allocInfo, &m_descriptorSetA_to_B) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set A->B!");
    }
    if (vkAllocateDescriptorSets(m_context->getDevice(), &allocInfo, &m_descriptorSetB_to_A) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set B->A!");
    }
    VkDescriptorBufferInfo bufferInfoA_in{};
    bufferInfoA_in.buffer = m_bufferA;
    bufferInfoA_in.offset = 0;
    bufferInfoA_in.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo bufferInfoB_out{};
    bufferInfoB_out.buffer = m_bufferB;
    bufferInfoB_out.offset = 0;
    bufferInfoB_out.range = VK_WHOLE_SIZE;
    std::vector<VkWriteDescriptorSet> writesA_B(2);
    writesA_B[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writesA_B[0].dstSet = m_descriptorSetA_to_B;
    writesA_B[0].dstBinding = 0;
    writesA_B[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writesA_B[0].descriptorCount = 1;
    writesA_B[0].pBufferInfo = &bufferInfoA_in;
    writesA_B[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writesA_B[1].dstSet = m_descriptorSetA_to_B;
    writesA_B[1].dstBinding = 1;
    writesA_B[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writesA_B[1].descriptorCount = 1;
    writesA_B[1].pBufferInfo = &bufferInfoB_out;
    vkUpdateDescriptorSets(m_context->getDevice(), 2, writesA_B.data(), 0, nullptr);
    VkDescriptorBufferInfo bufferInfoB_in{};
    bufferInfoB_in.buffer = m_bufferB;
    bufferInfoB_in.offset = 0;
    bufferInfoB_in.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo bufferInfoA_out{};
    bufferInfoA_out.buffer = m_bufferA;
    bufferInfoA_out.offset = 0;
    bufferInfoA_out.range = VK_WHOLE_SIZE;
    std::vector<VkWriteDescriptorSet> writesB_A(2);
    writesB_A[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writesB_A[0].dstSet = m_descriptorSetB_to_A;
    writesB_A[0].dstBinding = 0;
    writesB_A[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writesB_A[0].descriptorCount = 1;
    writesB_A[0].pBufferInfo = &bufferInfoB_in;
    writesB_A[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writesB_A[1].dstSet = m_descriptorSetB_to_A;
    writesB_A[1].dstBinding = 1;
    writesB_A[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writesB_A[1].descriptorCount = 1;
    writesB_A[1].pBufferInfo = &bufferInfoA_out;
    vkUpdateDescriptorSets(m_context->getDevice(), 2, writesB_A.data(), 0, nullptr);
}

void GpuTreeReduceTask::createBuffers() {
    VkDevice device = m_context->getDevice();
    VkDeviceSize dataSize = sizeof(float) * m_n;

    // Define the unified memory properties for a mobile GPU
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // --- 1. Create Buffer A (Input / Ping-Pong) ---
    BaseComputeTask::createBuffer(m_bufferA, m_bufferMemoryA, dataSize,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // Needs to be storage
                                  properties);

    // --- 2. Fill Buffer A directly (no staging buffer!) ---
    void* mappedData;
    vkMapMemory(device, m_bufferMemoryA, 0, dataSize, 0, &mappedData);

    float* dataPtr = (float*)mappedData;
    for(size_t i = 0; i < m_n; i++) {
        dataPtr[i] = 1.0f; // Fill with 1.0f
    }

    vkUnmapMemory(device, m_bufferMemoryA);

    // --- 3. Create Buffer B (Intermediate / Ping-Pong) ---
    // Size is based on the number of workgroups from pass 1
    VkDeviceSize intermediateSize = sizeof(float) * (m_n / WORKGROUP_SIZE);

    BaseComputeTask::createBuffer(m_bufferB, m_bufferMemoryB, intermediateSize,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // Storage + readback
                                  properties);
}

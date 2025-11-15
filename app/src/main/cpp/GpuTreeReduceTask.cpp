#include "GpuTreeReduceTask.h"
#include <vector>
#include <stdexcept>
#include <numeric> // For std::iota
#include <cmath>   // For ceil()

GpuTreeReduceTask::GpuTreeReduceTask(AAssetManager* assetManager)
        : BaseComputeTask(assetManager) {
    LOGI("GpuTreeReduceTask created");
}

GpuTreeReduceTask::~GpuTreeReduceTask() {
    LOGI("GpuTreeReduceTask destroyed");
}

void GpuTreeReduceTask::cleanup() {
    LOGI("GpuTreeReduceTask::cleanup()");
    cleanupBuffers();

    // We must clean up the extra descriptor set
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_context->getDevice(), m_descriptorPool, 1, &m_descriptorSetA_to_B);
        vkFreeDescriptorSets(m_context->getDevice(), m_descriptorPool, 1, &m_descriptorSetB_to_A);
    }

    // Call base class cleanup
    BaseComputeTask::cleanup();
}

void GpuTreeReduceTask::cleanupBuffers() {
    VkDevice device = m_context->getDevice();
    if (m_bufferA != VK_NULL_HANDLE) vkDestroyBuffer(device, m_bufferA, nullptr);
    if (m_bufferB != VK_NULL_HANDLE) vkDestroyBuffer(device, m_bufferB, nullptr);
    if (m_bufferMemoryA != VK_NULL_HANDLE) vkFreeMemory(device, m_bufferMemoryA, nullptr);
    if (m_bufferMemoryB != VK_NULL_HANDLE) vkFreeMemory(device, m_bufferMemoryB, nullptr);
}

// --- Overridden init() ---
// We override init() to create a custom VkPipelineLayout with Push Constants
void GpuTreeReduceTask::init() {
    LOGI("GpuTreeReduceTask::init() starting...");

    // 1. Run most of the BaseComputeTask's init steps manually
    createBuffers();
    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSet(); // This creates both A->B and B->A sets

    // 2. Compile the shader
    std::string shaderPath = getShaderPath();
    if (shaderPath.empty()) {
        throw std::runtime_error("Shader path not provided by subclass");
    }
    VkShaderModule shaderModule = loadShaderModule(shaderPath);

    // 3. *** Create Pipeline Layout (CUSTOM PART) ***
    // We need to define the "push constant" range
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushData); // Our custom struct

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1; // <-- Tell Vulkan we're using push constants
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_context->getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout with push constants!");
    }

    // 4. Create Compute Pipeline (same as base class)
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
    LOGI("GpuTreeReduceTask::init() finished.");
}


// --- "Fill-in-the-blank" Implementations ---

std::string GpuTreeReduceTask::getShaderPath() {
    return "shaders/tree_reduce.spv";
}

void GpuTreeReduceTask::createDescriptorSetLayout() {
    // Layout is the same for A->B and B->A
    // Binding 0: Input
    // Binding 1: Output
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

void GpuTreeReduceTask::createBuffers() {
    // We need 1 buffer for input, and 1 for intermediate results
    // Buffer A: Holds the initial 1M elements
    // Buffer B: Holds the partial sums (4096, then 2048, ...)

    // --- 1. Create Input Data ---
    VkDeviceSize dataSize = sizeof(float) * NUM_ELEMENTS;
    std::vector<float> inData(NUM_ELEMENTS);
    std::fill(inData.begin(), inData.end(), 1.0f); // Fill with 1.0

    // --- 2. Create Buffer A ---
    // (Input + Ping-Pong)
    // Needs to be Transfer Dst (for initial copy) and Storage
    createStagingBuffer(m_bufferA, m_bufferMemoryA, dataSize, inData.data());

    // --- 3. Create Buffer B ---
    // (Output + Ping-Pong)
    // Needs to be Storage, Transfer Src (for final readback)
    // Size is dataSize because it needs to hold Pass 1's 4096 elements
    VkDeviceSize intermediateSize = sizeof(float) * (NUM_ELEMENTS / WORKGROUP_SIZE);

    BaseComputeTask::createBuffer(m_bufferB, m_bufferMemoryB, intermediateSize,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void GpuTreeReduceTask::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 4; // 2 buffers * 2 descriptor sets

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 2; // We need two sets!

    if (vkCreateDescriptorPool(m_context->getDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void GpuTreeReduceTask::createDescriptorSet() {
    // We create TWO sets, one for A->B, one for B->A
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    // 1. Create A -> B set
    if (vkAllocateDescriptorSets(m_context->getDevice(), &allocInfo, &m_descriptorSetA_to_B) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set A->B!");
    }
    // 2. Create B -> A set
    if (vkAllocateDescriptorSets(m_context->getDevice(), &allocInfo, &m_descriptorSetB_to_A) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set B->A!");
    }

    // --- Write Set 1: A (in) -> B (out) ---
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
    writesA_B[0].dstBinding = 0; // binding 0 = InBuffer
    writesA_B[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writesA_B[0].descriptorCount = 1;
    writesA_B[0].pBufferInfo = &bufferInfoA_in;

    writesA_B[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writesA_B[1].dstSet = m_descriptorSetA_to_B;
    writesA_B[1].dstBinding = 1; // binding 1 = OutBuffer
    writesA_B[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writesA_B[1].descriptorCount = 1;
    writesA_B[1].pBufferInfo = &bufferInfoB_out;

    vkUpdateDescriptorSets(m_context->getDevice(), 2, writesA_B.data(), 0, nullptr);

    // --- Write Set 2: B (in) -> A (out) ---
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
    writesB_A[0].dstBinding = 0; // binding 0 = InBuffer
    writesB_A[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writesB_A[0].descriptorCount = 1;
    writesB_A[0].pBufferInfo = &bufferInfoB_in;

    writesB_A[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writesB_A[1].dstSet = m_descriptorSetB_to_A;
    writesB_A[1].dstBinding = 1; // binding 1 = OutBuffer
    writesB_A[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writesB_A[1].descriptorCount = 1;
    writesB_A[1].pBufferInfo = &bufferInfoA_out;

    vkUpdateDescriptorSets(m_context->getDevice(), 2, writesB_A.data(), 0, nullptr);
}


// --- The Core Multi-Pass Dispatch Logic ---

void GpuTreeReduceTask::dispatch() {
    VkDevice device = m_context->getDevice();
    VkQueue queue = m_context->getQueue();

    // Start timer (we'll use timestamps for more accurate GPU time)
    auto startTime = std::chrono::high_resolution_clock::now();

    // --- 1. Allocate Command Buffer ---
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    // Bind the pipeline (we only do this once)
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    PushData pushData{};

    // --- 2. Pass 1: Local Reduce ---
    pushData.passType = 0;
    pushData.numElements = NUM_ELEMENTS;

    vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushData), &pushData);

    // Bind Set 1: Read from A, Write to B
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSetA_to_B, 0, nullptr);

    // Launch (1024*1024 / 256) = 4096 workgroups
    uint32_t numWorkgroups = NUM_ELEMENTS / WORKGROUP_SIZE;
    vkCmdDispatch(commandBuffer, numWorkgroups, 1, 1);

    // --- 3. Pass 2...N: Tree Reduce Loop ---

    // We start with 4096 elements in Buffer B
    uint32_t elementsToProcess = numWorkgroups;
    bool readFromB_writeToA = true;

    while (elementsToProcess > 1) {
        // *** CRITICAL BARRIER ***
        // Wait for the *previous* dispatch's shader writes to finish
        // before we start the *next* dispatch's shader reads.
        addBufferBarrier(commandBuffer,
                         readFromB_writeToA ? m_bufferB : m_bufferA, // Buffer just written to
                         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // --- Configure this pass ---
        pushData.passType = 1;
        pushData.numElements = elementsToProcess;

        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushData), &pushData);

        // Bind the correct descriptor set for ping-pong
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1,
                                readFromB_writeToA ? &m_descriptorSetB_to_A : &m_descriptorSetA_to_B,
                                0, nullptr);

        // We are processing 'elementsToProcess' items, 2 per thread.
        // Our shader only uses globalId, so we dispatch (N/2) threads.
        // With a workgroup size of 256, we dispatch (N/2 / 256) workgroups.
        numWorkgroups = (uint32_t)std::ceil((float)(elementsToProcess / 2.0f) / (float)WORKGROUP_SIZE);
        vkCmdDispatch(commandBuffer, numWorkgroups, 1, 1);

        // Update loop variables
        elementsToProcess = (uint32_t)std::ceil((float)elementsToProcess / 2.0f);
        readFromB_writeToA = !readFromB_writeToA;
    }

    // --- 4. Read Back Result ---

    // The final result (1 float) is in the *last buffer we wrote to*.
    // If readFromB_writeToA is true, we just wrote to A (Pass 2).
    // If readFromB_writeToA is false, we just wrote to B (Pass 1 or 3).
    VkBuffer finalBuffer = readFromB_writeToA ? m_bufferB : m_bufferA;

    // *** FINAL BARRIER ***
    // Wait for the last shader write to finish before we copy to staging
    addBufferBarrier(commandBuffer, finalBuffer,
                     VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Create staging buffer for 1 float
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VkDeviceSize bufferSize = sizeof(float);
    BaseComputeTask::createBuffer(stagingBuffer, stagingBufferMemory, bufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Copy the single float result
    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, finalBuffer, stagingBuffer, 1, &copyRegion);

    // --- 5. End Recording and Submit ---
    endSingleTimeCommands(commandBuffer);

    // Stop CPU timer (this includes stall time, not ideal)
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // --- 6. Verify ---
    void* mappedData;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &mappedData);

    float result = *(float*)mappedData;
    float expected = (float)NUM_ELEMENTS; // 1M * 1.0f

    LOGI("GPU Tree Reduce Result: %.0f", result);
    LOGI("Expected Result:        %.0f", expected);

    if (abs(result - expected) < 0.01f) {
        LOGI("--- GPU TREE REDUCE SUCCESS ---");
    } else {
        LOGE("--- GPU TREE REDUCE FAILED ---");
    }
    LOGI("GPU execution time (CPU-timed): %lld microseconds", (long long)duration.count());

    vkUnmapMemory(device, stagingBufferMemory);

    // --- 7. Cleanup ---
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
#pragma once

#include "BaseComputeTask.h"
#include <vector>

// This struct MUST match the layout in the shader
struct PushData {
    uint32_t passType;    // 0 = local, 1 = tree
    uint32_t numElements; // elements to process
};

class GpuTreeReduceTask : public BaseComputeTask {
public:
    GpuTreeReduceTask(AAssetManager* assetManager);
    ~GpuTreeReduceTask();

    // --- ComputeTask Interface ---

    // We override init to create our custom pipeline layout
    void init() override;

    void dispatch() override;
    void cleanup() override;

protected:
    // --- BaseComputeTask "Fill-in-the-blanks" ---
    std::string getShaderPath() override;
    void createDescriptorSetLayout() override;
    void createBuffers() override;
    void createDescriptorPool() override;
    void createDescriptorSet() override;

private:
    void cleanupBuffers();

    // --- Task-Specific Members ---

    // We need two buffers to "ping-pong" data between
    // Pass 1: A -> B
    // Pass 2: B -> A
    // Pass 3: A -> B
    VkBuffer m_bufferA = VK_NULL_HANDLE;
    VkBuffer m_bufferB = VK_NULL_HANDLE;
    VkDeviceMemory m_bufferMemoryA = VK_NULL_HANDLE;
    VkDeviceMemory m_bufferMemoryB = VK_NULL_HANDLE;

    // We store two descriptor sets, one for A->B
    // and one for B->A
    VkDescriptorSet m_descriptorSetA_to_B = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSetB_to_A = VK_NULL_HANDLE;

    // We use the same problem size as the CPU
    static const uint32_t NUM_ELEMENTS = 1024 * 1024;
    static const uint32_t WORKGROUP_SIZE = 256;
};
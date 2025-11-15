#pragma once

#include "BaseComputeTask.h"

class LocalReduceTask : public BaseComputeTask {
public:
    LocalReduceTask(AAssetManager* assetManager);
    ~LocalReduceTask();

    // --- ComputeTask Interface ---
    long long dispatch() override;
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

    // --- Our 2 compute buffers ---
    VkBuffer m_bufferIn = VK_NULL_HANDLE;
    VkBuffer m_bufferOut = VK_NULL_HANDLE; // Will hold 1 float

    VkDeviceMemory m_bufferMemoryIn = VK_NULL_HANDLE;
    VkDeviceMemory m_bufferMemoryOut = VK_NULL_HANDLE;

    // Must match the shader's local_size_x
    static const uint32_t NUM_ELEMENTS = 256;
};
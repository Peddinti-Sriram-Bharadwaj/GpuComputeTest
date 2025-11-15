#pragma once

#include "BaseComputeTask.h"

// --- REMOVED: #include "vector_add_shader.h" ---

class VectorAddTask : public BaseComputeTask {
public:
    // --- MODIFIED: Constructor now takes AAssetManager ---
    VectorAddTask(AAssetManager* assetManager);
    ~VectorAddTask();

    // --- ComputeTask Interface ---
    void dispatch() override;
    void cleanup() override;

protected:
    // --- BaseComputeTask "Fill-in-the-blanks" ---
    std::string getShaderPath() override; // <-- MODIFIED
    void createDescriptorSetLayout() override;
    void createBuffers() override;
    void createDescriptorPool() override;
    void createDescriptorSet() override;

private:
    void createDeviceLocalBuffer(VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize size, const void* initialData);
    void cleanupBuffers();

    // --- Our 3 compute buffers ---
    VkBuffer m_bufferA = VK_NULL_HANDLE;
    VkBuffer m_bufferB = VK_NULL_HANDLE;
    VkBuffer m_bufferC = VK_NULL_HANDLE;

    VkDeviceMemory m_bufferMemoryA = VK_NULL_HANDLE;
    VkDeviceMemory m_bufferMemoryB = VK_NULL_HANDLE;
    VkDeviceMemory m_bufferMemoryC = VK_NULL_HANDLE;

    static const uint32_t NUM_ELEMENTS = 1024;
};
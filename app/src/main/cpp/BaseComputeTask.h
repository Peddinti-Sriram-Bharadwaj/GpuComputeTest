#pragma once

#include "ComputeTask.h"
#include <vector>
#include <string>
#include <android/asset_manager.h> // <-- NEW

class BaseComputeTask : public ComputeTask {
public:
    // --- MODIFIED: Constructor now takes the AAssetManager ---
    BaseComputeTask(AAssetManager* assetManager);
    virtual ~BaseComputeTask();

    // --- Template Method Implementation ---
    void init() override;
    void cleanup() override;

protected:
    // --- "Fill in the blank" methods for subclasses ---

    // 1. Subclass provides its shader file path
    virtual std::string getShaderPath() = 0; // <-- MODIFIED

    // 2. Subclass defines its buffer/binding layout
    virtual void createDescriptorSetLayout() = 0;

    // 3. Subclass creates its specific VkBuffers
    virtual void createBuffers() = 0;

    // 4. Subclass creates the descriptor pool
    virtual void createDescriptorPool() = 0;

    // 5. Subclass writes the descriptor set to link buffers
    virtual void createDescriptorSet() = 0;

    // --- NEW Helper: Compiles a shader from assets ---
    VkShaderModule loadShaderModule(const std::string& shaderPath);

    // --- Helper methods for subclasses ---
    void createBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

    // --- Common Vulkan Objects ---
    VulkanContext* m_context;
    AAssetManager* m_assetManager; // <-- NEW

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
};
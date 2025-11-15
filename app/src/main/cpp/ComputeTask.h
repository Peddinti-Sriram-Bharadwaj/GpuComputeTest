#pragma once

#include "VulkanContext.h"

class ComputeTask{
public:
    // Virtual destructor is required for base classes
    virtual ~ComputeTask() {}

    // --- Strategy Interface ---

    // 1. Create pipelines, buffers, descriptor sets
    virtual void init() = 0;

    // 2. Record commands and submit to the queue
    virtual long long dispatch() = 0;

    // 3. Clean up all the resources created in init()
    virtual void cleanup() = 0;
};
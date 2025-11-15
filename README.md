# GPGPU Collective Communications on Mobile (Pixel 10 Pro)

This project implements and profiles a parallel tree-reduction algorithm (a reduce collective) on both the CPU (multi-threaded) and the GPU (Vulkan compute) of a mobile device to analyze performance and identify synchronization overheads.

This repository is the result of the project proposal "GPU Collective Communication Algorithms on Pixel 10 Pro," with a focus on fulfilling the original, tool-agnostic objectives.

## Core Objectives

* Implement a `reduce` collective on both the CPU and the GPU.
* Quantify the synchronization overhead of a multi-pass Vulkan compute algorithm.
* Identify the "problem size threshold" where the GPU becomes more performant than the CPU.
* Analyze the performance of an iterative "hybrid" workload (frequent CPU-GPU sync).

## Technology Stack

* **Platform:** Android (Tested on Pixel 10 Pro, PowerVR D-Series GPU)
* **Language:** C++ (with C++17)
* **GPU API:** Vulkan (Compute)
* **Build System:** Android NDK (r27+), CMake
* **Shaders:** GLSL (maintained as `.comp` files)
* **SPIR-V:** Shaders are manually compiled using `glslc` and bundled in the app's assets directory.
* **CPU Sync:** `pthread_barrier_t`
* **GPU Sync:** `vkCmdPipelineBarrier`

## Project Architecture

The C++ code is structured using several Gang of Four (GoF) design patterns to ensure separation of concerns, easy debugging, and simple extensibility.

* **VulkanContext:** A **Singleton** that manages the global `VkInstance`, `VkDevice`, `VkQueue`, and `VkCommandPool`.
* **ComputeTask:** A **Strategy** interface (abstract class) that defines the `init()`, `dispatch()`, and `cleanup()` methods.
* **BaseComputeTask:** A **Template Method** class that provides common Vulkan logic (buffer creation, shader loading) for all GPU-based tasks.
* **CpuReduceTask:** A **Concrete Strategy** implementing `ComputeTask` for the multi-threaded CPU test.
* **GpuTreeReduceTask:** A **Concrete Strategy** implementing `BaseComputeTask` for the multi-pass Vulkan GPU test.

## How to Build and Run

1.  Open the project in Android Studio (Otter 2025.2.1+).
2.  Ensure NDK 27+ is installed via the SDK Manager.
3.  **Compile Shaders:** This project requires shaders to be manually compiled.
    ```bash
    # Navigate to the shaders directory
    cd app/src/main/assets/shaders/
    
    # Compile the tree reduce shader
    glslc tree_reduce.comp -o tree_reduce.spv
    ```
4.  Connect an Android device (API 24+ with Vulkan support).
5.  Click **Run**.
6.  Open the **Logcat** tab in Android Studio and filter for the tag `GpuCompute`.
7.  To switch which experiment is run, modify the `stringFromJNI` function in `app/src/main/cpp/native-lib.cpp`.

## Summary of Findings

The project successfully quantified the performance of CPU vs. GPU reduction and the cost of synchronization.

### 1. Final Benchmark Data

The following data was collected after a warmup pass to ensure stable, "hot cache" performance. The GPU time measured is the total end-to-end time, including all compute, memory, and synchronization stall time (`vkQueueWaitIdle`).

| Elements (N) | CPU Time (µs) | Total GPU Time (µs) | GPU Speedup |
| :--- | :--- | :--- | :--- |
| 256 | 8,144 | 1,816 | 4.5x |
| 1,024 | 7,324 | 605 | 12.1x |
| 4,096 | 5,964 | 1,057 | 5.6x |
| 16,384 | 6,137 | 615 | 10.0x |
| 32,768 | 6,619 | 632 | 10.5x |
| 65,536 | 2,481 | 656 | 3.8x |
| 131,072 | 4,793 | 695 | 6.9x |
| 262,144 | 8,296 | 927 | 8.9x |
| 524,288 | 9,624 | 1,309 | 7.4x |
| 1,048,576 | 9,251 | 1,633 | 5.7x |

### 2. Key Insights

* **Insight 1: The Crossover Point.** The initial hypothesis was that the CPU would be faster for small N. This was disproven. The GPU was significantly faster at all tested problem sizes, from 256 elements up to 1M. This implies the overhead of `vkCmdPipelineBarrier` is substantially lower than the overhead of `pthread_barrier_t` on this platform.

* **Insight 2: The GPU is Synchronization-Bound.** The GPU's performance is nearly flat from N=1,024 to N=131,072 (hovering around 600-700 µs). This proves the algorithm is barrier-bound, not compute-bound. The runtime is dominated by the fixed cost of the multi-pass dispatches, not the work being done.

* **Insight 3: The Hybrid Workload Bottleneck.** The iterative test (100 runs at N=1M) revealed the true bottleneck in a hybrid app:
    * Average GPU Task (`dispatch()`): 1,243 µs
    * Average CPU Task (`reset()`): 2,645 µs
    * **Conclusion:** The GPU spends 68% of its time idle, waiting for the CPU to map memory and prepare the next batch of data.

* **Insight 4: Unreliable Profiling Tools.** The GPU-native `vkCmdWriteTimestamp` queries were found to be unreliable on this PowerVR driver, returning 0.0 or inconsistent, repeating values. This confirms that end-to-end, CPU-side timing (`std::chrono`) is the only reliable method for this hardware.

## Future Work

* Implement the `allreduce` collective by adding a multi-pass "broadcast" tree after the `reduce`.
* Optimize the `passType == 1` shader to use shared memory, which would reduce the number of barriers needed.
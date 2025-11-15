# GPGPU Compute Project Journal

## 2025-11-15: Phase 0 - Project Setup (v0.1)

**Git Commit:** `feat(init): v0.1 - Initial project setup with Vulkan linked`

**Status:** Completed.

**What I did:**
* Created a new Android Studio "Native C++" project targeting API 24 (Android 7.0).
* Confirmed "Hello from C++" runs on the Pixel 10 Pro.
* Updated `AndroidManifest.xml` with Vulkan requirements (`<uses-feature>`).
* Updated `CMakeLists.txt` to find and link `libvulkan.so`.
* Set up the Git repository with a comprehensive `.gitignore` file.
* Created this journal file.

**Next Step:**
* Define the C++ class architecture (Singleton, Strategy) for our compute tasks.

# GPGPU Compute Project Journal

## 2025-11-15: Phase 1 - Vector Add (v1.0)
**Tag:** `v1.0-phase1-vector-add`

**Status:** Completed.

**What I did:**
* Implemented the full Vulkan compute pipeline using the OOP architecture (Singleton, Strategy).
* **Pivoted on shader compilation:** Bypassed complex build-time and runtime compilation by manually compiling the shader on the terminal.
* **New Workflow:**
    1.  Write shader (`vector_add.comp`).
    2.  Manually compile: `glslc vector_add.comp -o vector_add.spv`
    3.  Copy the `.spv` file to the app's `assets/shaders/` directory.
    4.  C++ code now loads the `.spv` *bytecode* from assets, bypassing `shaderc`.
* **Result:** App successfully runs the "Vector Add" task on the Pixel 10 Pro GPU.

**Key Learnings:**
* NDK build system for shaders (glslc path, Prefab, find_package) is extremely brittle.
* Manually compiling to `.spv` is a stable, simple, and effective solution for this project.

**Next Step:**
* Implement Phase 2 (Single-Workgroup Reduction) using this new manual-compile workflow.

## 2025-11-15: Phase 2 - Local Reduction (v2.0)
**Tag:** `v2.0-phase2-local-reduce`
**Branch:** `feat/2-local-reduce`

**Status:** Completed.

**What I did:**
* Wrote `local_reduce.comp` using `shared` memory and `barrier()` for an in-workgroup parallel sum.
* Manually compiled it to `local_reduce.spv` and loaded it from assets.
* Created the `LocalReduceTask` C++ class.
* Verified in Logcat that the sum of 1...256 was correctly calculated as 32896.

**Key Learnings:**
* The `barrier()` intrinsic is essential for synchronizing threads *within* a workgroup.
* The `shared` memory reduction pattern is very effective.

**Next Step:**
* Implement Phase 3 (CPU Baseline) for our first performance comparison.

## 2025-11-15: Phase 3 - CPU Baseline (v3.0)
**Tag:** `v3.0-phase3-cpu-baseline`
**Branch:** `feat/3-cpu-baseline`

**Status:** Completed.

**What I did:**
* Implemented a multi-threaded CPU reduction task (`CpuReduceTask`).
* Used `std::thread` and `pthread_barrier_t` to implement a two-stage reduction (local sum + tree sum).
* Ran on 1,048,576 elements (all 1.0f).
* Verified correct sum of 1,048,576.

**Performance Result:**
* **Baseline Time: 5452 µs (5.452 ms)**

**Next Step:**
* Implement the full multi-pass GPU reduction (Phase 4) to beat this time.

## 2025-11-15: Phase 4 - GPU Tree Reduction (v4.0)
**Tag:** `v4.0-phase4-gpu-tree-reduce`
**Branch:** `feat/4-gpu-tree-reduce`

**Status:** Completed.

**What I did:**
* Implemented a multi-pass, ping-ponging reduction on the GPU (`GpuTreeReduceTask`).
* Used a single shader with push constants (`passType`) to handle both the "local reduce" and "tree reduce" steps.
* Used `vkCmdPipelineBarrier` between each of the 12 dispatch passes to synchronize.
* Fixed an off-by-one bug in the final buffer selection.

**Performance Result (1M Elements):**
* **CPU Time:** 3056 µs
* **GPU Time:** 2015 µs
* **Result:** The GPU is **~34% faster** than the CPU, even with a non-optimized shader and heavy barrier synchronization.

**Next Step:**
* Implement Phase 5 (Profiling) to get the *true* GPU-only execution time using `vkCmdWriteTimestamp`.

## 2025-11-15: Phase 5 - Accurate GPU Profiling (v5.0)
**Tag:** `v5.0-final-profiling`
**Branch:** `feat/5-profiling`

**Status:** Completed.

**What I did:**
* Implemented a `VkQueryPool` with `vkCmdWriteTimestamp` to get the true, on-device GPU execution time.
* Fixed the final off-by-one buffer read bug.

**Final Performance Results (1M Elements):**
* **CPU (Multi-threaded):** 3056 µs
* **GPU (CPU-timed, incl. stall):** 2015 µs
* **GPU (True, `vkCmdWriteTimestamp`):** 1048 µs

**Conclusion:**
The project is a success. We've proven that for a 1M element reduction, the GPU on the Pixel 10 Pro is **2.9 times faster** than its multi-core CPU. We also identified that the CPU stall time (`vkQueueWaitIdle`) accounts for nearly 50% of the perceived "GPU time" when measured from the CPU.

## 2025-11-15: Phase 5.1 - Variable Size Analysis
**Tag:** `v5.1-variable-size-analysis`
**Branch:** `feat/6-variable-size`

**Status:** Completed.

**What I did:**
* Implemented a warmup loop to get stable, "warmed-up" performance.
* Refactored both `CpuReduceTask` and `GpuTreeReduceTask` to accept a variable problem size `N`.
* Ran a full benchmark on 10 data points from N=256 to N=1,048,576.

**Final Data Table (Warmed Up):**

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

**Key Insights:**
1.  **Crossover Point:** The hypothesis of a crossover was disproven. The GPU is significantly faster than the CPU at *all* tested sizes, even for N=256.
2.  **Bottleneck:** The GPU time is nearly flat from N=1k to N=131k. This proves the workload is **synchronization-bound (barrier-bound)**, not compute-bound. The fixed cost of the multiple dispatches dominates the runtime.
3.  **Profiling Tool:** The `vkCmdWriteTimestamp` query results were confirmed to be unreliable on this device, returning `0.0` or repeating values. The `CPU-side timer (incl. stall)` is the correct and reliable metric for this hardware.

**Next Step:**
* Analyze the iterative workload (Objective 2).
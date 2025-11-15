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
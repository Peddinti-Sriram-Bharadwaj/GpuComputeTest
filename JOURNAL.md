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
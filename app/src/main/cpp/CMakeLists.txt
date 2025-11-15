# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name.
project("gpucomputetest")

# --- 1. Find System Dependencies ---
find_library(log-lib log)
find_library(vulkan-lib vulkan)
find_library(android-lib android)

# --- 2. Setup Shader Compilation at Build Time ---

# Find glslc compiler (prioritize Homebrew on macOS)
find_program(GLSLC glslc HINTS
        "/opt/homebrew/bin"              # Homebrew on Apple Silicon
        "/usr/local/bin"                 # Homebrew on Intel Mac
        "$ENV{VULKAN_SDK}/bin"           # Vulkan SDK
        "$ENV{VULKAN_SDK}/macOS/bin"
        "$ENV{HOME}/VulkanSDK/*/macOS/bin"
)

if(GLSLC)
    message(STATUS "Found glslc: ${GLSLC}")

    # Example: Compile shaders at build time
    # Uncomment and modify for your shaders
    # set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    # set(SHADER_SOURCE "${SHADER_DIR}/compute.comp")
    # set(SHADER_OUTPUT "${SHADER_DIR}/compute.spv")
    #
    # add_custom_command(
    #     OUTPUT ${SHADER_OUTPUT}
    #     COMMAND ${GLSLC} ${SHADER_SOURCE} -o ${SHADER_OUTPUT}
    #     DEPENDS ${SHADER_SOURCE}
    #     COMMENT "Compiling shader: ${SHADER_SOURCE}"
    # )
    #
    # add_custom_target(compile_shaders ALL DEPENDS ${SHADER_OUTPUT})
    # add_dependencies(${CMAKE_PROJECT_NAME} compile_shaders)
else()
    message(WARNING
            "glslc not found in standard locations. Searched:\n"
            "  - $VULKAN_SDK/bin\n"
            "  - /usr/local/bin\n"
            "  - /opt/homebrew/bin\n"
            "You can:\n"
            "  1. Set VULKAN_SDK environment variable\n"
            "  2. Install via Homebrew: brew install glslc\n"
            "  3. Install Vulkan SDK: https://vulkan.lunarg.com/sdk/home\n"
            "  4. Compile shaders manually and use pre-compiled .spv files"
    )
endif()

# --- 3. Define the Library and Its Sources ---
add_library(${CMAKE_PROJECT_NAME} SHARED
        # JNI entry point
        native-lib.cpp

        # Your C++ implementation files
        VulkanContext.cpp
        BaseComputeTask.cpp
        VectorAddTask.cpp

        # Your C++ header files (for IDE visibility)
        VulkanContext.h
        ComputeTask.h
        BaseComputeTask.h
        VectorAddTask.h
)

# --- 4. Configure Target Properties ---

# Set C++ standard (recommended for Vulkan projects)
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)

# Optional: Enable warnings for better code quality
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Werror=return-type
)

# --- 5. Link Libraries to Your Target ---
target_link_libraries(${CMAKE_PROJECT_NAME}
        # Link the libraries found by find_library()
        ${vulkan-lib}
        ${log-lib}
        ${android-lib}
)

# --- 6. Optional: Strip symbols in Release builds for smaller APK ---
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
            -Wl,--strip-all
            -Wl,--gc-sections
    )
endif()
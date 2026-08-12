cmake_minimum_required(VERSION 3.10)

# Define the name of your project
project(helga VERSION 0.1)

# Enable ccache for faster builds
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Set the C++ standard to C++17 (the ray tracing books use modern C++)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force aggressive optimization and architecture-specific instructions
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O3 -march=native -ffast-math)
elseif(MSVC)
    add_compile_options(/O2 /fp:fast)
endif()

# Define the executable name ("helga") and its source and header files
add_executable(helga 
    src/pdf.cc
    src/main.cc
    src/camera.cc
    src/integrator.cc
    src/scene_parser.cc
    src/rtw_stb_image.cc
    src/rtw_obj_loader.cc
    src/rtw_stb_image_write.cc
)
target_include_directories(helga PRIVATE include)

# Find and link OpenMP to multithread the render loop
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(helga PUBLIC OpenMP::OpenMP_CXX)
endif()
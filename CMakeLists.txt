cmake_minimum_required(VERSION 3.15)
project(helga VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Generate the version header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/helga_version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/helga_version.h"
    @ONLY
)

# Enable ccache for faster builds
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Set the C++ standard to modern C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Configuration Options ---
option(BUILD_GUI "Build the graphical interface (requires SDL2/OpenGL)" OFF)
option(USE_NATIVE_ARCH "Optimize for current CPU (Disable when distributing binaries)" OFF)

# Force aggressive optimization and architecture-specific instructions
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(OPT_FLAGS "-O3" "-ffast-math" "-fno-finite-math-only")
    if(USE_NATIVE_ARCH)
        list(APPEND OPT_FLAGS "-march=native")
    endif()
    add_compile_options(${OPT_FLAGS})
elseif(MSVC)
    add_compile_options(/O2 /fp:fast /openmp:llvm)
endif()

# --- Core Files ---
set(HELGA_CORE_SRCS 
    src/pdf.cc
    src/camera.cc
    src/integrator.cc
    src/scene_parser.cc
    src/rtw_stb_image.cc
    src/post_processor.cc
    src/rtw_obj_loader.cc
    src/rtw_stb_image_write.cc
)

# --- Threading ---
find_package(OpenMP)
set(OMP_LIB "")
if(OpenMP_CXX_FOUND)
    set(OMP_LIB OpenMP::OpenMP_CXX)
endif()

# ==========================================
# Target 1: Command Line Interface (Headless)
# ==========================================
add_executable(helga-cli src/main_cli.cc ${HELGA_CORE_SRCS})
target_include_directories(helga-cli PRIVATE
    include
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

if(OMP_LIB)
    target_link_libraries(helga-cli PUBLIC ${OMP_LIB})
endif()

# ==========================================
# Target 2: Graphical User Interface
# ==========================================
if(BUILD_GUI)
    find_package(SDL2 REQUIRED)
    find_package(OpenGL REQUIRED)
    
    file(GLOB IMGUI_SRCS "include/external/imgui/*.cpp" "include/external/imgui/backends/*.cpp")

    add_executable(helga-gui src/main_gui.cc ${HELGA_CORE_SRCS} ${IMGUI_SRCS})
    target_compile_definitions(helga-gui PRIVATE HELGA_VERSION="${PROJECT_VERSION}")
    target_include_directories(helga-gui PRIVATE
        include
        include/external/imgui
        include/external/imgui/backends
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
    target_link_libraries(helga-gui PUBLIC ${OMP_LIB} SDL2::SDL2 OpenGL::GL)
endif()
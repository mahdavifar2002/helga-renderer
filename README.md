# Helga Renderer

Helga is a physically-based ray tracer written in C++. This project is based on Peter Shirley's [*Ray Tracing in One Weekend*](https://raytracing.github.io) book series.

## Prerequisites
To build this project in a Linux environment, you will need the following packages installed:

* A modern C++ compiler (GCC or Clang)

  `apt install build-essential`

* CMake (Version 3.15 or higher)

  `apt install cmake`

* Ninja build system

  `apt install ninja-build`

* SDL2 and OpenGL (for building the GUI executable as well)

  `apt install libsdl2-dev libgl1-mesa-dev`

## Build Instructions

This project uses an out-of-source CMake build to keep the workspace clean. To configure and compile the project in `Debug` mode, follow the instructions below.

### Linux Development Build

```bash
# Configure the build directory
cmake --preset dev-linux

# Compile the executable
cmake --build build/dev

# Enter the build directory
cd build/dev
```

### Windows Development Build

```bash
# Configure the build directory
cmake --preset dev-windows

# Compile the executable
cmake --build build\dev --preset dev-windows

# Enter the build directory
cd build\dev
```

## Running
Once compiled, you can run the CLI executable and pipe the output to a `.png` and a high dynamic range `.hdr` image file.

The repository contains example JSON scenes which could be used for testing.

```bash
./helga-cli --width 100 --samples 50 --scene ../../scenes/bedroom.json --output bedroom.png
```

## Project Structure
* `src/` - Contains the main execution logic and C++ source files.

* `include/` - Contains all class headers (`vec3.h`, `ray.h`, `camera.h`, etc.).

* `CMakeLists.txt` - Build system configuration and compiler flags.

## Sample Rendered Scene
![A minimal low-poly scene of a bedroom with two warm creamy lights](screenshots/bedroom.png)

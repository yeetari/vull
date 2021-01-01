# vull

A rendering engine using C++ 20 and Vulkan.

## Features

* Tiled forward rendering
* Minimal dependencies - only GLFW, GLM and Vulkan + limited reliance on the C++ standard library

## Building and running

Vull uses [CMake](https://cmake.org) as its primary build system. On Linux systems, `ninja` or `make` will additionally
be needed. Windows users should be able to use both the NMake and Visual Studio generators, although this hasn't been
tested. To compile the shaders, [glslc](https://github.com/google/shaderc) is required to be on the `PATH`.

### Installing dependencies and tools

#### Gentoo

    emerge \
     dev-util/cmake \
     dev-util/ninja \
     dev-util/vulkan-headers \
     media-libs/glfw \
     media-libs/glm \
     media-libs/shaderc \
     media-libs/vulkan-layers \
     media-libs/vulkan-loader
     
Note that installing the Vulkan validation layers is optional, but is recommended when debugging.

### Configuring CMake

Configuring CMake is as simple as:

    cmake . \
     -Bbuild \
     -DCMAKE_BUILD_TYPE=Release \
     -DVULL_BUILD_BENCHMARKS=ON \
     -GNinja

This will create a release build and also enable building the benchmarks. To also build the tests, pass
`-DVULL_BUILD_TESTS=ON`. Note that creating a release build automatically enables LTO and fast math, if available.

To make a debug build instead, pass `-DCMAKE_BUILD_TYPE=Debug`. This will enable assertions in the code and will
attempt to use the Vulkan validation layers (a warning will be printed if they could not be loaded).

### Building

To build with a makefile generate run:

    cmake --build build
    
### Running

Depending on the options passed to CMake, three executables will be produced after building:

* `./build/engine/benchmarks/vull-benchmarks`
* `./build/engine/tests/vull-tests`
* `./build/sandbox/vull-sandbox`

# vull

A vulkan rendering engine written in C++ 20.

![screenshot](docs/screenshot.jpg)

## Features

* Render graph based vulkan abstraction
* Work-stealing coroutine job system
* Retained mode GUI
* Compressed pack file format

## Dependencies

* [bc7enc_rdo](https://github.com/richgel999/bc7enc_rdo) (for vpak tool; vendored)
* [freetype](https://freetype.org)
* [harfbuzz](https://github.com/harfbuzz/harfbuzz)
* [libpng](https://github.com/glennrp/libpng) (for vpak tool)
* [meshoptimizer](https://github.com/zeux/meshoptimizer) (for vpak tool)
* [shaderc](https://github.com/google/shaderc) (for offline shader compilation step)
* [simdjson](https://github.com/simdjson/simdjson) (for vpak tool)
* [xcb](https://xcb.freedesktop.org)
* [zstd](https://github.com/facebook/zstd)

## Requirements

* Vulkan 1.3
* VK_EXT_descriptor_buffer
* VK_EXT_shader_atomic_float
* VK_EXT_shader_atomic_float2

## Building and running

Vull uses [cmake](https://cmake.org) as its build system. Either `make` or `ninja` will additionally be needed. Windows
is not currently supported.

### Installing dependencies and tools

#### Gentoo

    emerge -n \
     app-arch/zstd \
     dev-util/cmake \
     dev-util/ninja \
     media-libs/freetype \
     media-libs/harfbuzz \
     media-libs/libpng \
     media-libs/shaderc \
     media-libs/vulkan-layers \
     media-libs/vulkan-loader \
     x11-libs/libxcb \
     x11-libs/xcb-util

Note that `meshoptimizer` and `simdjson` are always linked to via `FetchContent`, and that `bc7enc_rdo` is vendored. The
vulkan validation layers (`vulkan-layers`) are not required but are extremely useful for development and testing.

### Configuring CMake

To configure vull, use one of the available presets:

    cmake . \
     --preset release-gcc \
     -Bbuild \
     -DVULL_BUILD_SANDBOX=ON \
     -DVULL_BUILD_TESTS=ON \
     -GNinja

#### Available options

| Option               | Description                           | Default Value |
|----------------------|---------------------------------------|---------------|
| `VULL_BUILD_GLTF`    | Build the vpak tool with glTF support | `OFF`         |
| `VULL_BUILD_PNG`     | Build the vpak tool with PNG support  | `OFF`         |
| `VULL_BUILD_SANDBOX` | Build the sandbox application         | `ON`          |
| `VULL_BUILD_TESTS`   | Build tests                           | `OFF`         |

### Building

    cmake --build build

### Running the tests

If `VULL_BUILD_TESTS` is enabled, tests can be run with

    cmake --build build --target test

## Building the documentation

If doxygen is available, documentation can be built with

    cmake --build build --target vull-docs

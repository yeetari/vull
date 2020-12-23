cmake_minimum_required(VERSION 3.19)
project(vull CXX)

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)
add_subdirectory(shaders)
add_subdirectory(src)

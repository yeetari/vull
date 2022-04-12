#pragma once

#include <vull/support/Vector.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull {

class CommandPool;
class PackReader;
class Queue;
class VkContext;
class World;

} // namespace vull

void load_scene(vull::VkContext &context, vull::PackReader &pack_reader, vull::CommandPool &command_pool,
                vull::Queue &queue, vull::World &world, vull::Vector<vull::vk::Buffer> &vertex_buffers,
                vull::Vector<vull::vk::Buffer> &index_buffers, vull::Vector<vull::vk::Image> &images,
                vull::Vector<vull::vk::ImageView> &image_views, vull::vk::DeviceMemory memory);

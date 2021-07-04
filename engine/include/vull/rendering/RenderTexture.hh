#pragma once

#include <vull/rendering/MemoryResource.hh>
#include <vull/rendering/RenderResource.hh>

#include <vulkan/vulkan_core.h>

#include <cstdint>

enum class TextureType {
    Depth,
    Normal,
    Swapchain,
};

// TODO: Make sure every class has gang of five implemented.
class RenderTexture : public RenderResource, public MemoryResource {
    const TextureType m_type;
    VkClearValue m_clear_value{};
    VkExtent3D m_extent{};
    VkFormat m_format{VK_FORMAT_UNDEFINED};

protected:
    VkImage m_image{nullptr};
    VkImageView m_image_view{nullptr};

public:
    RenderTexture(TextureType type, MemoryUsage usage);
    ~RenderTexture();

    void set_clear_colour(float r, float g, float b, float a);
    void set_clear_depth_stencil(float depth, std::uint32_t stencil);
    void set_extent(VkExtent3D extent);
    void set_format(VkFormat format);

    void build_objects(const Device &device, ExecutableGraph *executable_graph) override;
    void transfer(const void *data, VkDeviceSize size) override;

    TextureType type() const { return m_type; }
    const VkClearValue &clear_value() const { return m_clear_value; }
    const VkExtent3D &extent() const { return m_extent; }
    VkFormat format() const { return m_format; }
    VkImageView image_view() const { return m_image_view; }
};

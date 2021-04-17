#include <vull/renderer/Image.hh>

#include <vull/renderer/Device.hh>
#include <vull/support/Assert.hh>

Image::~Image() {
    ASSERT(m_device != nullptr);
    vkDestroyImage(**m_device, m_image, nullptr);
    vkFreeMemory(**m_device, m_memory, nullptr);
}

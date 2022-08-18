#include <vull/vulkan/QueryPool.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Span.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

QueryPool::QueryPool(const Context &context, uint32_t count, vkb::QueryType type) : m_context(context), m_count(count) {
    vkb::QueryPoolCreateInfo pool_ci{
        .sType = vkb::StructureType::QueryPoolCreateInfo,
        .queryType = type,
        .queryCount = count,
    };
    VULL_ENSURE(m_context.vkCreateQueryPool(&pool_ci, &m_pool) == vkb::Result::Success);
}

QueryPool::QueryPool(QueryPool &&other) : m_context(other.m_context) {
    m_count = vull::exchange(other.m_count, 0u);
    m_pool = vull::exchange(other.m_pool, nullptr);
}

QueryPool::~QueryPool() {
    m_context.vkDestroyQueryPool(m_pool);
}

void QueryPool::read_host(Span<uint64_t> data, uint32_t first) const {
    m_context.vkGetQueryPoolResults(m_pool, first, data.size(), data.size_bytes(), data.data(), sizeof(uint64_t),
                                    vkb::QueryResultFlags::_64);
}

} // namespace vull::vk

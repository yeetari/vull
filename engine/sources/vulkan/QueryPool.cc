#include <vull/vulkan/QueryPool.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Span.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

QueryPool::QueryPool(const Context &context, uint32_t count, vkb::QueryType type) : m_context(context) {
    recreate(count, type);
}

QueryPool::QueryPool(const Context &context, uint32_t count, vkb::QueryPipelineStatisticFlags pipeline_statistics)
    : m_context(context) {
    recreate(count, vkb::QueryType::PipelineStatistics, pipeline_statistics);
}

QueryPool::QueryPool(QueryPool &&other) : m_context(other.m_context) {
    m_count = vull::exchange(other.m_count, 0u);
    m_pool = vull::exchange(other.m_pool, nullptr);
}

QueryPool::~QueryPool() {
    m_context.vkDestroyQueryPool(m_pool);
}

void QueryPool::read_host(Span<uint64_t> data, uint32_t count, uint32_t first) const {
    VULL_ASSERT(data.size_bytes() % count == 0);
    const auto stride = data.size_bytes() / count;
    m_context.vkGetQueryPoolResults(m_pool, first, count, data.size_bytes(), data.data(), stride,
                                    vkb::QueryResultFlags::_64);
}

void QueryPool::recreate(uint32_t count, vkb::QueryType type, vkb::QueryPipelineStatisticFlags pipeline_statistics) {
    m_context.vkDestroyQueryPool(vull::exchange(m_pool, nullptr));
    vkb::QueryPoolCreateInfo pool_ci{
        .sType = vkb::StructureType::QueryPoolCreateInfo,
        .queryType = type,
        .queryCount = (m_count = count),
        .pipelineStatistics = pipeline_statistics,
    };
    VULL_ENSURE(m_context.vkCreateQueryPool(&pool_ci, &m_pool) == vkb::Result::Success);
    m_context.vkResetQueryPool(m_pool, 0, count);
}

} // namespace vull::vk

#pragma once

#include <vull/support/Span.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

class QueryPool {
    const Context &m_context;
    uint32_t m_count{0};
    vkb::QueryPool m_pool{nullptr};

public:
    explicit QueryPool(const Context &context) : m_context(context) {}
    QueryPool(const Context &context, uint32_t count, vkb::QueryType type);
    QueryPool(const Context &context, uint32_t count, vkb::QueryPipelineStatisticFlags pipeline_statistics);
    QueryPool(const QueryPool &) = delete;
    QueryPool(QueryPool &&);
    ~QueryPool();

    QueryPool &operator=(const QueryPool &) = delete;
    QueryPool &operator=(QueryPool &&) = delete;

    void read_host(Span<uint64_t> data, uint32_t count, uint32_t first = 0) const;
    void recreate(uint32_t count, vkb::QueryType type,
                  vkb::QueryPipelineStatisticFlags pipeline_statistics = vkb::QueryPipelineStatisticFlags::None);

    explicit operator bool() const { return m_pool != nullptr; }
    const Context &context() const { return m_context; }
    uint32_t count() const { return m_count; }
    vkb::QueryPool operator*() const { return m_pool; }
};

} // namespace vull::vk

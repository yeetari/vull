#pragma once

#include <vull/support/Span.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

class QueryPool {
    const Context &m_context;
    uint32_t m_count;
    vkb::QueryPool m_pool;

public:
    QueryPool(const Context &context, uint32_t count, vkb::QueryType type);
    QueryPool(const QueryPool &) = delete;
    QueryPool(QueryPool &&);
    ~QueryPool();

    QueryPool &operator=(const QueryPool &) = delete;
    QueryPool &operator=(QueryPool &&) = delete;

    void read_host(Span<uint64_t> data, uint32_t first = 0) const;
    uint32_t count() const { return m_count; }
    vkb::QueryPool operator*() const { return m_pool; }
};

} // namespace vull::vk

#pragma once

#include <vull/support/source_location.hh>
#include <vull/support/string_view.hh>

#include <stdint.h>

namespace vull::tracing {

class ScopedTrace {
    bool m_active{true};

public:
    explicit ScopedTrace(StringView name, uint32_t colour = 0,
                         const SourceLocation &location = SourceLocation::current());
    ScopedTrace(const ScopedTrace &) = delete;
    ScopedTrace(ScopedTrace &&) = delete;
    ~ScopedTrace();

    ScopedTrace &operator=(const ScopedTrace &) = delete;
    ScopedTrace &operator=(ScopedTrace &&) = delete;

    void add_text(StringView text) const;
    void finish();
};

/**
 * @brief Returns true if the Tracy profiler is enabled.
 */
bool is_enabled();

void end_frame();
template <typename T>
void plot_data(const char *name, T value);
void enter_fiber(const char *name);
void leave_fiber();

} // namespace vull::tracing

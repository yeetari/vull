#include <vull/core/tracing.hh>

#include <vull/support/source_location.hh> // IWYU pragma: keep
#include <vull/support/string_view.hh>

#ifdef TRACY_ENABLE
#include <tracy/../client/TracyProfiler.hpp>
#include <vull/support/integral.hh>
#include <vull/support/utility.hh>
#endif

namespace vull::tracing {

ScopedTrace::ScopedTrace([[maybe_unused]] StringView name, [[maybe_unused]] uint32_t colour,
                         [[maybe_unused]] const SourceLocation &location) {
#ifdef TRACY_ENABLE
    TracyQueuePrepare(tracy::QueueType::ZoneBeginAllocSrcLoc);
    const auto data = tracy::Profiler::AllocSourceLocation(
        location.line(), location.file_name().data(), location.file_name().length(), location.function_name().data(),
        location.function_name().length(), name.data(), name.length(), colour);
    tracy::MemWrite(&item->zoneBegin.time, tracy::Profiler::GetTime());
    tracy::MemWrite(&item->zoneBegin.srcloc, data);
    TracyQueueCommit(zoneBeginThread);
#endif
}

ScopedTrace::~ScopedTrace() {
    finish();
}

void ScopedTrace::add_text([[maybe_unused]] StringView text) const {
    if (!m_active) {
        return;
    }
#ifdef TRACY_ENABLE
    auto *data = tracy::tracy_malloc(text.length());
    memcpy(data, text.data(), text.length());
    TracyQueuePrepare(tracy::QueueType::ZoneText);
    tracy::MemWrite(&item->zoneTextFat.text, data);
    tracy::MemWrite(&item->zoneTextFat.size, static_cast<uint16_t>(text.length()));
    TracyQueueCommit(zoneTextFatThread);
#endif
}

void ScopedTrace::finish() {
#ifdef TRACY_ENABLE
    if (m_active) {
        TracyQueuePrepare(tracy::QueueType::ZoneEnd);
        tracy::MemWrite(&item->zoneEnd.time, tracy::Profiler::GetTime());
        TracyQueueCommit(zoneEndThread);
    }
#endif
    m_active = false;
}

bool is_enabled() {
#ifdef TRACY_ENABLE
    return true;
#else
    return false;
#endif
}

void end_frame() {
#ifdef TRACY_ENABLE
    tracy::Profiler::SendFrameMark(nullptr);
#endif
}

template <typename T>
void plot_data([[maybe_unused]] const char *name, [[maybe_unused]] T value) {
#ifdef TRACY_ENABLE
    if constexpr (vull::is_integral<T>) {
        tracy::Profiler::PlotData(name, static_cast<int64_t>(value));
    } else if constexpr (vull::is_same<T, float> || vull::is_same<T, double>) {
        tracy::Profiler::PlotData(name, value);
    } else {
        static_assert(!vull::is_same<T, T>);
    }
#endif
}

void enter_fiber([[maybe_unused]] const char *name) {
#ifdef TRACY_ENABLE
    tracy::Profiler::EnterFiber(name, 0);
#endif
}

void leave_fiber() {
#ifdef TRACY_ENABLE
    tracy::Profiler::LeaveFiber();
#endif
}

template void plot_data(const char *, bool);
template void plot_data(const char *, char);
template void plot_data(const char *, int8_t);
template void plot_data(const char *, int16_t);
template void plot_data(const char *, int32_t);
template void plot_data(const char *, int64_t);
template void plot_data(const char *, uint8_t);
template void plot_data(const char *, uint16_t);
template void plot_data(const char *, uint32_t);
template void plot_data(const char *, uint64_t);
template void plot_data(const char *, float);
template void plot_data(const char *, double);

} // namespace vull::tracing

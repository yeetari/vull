#include <vull/graphics/frame_pacer.hh>

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/maths/vec.hh>
#include <vull/platform/event.hh>
#include <vull/platform/thread.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/promise.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/query_pool.hh>
#include <vull/vulkan/render_graph.hh>
#include <vull/vulkan/semaphore.hh>
#include <vull/vulkan/swapchain.hh>

#include <stdint.h>

namespace vull {
namespace {

HashMap<String, float> get_pass_times(UniquePtr<vk::RenderGraph> &render_graph) {
    if (!render_graph) {
        return {};
    }

    auto &timestamp_pool = render_graph->timestamp_pool();
    if (!timestamp_pool) {
        return {};
    }

    Vector<uint64_t> timestamp_data(timestamp_pool.count());
    timestamp_pool.read_host(timestamp_data.span(), timestamp_pool.count());

    for (uint32_t i = 1; i < timestamp_data.size(); i++) {
        if (timestamp_data[i] == 0) {
            timestamp_data[i] = timestamp_data[i - 1];
        }
    }

    HashMap<String, float> times;
    for (uint32_t i = 0; i < render_graph->pass_count(); i++) {
        const vk::Pass &pass = render_graph->pass_order()[i];
        float time = timestamp_pool.context().timestamp_elapsed(timestamp_data[i], timestamp_data[i + 1]);
        times.set(pass.name(), time);
    }
    return times;
}

} // namespace

FramePacer::FramePacer(vk::Swapchain &swapchain, uint32_t queue_length)
    : m_context(swapchain.context()), m_swapchain(swapchain) {
    VULL_ASSERT(queue_length > 0);

    // Create per-queued frame objects.
    m_frame_futures.ensure_size(queue_length);
    m_render_graphs.ensure_size(queue_length);
    for (uint32_t i = 0; i < queue_length; i++) {
        auto &acquire_semaphore = m_acquire_semaphores.emplace(m_context);
        m_context.set_object_name(acquire_semaphore, vull::format("Acquire semaphore #{}", i));
    }

    // TODO(tasklet): Allow futures to start completed.
    for (auto &future : m_frame_futures) {
        future = tasklet::schedule([] {});
    }

    // Spawn WSI thread.
    auto &scheduler = tasklet::Scheduler::current();
    m_thread = VULL_EXPECT(platform::Thread::create([this, &scheduler] {
        scheduler.setup_thread();

        Optional<uint32_t> image_index;
        while (m_running.load(vull::memory_order_acquire)) {
            if (m_swapchain.is_recreate_required(m_window_extent)) {
                m_context.vkDeviceWaitIdle();
                m_swapchain.recreate(m_window_extent);
                image_index.clear();

                // There needs to be a present semaphore per swapchain image.
                m_present_semaphores.clear();
                for (uint32_t i = 0; i < m_swapchain.image_count(); i++) {
                    auto &present_semaphore = m_present_semaphores.emplace(m_context);
                    m_context.set_object_name(present_semaphore, vull::format("Present semaphore #{}", i));
                }
            }

            // Present the current frame.
            if (image_index) {
                Array present_wait_semaphores{*m_present_semaphores[*image_index]};
                m_swapchain.present(*image_index, present_wait_semaphores.span());
            }

            // Advance the frame index.
            m_frame_index = (m_frame_index + 1) % m_frame_futures.size();

            // Wait on the frame future. This prevents the host running ahead.
            m_frame_futures[m_frame_index].await();

            // Get the render graph for the previous frame N and the pass timings.
            auto &render_graph = m_render_graphs[m_frame_index];
            auto pass_times = get_pass_times(render_graph);

            // Make a new render graph for the next frame, deleting the old one.
            render_graph = vull::make_unique<vk::RenderGraph>(m_context);

            // Acquire an image for the next frame.
            const auto &acquire_semaphore = m_acquire_semaphores[m_frame_index];
            image_index = m_swapchain.acquire_image(*acquire_semaphore);
            if (!image_index) {
                continue;
            }

            // Pass the frame info to the render tasklet.
            m_promise.fulfill(FrameInfo{
                .acquire_semaphore = acquire_semaphore,
                .present_semaphore = m_present_semaphores[*image_index],
                .swapchain_image = m_swapchain.image(*image_index),
                .graph = *render_graph,
                .pass_times = vull::move(pass_times),
                .frame_index = m_frame_index,
            });

            // Wait for the command recording to complete before presenting.
            m_recorded_event.wait();
        }
    }));
    VULL_IGNORE(m_thread.set_name("Frame pacer"));
}

FramePacer::~FramePacer() {
    m_running.store(false, vull::memory_order_release);
    m_recorded_event.set();
    for (auto &future : m_frame_futures) {
        future.await();
    }
    VULL_EXPECT(m_thread.join());
    m_context.vkDeviceWaitIdle();
}

FrameInfo FramePacer::acquire_frame(Vec2u window_extent) {
    m_window_extent = window_extent;
    m_promise.wait();
    auto frame_info = vull::move(m_promise.value());
    m_promise.reset();
    return frame_info;
}

void FramePacer::submit_frame(tasklet::Future<void> &&future) {
    m_frame_futures[m_frame_index] = vull::move(future);
    m_recorded_event.set();
}

} // namespace vull

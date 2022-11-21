#include <vull/core/Log.hh>

#include <vull/platform/Timer.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/thread/Mutex.hh>
#include <vull/thread/ScopedLocker.hh>

// IWYU pragma: no_include <bits/types/struct_sched_param.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

namespace vull {
namespace {

// SPSC lock-free queue. Owning thread is the producer, sink tasklet is the consumer.
// TODO: Per-thread allocator for log messages?
class LogQueue {
    Array<Atomic<char *>, 256> m_slots;
    Atomic<int64_t> m_head;
    Atomic<int64_t> m_tail;

public:
    void enqueue(String &&);
    Optional<String> dequeue();
};

void LogQueue::enqueue(String &&message) {
    int64_t head = m_head.load(MemoryOrder::Relaxed);
    int64_t tail = m_tail.load(MemoryOrder::Acquire);

    // TODO: Messages are lost if the queue is full.
    if (head - tail >= m_slots.size()) [[unlikely]] {
        return;
    }

    // Store element in slot and bump the head index.
    m_slots[static_cast<uint32_t>(head % m_slots.size())].store(message.disown(), MemoryOrder::Relaxed);
    m_head.store(head + 1, MemoryOrder::Release);
}

Optional<String> LogQueue::dequeue() {
    int64_t tail = m_tail.load(MemoryOrder::Relaxed);
    int64_t head = m_head.load(MemoryOrder::Acquire);

    // Nothing available.
    if (tail >= head) {
        return {};
    }

    char *msg = m_slots[static_cast<uint32_t>(tail % m_slots.size())].exchange(nullptr, MemoryOrder::Relaxed);
    m_tail.store(tail + 1, MemoryOrder::Release);
    return String::move_raw(msg, strlen(msg));
}

class GlobalState {
    Vector<LogQueue *> m_queues;
    Mutex m_queues_mutex;
    sem_t m_semaphore;
    pthread_t m_sink_thread;
    Atomic<bool> m_running{true};

public:
    GlobalState();
    GlobalState(const GlobalState &) = delete;
    GlobalState(GlobalState &&) = delete;
    ~GlobalState();

    GlobalState &operator=(const GlobalState &) = delete;
    GlobalState &operator=(GlobalState &&) = delete;

    auto begin() const { return m_queues.begin(); }
    auto end() const { return m_queues.end(); }

    void add_queue(LogQueue *);
    void close();
    void post();
    bool wait();
};

void *sink_loop(void *);

GlobalState::GlobalState() {
    sched_param param{};
    sem_init(&m_semaphore, 0, 0);
    pthread_create(&m_sink_thread, nullptr, &sink_loop, &m_queues_mutex);
    pthread_setschedparam(m_sink_thread, SCHED_IDLE, &param);
}

GlobalState::~GlobalState() {
    close();
    sem_destroy(&m_semaphore);
    for (auto *queue : m_queues) {
        delete queue;
    }
}

void GlobalState::add_queue(LogQueue *queue) {
    ScopedLocker locker(m_queues_mutex);
    m_queues.push(queue);
}

void GlobalState::close() {
    m_running.store(false, MemoryOrder::Relaxed);
    sem_post(&m_semaphore);
    pthread_join(m_sink_thread, nullptr);
}

void GlobalState::post() {
    sem_post(&m_semaphore);
}

bool GlobalState::wait() {
    m_queues_mutex.unlock();
    if (!m_running.load(MemoryOrder::Relaxed)) {
        if (sem_trywait(&m_semaphore) == 0) {
            // Still messages queued.
            m_queues_mutex.lock();
            return true;
        }
        VULL_ASSERT(errno == EAGAIN);
        return false;
    }
    sem_wait(&m_semaphore);
    m_queues_mutex.lock();
    return true;
}

VULL_GLOBAL(thread_local LogQueue *s_queue = nullptr);
VULL_GLOBAL(GlobalState s_state);

void *sink_loop(void *mutex) {
    static_cast<Mutex *>(mutex)->lock();
    while (s_state.wait()) {
        // TODO: Sort messages by timestamp.
        for (auto *queue : s_state) {
            while (auto message = queue->dequeue()) {
                // TODO: Proper sink system (e.g. FileSink, StdoutSink).
                printf("%s\n", message->data());
            }
        }
    }
    return nullptr;
}

} // namespace

VULL_GLOBAL(Timer g_log_timer);

void log_raw(String &&message) {
    if (s_queue == nullptr) {
        s_queue = new LogQueue;
        s_state.add_queue(s_queue);
    }
    s_queue->enqueue(vull::move(message));
    s_state.post();
}

void log_close() {
    s_state.close();
}

} // namespace vull

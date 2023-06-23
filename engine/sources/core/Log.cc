#include <vull/core/Log.hh>

#include <vull/container/Array.hh>
#include <vull/platform/Timer.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>

// IWYU pragma: no_include <bits/types/struct_sched_param.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

namespace vull {
namespace {

struct LogMessage {
    size_t length;

    void *operator new(size_t size, size_t length) { return new uint8_t[size + length]; }

    // NOLINTNEXTLINE
    void operator delete(void *ptr) { delete[] static_cast<uint8_t *>(ptr); }
};

// SPSC lock-free queue. Owning thread is the producer, sink thread is the consumer.
// TODO: Per-thread allocator for log messages?
class LogQueue {
    Array<Atomic<LogMessage *>, 256> m_slots;
    Atomic<int64_t> m_head;
    Atomic<int64_t> m_tail;
    Atomic<LogQueue *> m_next;
    Atomic<uint32_t> m_lost_message_count;

public:
    LogQueue() = default;
    LogQueue(const LogQueue &) = delete;
    LogQueue(LogQueue &&) = delete;
    ~LogQueue();

    LogQueue &operator=(const LogQueue &) = delete;
    LogQueue &operator=(LogQueue &&) = delete;

    void enqueue(StringView);
    LogMessage *dequeue();

    void set_next(LogQueue *queue) { m_next.store(queue); }
    LogQueue *next() const { return m_next.load(); }
    uint32_t exchange_lost_message_count() { return m_lost_message_count.exchange(0); }
};

LogQueue::~LogQueue() {
    for (int64_t i = m_tail.load(); i < m_head.load(); i++) {
        delete m_slots[static_cast<uint32_t>(i % m_slots.size())].exchange(nullptr);
    }
}

void LogQueue::enqueue(StringView string) {
    int64_t head = m_head.load(MemoryOrder::Relaxed);
    int64_t tail = m_tail.load(MemoryOrder::Acquire);

    if (head - tail >= m_slots.size()) [[unlikely]] {
        m_lost_message_count.fetch_add(1);
        return;
    }

    auto *message = new (string.length()) LogMessage{
        .length = string.length(),
    };
    memcpy(message + 1, string.data(), string.length());

    // Store element in slot and bump the head index.
    m_slots[static_cast<uint32_t>(head % m_slots.size())].store(message);
    m_head.store(head + 1, MemoryOrder::Release);
}

LogMessage *LogQueue::dequeue() {
    int64_t tail = m_tail.load(MemoryOrder::Relaxed);
    int64_t head = m_head.load(MemoryOrder::Acquire);

    // Nothing available.
    if (tail >= head) {
        return {};
    }

    auto *message = m_slots[static_cast<uint32_t>(tail % m_slots.size())].exchange(nullptr);
    m_tail.store(tail + 1, MemoryOrder::Release);
    return message;
}

class GlobalState {
    Atomic<LogQueue *> m_queue_head{nullptr};
    Atomic<LogQueue *> m_queue_tail{nullptr};
    sem_t m_semaphore;
    pthread_t m_sink_thread{};
    Atomic<bool> m_running;

public:
    GlobalState();
    GlobalState(const GlobalState &) = delete;
    GlobalState(GlobalState &&) = delete;
    ~GlobalState();

    GlobalState &operator=(const GlobalState &) = delete;
    GlobalState &operator=(GlobalState &&) = delete;

    void open_sink();
    void close_sink();

    void add_queue(LogQueue *);
    void post();
    bool wait();

    LogQueue *queue_head() const { return m_queue_head.load(); }
};

void *sink_loop(void *);

GlobalState::GlobalState() {
    sem_init(&m_semaphore, 0, 0);
}

GlobalState::~GlobalState() {
    close_sink();
    sem_destroy(&m_semaphore);
    for (auto *queue = m_queue_head.load(); queue != nullptr;) {
        delete vull::exchange(queue, queue->next());
    }
}

void GlobalState::open_sink() {
    sched_param param{};
    m_running.store(true);
    pthread_create(&m_sink_thread, nullptr, &sink_loop, nullptr);
    pthread_setschedparam(m_sink_thread, SCHED_IDLE, &param);
}

void GlobalState::close_sink() {
    if (m_running.exchange(false)) {
        sem_post(&m_semaphore);
        pthread_join(m_sink_thread, nullptr);
    }
}

void GlobalState::add_queue(LogQueue *queue) {
    auto *tail = m_queue_tail.load();
    while (!m_queue_tail.compare_exchange_weak(tail, queue)) {
    }
    if (tail != nullptr) {
        VULL_ASSERT(tail->next() == nullptr);
        tail->set_next(queue);
    } else {
        [[maybe_unused]] auto *head = m_queue_head.exchange(queue);
        VULL_ASSERT(head == nullptr);
    }
}

void GlobalState::post() {
    sem_post(&m_semaphore);
}

bool GlobalState::wait() {
    if (!m_running.load()) {
        if (sem_trywait(&m_semaphore) == 0) {
            // Still messages queued.
            return true;
        }
        VULL_ASSERT(errno == EAGAIN);
        return false;
    }
    sem_wait(&m_semaphore);
    return true;
}

VULL_GLOBAL(thread_local LogQueue *s_queue = nullptr);
VULL_GLOBAL(GlobalState s_state);

void *sink_loop(void *) {
    while (s_state.wait()) {
        // TODO: Sort messages by timestamp.
        for (auto *queue = s_state.queue_head(); queue != nullptr; queue = queue->next()) {
            uint32_t lost_message_count = queue->exchange_lost_message_count();
            if (lost_message_count != 0) {
                printf("lost %u log messages\n", lost_message_count);
            }
            while (auto *message = queue->dequeue()) {
                // TODO: Proper sink system (e.g. FileSink, StdoutSink).
                fwrite(reinterpret_cast<const char *>(message + 1), 1, message->length, stdout);
                fputc('\n', stdout);
                delete message;
            }
        }
    }
    return nullptr;
}

} // namespace

VULL_GLOBAL(Timer g_log_timer);

void logln(StringView message) {
    if (s_queue == nullptr) [[unlikely]] {
        s_queue = new LogQueue;
        s_state.add_queue(s_queue);
    }
    s_queue->enqueue(message);
    s_state.post();
}

void open_log() {
    s_state.open_sink();
}

void close_log() {
    s_state.close_sink();
}

void print(StringView line) {
    fwrite(line.data(), 1, line.length(), stdout);
}

void println(StringView line) {
    print(line);
    fputc('\n', stdout);
}

} // namespace vull

#include <vull/tasklet/Tasklet.hh>

#include <vull/support/Array.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Utility.hh>
#include <vull/tasklet/Semaphore.hh>

namespace vull {

void Tasklet::invoke() {
    if (m_semaphore && !m_semaphore->try_acquire()) {
        schedule(move(*this));
        return;
    }
    m_invoker(m_inline_storage.data());
    if (m_semaphore) {
        m_semaphore->release();
    }
}

} // namespace vull

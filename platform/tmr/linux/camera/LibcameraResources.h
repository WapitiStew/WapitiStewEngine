#ifndef WSE_LIBCAMERA_RESOURCES_H
#define WSE_LIBCAMERA_RESOURCES_H

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <sys/mman.h>
#include <utility>
#include <vector>

namespace wse::tmr::detail
{
// libcamera permits only one CameraManager. Leases share it across WSE enumeration,
// capability queries and sessions. Creation AND final destruction use the same lock;
// a weak_ptr alone would race a new constructor against the last owner's destructor.
template<class Manager> class LibcameraManagerLease final
{
    struct Registry
    {
        std::mutex mutex;
        std::unique_ptr<Manager> manager;
        std::size_t leases = 0;
    };
    static Registry& registry() { static Registry value; return value; }
    Manager* m_manager = nullptr;
public:
    LibcameraManagerLease() = default;
    LibcameraManagerLease(const LibcameraManagerLease&) = delete;
    ~LibcameraManagerLease() noexcept { reset(); }
    int acquire()
    {
        if(m_manager) return 0;
        auto& state = registry();
        std::lock_guard<std::mutex> lock(state.mutex);
        if(!state.manager)
        {
            auto manager = std::make_unique<Manager>();
            const int result = manager->start();
            if(result < 0) return result;
            state.manager = std::move(manager);
        }
        ++state.leases;
        m_manager = state.manager.get();
        return 0;
    }
    void reset() noexcept
    {
        if(!m_manager) return;
        auto& state = registry();
        std::lock_guard<std::mutex> lock(state.mutex);
        m_manager = nullptr;
        if(--state.leases == 0) state.manager.reset();
    }
    Manager* operator->() const noexcept { return m_manager; }
};

// Private, non-allocating rollback. Native cleanup must contain its own exceptions.
template<class Cleanup> class LibcameraRollback final
{
    Cleanup m_cleanup;
    bool m_armed = true;
public:
    explicit LibcameraRollback(Cleanup cleanup_in) : m_cleanup(std::move(cleanup_in)) {}
    LibcameraRollback(const LibcameraRollback&) = delete;
    ~LibcameraRollback() noexcept { if(m_armed) m_cleanup(); }
    void commit() noexcept { m_armed = false; }
};

template<class Prepare, class Start, class Queue, class Cleanup>
auto startLibcameraTransaction(Prepare prepare_in, Start start_in, Queue queue_in,
    Cleanup cleanup_in) -> decltype(prepare_in())
{
    LibcameraRollback rollback(std::move(cleanup_in));
    auto prepared = prepare_in();
    if(!prepared.succeeded()) return prepared;
    auto started = start_in();
    if(!started.succeeded()) return started;
    auto queued = queue_in();
    if(!queued.succeeded()) return queued;
    rollback.commit();
    return queued;
}

template<class Copy, class Requeue, class Cleanup>
auto readLibcameraTransaction(Copy copy_in, Requeue requeue_in, Cleanup cleanup_in)
    -> decltype(copy_in())
{
    using Result = decltype(copy_in());
    LibcameraRollback rollback(std::move(cleanup_in));
    auto frame = copy_in();
    auto returned = requeue_in();
    if(!returned.succeeded())
        return frame.succeeded() ? Result::failure(returned.error()) : std::move(frame);
    rollback.commit();
    return frame;
}

struct LibcameraMappingCalls
{
    decltype(&::mmap) map = &::mmap;
    decltype(&::munmap) unmap = &::munmap;
};

// Returns errno, without building a diagnostic while a mapping is owned.
inline int appendLibcameraPlane(std::vector<std::uint8_t>* const bytes_inout,
    const LibcameraMappingCalls& calls_in, const int fd_in,
    const std::size_t offset_in, const std::size_t length_in, const std::size_t used_in)
{
    auto& bytes = *bytes_inout;
    if(used_in > length_in || length_in == 0
        || offset_in > std::numeric_limits<std::size_t>::max() - length_in
        || used_in > bytes.max_size() - bytes.size()) return EOVERFLOW;
    if(used_in == 0) return 0;
    const auto length = offset_in + length_in;
    void* const mapped = calls_in.map(nullptr, length, PROT_READ, MAP_SHARED, fd_in, 0);
    if(mapped == MAP_FAILED) return errno;
    LibcameraRollback release([&]() noexcept { (void)calls_in.unmap(mapped, length); });
    const auto* data = static_cast<const std::uint8_t*>(mapped) + offset_in;
    bytes.insert(bytes.end(), data, data + used_in);
    // Explicit release makes an unmap failure observable on the success path.
    release.commit();
    return calls_in.unmap(mapped, length) == 0 ? 0 : errno;
}

// The adapter serializes these methods with its mutex. All completion-side operations
// are allocation-free; one slot per owned request bounds the ring and pending ledger.
template<class Request> class LibcameraRequests final
{
    std::vector<std::unique_ptr<Request>> m_owners;
    std::vector<Request*> m_ready;
    std::vector<bool> m_pending;
    std::size_t m_head = 0, m_count = 0, m_outstanding = 0;
    bool m_fault = false;
    std::size_t index(Request* request_in) const noexcept
    {
        const auto found = std::find_if(m_owners.begin(), m_owners.end(),
            [request_in](const auto& owner_in) { return owner_in.get() == request_in; });
        return static_cast<std::size_t>(found - m_owners.begin());
    }
public:
    void prepare(std::vector<std::unique_ptr<Request>> owners_in)
    {
        std::vector<Request*> ready(owners_in.size(), nullptr);
        std::vector<bool> pending(owners_in.size(), false);
        m_owners = std::move(owners_in);
        m_ready = std::move(ready); m_pending = std::move(pending);
        m_head = m_count = m_outstanding = 0; m_fault = false;
    }
    const auto& owners() const noexcept { return m_owners; }
    std::size_t outstanding() const noexcept { return m_outstanding; }
    bool empty() const noexcept { return m_count == 0; }
    bool fault() const noexcept { return m_fault; }
    bool queued(Request* request_in) noexcept
    {
        const auto slot = index(request_in);
        if(slot == m_owners.size() || m_pending[slot]) { m_fault = true; return false; }
        m_pending[slot] = true; ++m_outstanding; return true;
    }
    void rejected(Request* request_in) noexcept
    {
        const auto slot = index(request_in);
        if(slot < m_owners.size() && m_pending[slot])
        { m_pending[slot] = false; --m_outstanding; }
    }
    void complete(Request* request_in, const bool publish_in) noexcept
    {
        const auto slot = index(request_in);
        if(slot == m_owners.size() || !m_pending[slot]) { m_fault = true; return; }
        m_pending[slot] = false; --m_outstanding;
        if(!publish_in) return;
        if(m_count == m_ready.size()) { m_fault = true; return; }
        m_ready[(m_head + m_count) % m_ready.size()] = request_in; ++m_count;
    }
    Request* pop() noexcept
    {
        if(empty()) return nullptr;
        auto* request = m_ready[m_head];
        m_head = (m_head + 1) % m_ready.size(); --m_count; return request;
    }
    // Only a successful, non-disconnected native stop supplies this barrier.
    void quiesced() noexcept
    { std::fill(m_pending.begin(), m_pending.end(), false); m_outstanding = 0; }
    bool clear() noexcept
    {
        if(m_outstanding != 0) return false;
        m_owners.clear(); m_ready.clear(); m_pending.clear();
        m_head = m_count = 0; m_fault = false; return true;
    }
};
}
#endif

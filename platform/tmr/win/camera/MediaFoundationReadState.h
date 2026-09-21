#ifndef WSE_MEDIA_FOUNDATION_READ_STATE_H
#define WSE_MEDIA_FOUNDATION_READ_STATE_H
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <wrl/client.h>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace wse::tmr::detail
{
// The source reader retains its COM callback. This state never borrows a backend pointer,
// and closes its publication gate before the backend releases reader/runtime ownership.
class MediaFoundationReadState
{
    bool m_accepting = false, m_flushing = false, m_closed = false;
public:
    std::mutex mutex;
    std::condition_variable condition;
    Microsoft::WRL::ComPtr<IMFSample> sample;
    HRESULT status = S_OK;
    DWORD flags = 0;
    LONGLONG timestamp = 0;
    bool ready = false, request_in_flight = false;

    void reset() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(m_closed || m_flushing) return;
        sample.Reset(); status = S_OK; flags = 0; timestamp = 0;
        ready = request_in_flight = false; m_accepting = true;
    }
    void receive(const HRESULT status_in, const DWORD flags_in,
        const LONGLONG timestamp_in, IMFSample* const sample_in) noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(!m_accepting || m_closed) return;
        sample = sample_in; status = status_in; flags = flags_in; timestamp = timestamp_in;
        ready = true; request_in_flight = false; condition.notify_all();
    }
    bool beginFlush() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(m_closed) return false;
        m_accepting = false;
        sample.Reset(); ready = false;
        // Flush queued native samples even when the last requested sample already arrived.
        m_flushing = true;
        return m_flushing;
    }
    void flushed() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(!m_flushing || m_closed) return;
        request_in_flight = false; m_flushing = false;
        condition.notify_all();
    }
    bool waitFlushed(const std::chrono::milliseconds timeout_in)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return condition.wait_for(lock, timeout_in, [this]() { return !m_flushing || m_closed; })
            && !m_closed;
    }
    void close() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        m_closed = true; m_accepting = false; sample.Reset(); ready = false;
        condition.notify_all();
    }
};

struct MediaFoundationFlushResult
{
    HRESULT status;
    bool timed_out;
};
template<class Flush>
MediaFoundationFlushResult flushMediaFoundationReader(MediaFoundationReadState& state_inout,
    Flush flush_in, const std::chrono::milliseconds timeout_in)
{
    if(!state_inout.beginFlush()) return {MF_E_SHUTDOWN, false};
    const HRESULT status = flush_in();
    if(FAILED(status)) { state_inout.close(); return {status, false}; }
    if(!state_inout.waitFlushed(timeout_in))
    { state_inout.close(); return {S_OK, true}; }
    return {S_OK, false};
}
}
#endif

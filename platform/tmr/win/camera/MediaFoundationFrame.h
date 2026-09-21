#ifndef WSE_INTERNAL_MEDIA_FOUNDATION_FRAME_H
#define WSE_INTERNAL_MEDIA_FOUNDATION_FRAME_H

#include "../../../../core/tmr/camera/CameraBackend.h"
#include <mfobjects.h>
#include <limits>
#include <utility>

namespace wse::tmr::detail
{
// Borrows the COM owner held by the caller. A successful Lock creates exactly
// one Unlock obligation, including while frame allocation/error creation throws.
class MediaBufferLock final
{
    IMFMediaBuffer* m_buffer = nullptr;
public:
    explicit MediaBufferLock( IMFMediaBuffer& buffer_inout ) noexcept : m_buffer( &buffer_inout ) {}
    MediaBufferLock( const MediaBufferLock& ) = delete;
    MediaBufferLock& operator=( const MediaBufferLock& ) = delete;
    ~MediaBufferLock() noexcept { (void)this->unlock(); }
    HRESULT unlock() noexcept
    {
        auto* const buffer = std::exchange( this->m_buffer, nullptr );
        return buffer ? buffer->Unlock() : S_OK;
    }
};

inline CameraResult< sCameraFrame > copyMediaFoundationFrame(
    IMFMediaBuffer& buffer_inout, const sCameraFrameDescription& description_in,
    const std::uint64_t sequence_in, const LONGLONG timestamp_in )
{
    BYTE* bytes = nullptr;
    DWORD capacity = 0, length = 0;
    const HRESULT locked = buffer_inout.Lock( &bytes, &capacity, &length );
    if( FAILED( locked ) )
        return CameraResult< sCameraFrame >::failure( CameraError(
            eCameraErrorCategory::Backend, eCameraErrorCode::ReadFailed,
            "Media Foundation frame mapping failed.", locked ) );
    MediaBufferLock lease( buffer_inout );
    if( !bytes || length > capacity
        || timestamp_in > (std::numeric_limits< std::int64_t >::max)() / 100
        || timestamp_in < (std::numeric_limits< std::int64_t >::min)() / 100 )
        return CameraResult< sCameraFrame >::failure( CameraError(
            eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
            "Media Foundation returned invalid frame metadata." ) );
    sCameraFrame frame;
    frame.description = description_in;
    frame.data.assign( bytes, bytes + length );
    frame.sequence = sequence_in;
    frame.monotonic_timestamp_ns = timestamp_in * 100;
    if( !frame.valid() )
        return CameraResult< sCameraFrame >::failure( CameraError(
            eCameraErrorCategory::InputOutput, eCameraErrorCode::ReadFailed,
            "Media Foundation returned a truncated camera frame." ) );
    const HRESULT unlocked = lease.unlock();
    if( FAILED( unlocked ) )
        return CameraResult< sCameraFrame >::failure( CameraError(
            eCameraErrorCategory::Backend, eCameraErrorCode::ReadFailed,
            "Media Foundation frame unmapping failed.", unlocked ) );
    return CameraResult< sCameraFrame >::success( std::move( frame ) );
}
} // namespace wse::tmr::detail
#endif

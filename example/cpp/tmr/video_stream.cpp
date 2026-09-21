// @file video_stream.cpp
// @brief Receives a short video stream (10 frames) from the first enumerated web camera
//        through the push callback that runs on the session's worker thread, turning each
//        frame into a Core wse::Image_ inside the callback.
//
// Requires WSE_BUILD_TMR=ON and a physical USB/UVC camera at run time; a machine without
// one reports that through the log and exits without failing the build.

#include "example_camera_utility.h"

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace
{

//! How many frames the sample keeps before it stops the stream. Ten is enough to show that
//! delivery is repeated and that the sequence number advances, while still finishing in about a
//! third of a second on a thirty-frame-per-second device. Raising it only makes the run longer;
//! nothing in the API caps it.
constexpr int TARGET_FRAMES = 10;
//! Budget for the whole stream, not for one frame. The push model has no per-frame deadline of
//! its own the way readFrame() does, so this condition-variable wait is the only thing that
//! stops the sample hanging on a camera that starts but never delivers. Ten seconds is far more
//! than ten frames need, so exceeding it means something is genuinely wrong rather than slow.
constexpr std::chrono::seconds STREAM_TIMEOUT( 10 );

} // namespace

int main()
{
    using namespace wse::tmr;

    wse::registDefaultLog();

    // A machine with no camera exits 0 on purpose, so this sample runs anywhere; a failure
    // after a device was found keeps its own non-zero code.
    WebCamera camera;
    sCameraDeviceInfo device;
    sCameraStreamConfiguration configuration;
    if ( !tmr_example::openFirstCamera( &camera, &device, &configuration ) )
    {
        return device.valid() ? 2 : 0;
    }

    // Callbacks are serialized on a worker thread owned by the session, so the counters
    // below only need protection against the waiting main thread.
    // These three objects outlive the stream because stop() and close() run before main()
    // returns; a callback that captured a shorter-lived object by reference would be a defect.
    std::mutex mutex;
    std::condition_variable condition;
    int received = 0;
    int failed = 0;

    // Passing a callback to start() selects the push model: the session delivers frames on its
    // own thread instead of the caller pulling them with readFrame(). The callback is held by
    // the session until stop(), so everything it captures has to stay alive that long. It runs
    // on the capture path, so work done here delays the next frame; the sample keeps to a
    // conversion and a log line for that reason.
    const auto started = camera.start( [&]( const CameraResult< sCameraFrame >& frame_in )
    {
        // The lock is held for the whole callback, which is what makes the counters and the
        // notify one atomic step against the waiting main thread. It costs nothing in
        // contention because the session serializes its callbacks anyway.
        std::lock_guard< std::mutex > lock( mutex );
        // Delivery does not stop the moment the target is met: frames already in flight keep
        // arriving until stop() takes effect, and this drops them rather than overcounting.
        if ( received >= TARGET_FRAMES ) return;
        if ( frame_in.succeeded() )
        {
            // The frame owns its bytes, so converting it here is safe even though the
            // callback runs on the session's worker thread.
            // The reference is only valid for the duration of the call, though: a consumer
            // that wanted the frame later would copy the sCameraFrame, which copies its data.
            wse::img3c08_t image;
            if ( !tmr_example::toRgbImage( &image, frame_in.value() ) )
            {
                ++failed;
            }
            else
            {
                ++received;
                wse::WLog() << "frame" << received << "/" << TARGET_FRAMES << ":"
                            << "sequence=" << frame_in.value().sequence
                            << "timestamp_ns=" << frame_in.value().monotonic_timestamp_ns
                            << "image=" << image.width() << "x" << image.height()
                            << "bytes=" << image.image_memory_size();
            }
        }
        else
        {
            // A failure arrives through the same callback rather than through a return value,
            // so this is the only place a streaming error can be seen.
            ++failed;
            tmr_example::logCameraError( "stream callback", frame_in.error() );
        }
        // One failure wakes the main thread immediately: the sample gives up rather than
        // waiting out STREAM_TIMEOUT for frames that are not going to be right.
        if ( received >= TARGET_FRAMES || failed > 0 )
        {
            condition.notify_all();
        }
    } );
    if ( !started.succeeded() )
    {
        // No stream was ever running, so only the session from open() has to be released.
        tmr_example::logCameraError( "start(callback)", started.error() );
        camera.close();
        return 3;
    }

    // The predicate form is not decoration: it re-checks the counters after every wake, so a
    // notify that arrived before this thread reached the wait is not lost, and a spurious wake
    // does not end the sample early. Its result is the predicate, so false means the deadline
    // expired rather than the target being met.
    bool complete = false;
    {
        std::unique_lock< std::mutex > lock( mutex );
        complete = condition.wait_for( lock, STREAM_TIMEOUT,
            [&]() { return received >= TARGET_FRAMES || failed > 0; } );
    }

    // The lock is released before this, because stop() has to be able to finish whatever the
    // worker thread is doing and a callback blocked on the mutex could not. stop() ends the
    // streaming and close() ends the session; both are safe to repeat and the destructor would
    // do the same. The counters are read unlocked afterwards, once no further callback is due.
    camera.stop();
    camera.close();

    if ( !complete )
    {
        wse::WLog() << "ERROR: stream timed out," << received << "of" << TARGET_FRAMES
                    << "frames arrived";
        return 4;
    }
    if ( failed > 0 )
    {
        return 5;
    }
    wse::WLog() << "stream complete:" << received << "frames received";
    return 0;
}

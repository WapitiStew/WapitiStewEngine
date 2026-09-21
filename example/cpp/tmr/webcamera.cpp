// @file webcamera.cpp
// @brief Portable WebCamera enumeration, profile selection, and one-frame capture.
//
// This is the smallest complete WebCamera session: enumerate, choose a profile, open, start,
// read one frame, stop, close. It stays at the frame level and never reaches the Core image
// layer, so it is the file to read first; the other tmr samples add conversion, controls, and
// streaming on top of the same five calls.
//
// Requires WSE_BUILD_TMR=ON and a physical USB/UVC camera at run time. A machine without one
// exits 0, because the samples are expected to run where no camera is attached. Every other
// exit code marks a device that was found and then failed.
//
// Unlike the other samples in this directory, this one takes its error text from
// error().message() alone. A caller that has to act on a failure branches on category() and
// code() instead, as example_camera_utility.h does.

#include <tmr/stew.h>
#include <wse/stew.h>

int main()
{
    using namespace wse::tmr;

    wse::registDefaultLog();

    // The default backend argument lets the platform pick Media Foundation or V4L2. The list
    // holds USB/UVC devices only; CSI, network, and virtual cameras are filtered out.
    const auto devices = WebCamera::enumerate();
    if (!devices.succeeded())
    {
        wse::WLog() << "ERROR: camera enumeration failed:" << devices.error().message();
        return 1;
    }
    if (devices.value().empty())
    {
        wse::WLog() << "No USB/UVC web camera is attached.";
        return 0;
    }

    // Being enumerated does not make a device usable, so the loop asks each one for its
    // capabilities and keeps the first that advertises a profile with at least one output
    // format. capabilities() inspects a device without opening it, so nothing has to be closed
    // when a candidate is rejected. The pointer stays valid because it aims into the vector
    // held by `devices`, which outlives it.
    const sCameraDeviceInfo* selected_device = nullptr;
    sCameraStreamProfile selected_profile;
    for (const auto& device : devices.value())
    {
        const auto capability = WebCamera::capabilities(device);
        if (!capability.succeeded() || capability.value().stream_profiles.empty()) continue;
        const auto& profile = capability.value().stream_profiles.front();
        if (profile.output_formats.empty()) continue;
        selected_device = &device;
        selected_profile = profile;
        break;
    }
    if (selected_device == nullptr)
    {
        wse::WLog() << "ERROR: no enumerated camera has a usable stream profile.";
        return 2;
    }

    // The three fields are the native capture format, the format the consumer wants, and
    // allow_conversion. Taking the device's own first output format asks for the least work;
    // the trailing true still permits a conversion, and with false the device would have to
    // produce the output format itself or open() would fail.
    const sCameraStreamConfiguration configuration{
        selected_profile.native_format, selected_profile.output_formats.front(), true};

    // open() fixes the resolution for the session and start() begins the capture; neither on
    // its own produces a frame. Failing here leaks nothing, because the destructor stops and
    // closes whatever was reached.
    WebCamera camera;
    if (!camera.open(*selected_device, configuration).succeeded()) return 4;
    if (!camera.start().succeeded()) return 5;

    // The argument is a per-frame deadline in milliseconds: no frame within it is reported as a
    // Timeout-category failure rather than an endless wait. Two seconds allows for the slow
    // first frame a UVC device delivers after start(). This budgets frame waiting;
    // copying, native cleanup and stop do not have a strict wall-time bound.
    // A native buffer-return failure can close the session: reopen before retrying.
    // Libcamera copy/requeue exceptions stop the stream; check stop() before restarting.
    // A disconnected libcamera pipeline may leave close() waiting for pending requests.
    // The frame owns a copy of its pixel bytes, so the session is released before the result is
    // examined and the frame stays valid afterwards.
    const auto frame = camera.readFrame(2000U);
    const auto stopped = camera.stop();
    camera.close();
    if (!stopped.succeeded()) return 7;
    if (!frame.succeeded()) return 6;

    // data.size() is not width * height * channels in general: description.row_stride may carry
    // padding the device adds, and only a conversion to a wse::Image_ folds that away.
    wse::WLog() << frame.value().description.width << "x" << frame.value().description.height
                << "bytes=" << frame.value().data.size();
    return 0;
}

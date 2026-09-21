//*****************************************************************************************************************
//!
//! @file    VideoStreamExample.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese 連続10 FrameのPull読出とSequence、Timestamp差分の報告.
//! @brief   \~english  Pulling ten consecutive frames and reporting each sequence number and timestamp delta.
//!
//! @details
//!     \~japanese
//!     @n 引数順序は arguments[0] がNative Library Pathである。追加引数は取らない。
//!     @n Cameraが無い環境でも失敗しない。列挙結果が空であることを報告して終了する。
//!     @n C++版はSession Worker Thread上で動作するPush Callback版のstartを用いる。Bindingにも
//!        Start( Action ) があるが、本Sampleは順序と区間が読み取りやすいPull読出を使う。
//!        Frameの内容はどちらも同一である。
//!     \~english
//!     @n Argument order: arguments[0] is the native library path. The sample takes no further
//!        arguments.
//!     @n A machine with no camera does not fail: the sample reports the empty enumeration and
//!        stops.
//!     @n The C++ counterpart uses the push-callback overload of start, which runs on the session's
//!        worker thread. The binding offers Start( Action ) as well, but this sample pulls with
//!        ReadFrame because the ordering and the intervals read more plainly that way; the frames
//!        themselves are identical either way.
//!
//! @date
//!   Sep-06, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Collections.Generic;
using WapitiStew.Wse;

/// <summary>Ten consecutive frames read from the first camera with a usable stream profile.</summary>
public static class VideoStreamExample
{
    /// <summary>How many frames the sample reads.</summary>
    // Ten frames is under a second at any ordinary rate, yet enough intervals for the mean below
    // to mean something. A larger count measures the cadence more accurately at the cost of a
    // longer run; a count of one would leave no interval to measure at all.
    private const int TargetFrames = 10;

    /// <summary>How long one frame read may take.</summary>
    // This bounds one read, not the whole loop. Two seconds is generous against a frame interval
    // of tens of milliseconds; the slack is for the first read after Start, where the device is
    // still negotiating the stream. A deadline near the frame interval would report TimedOut on a
    // healthy camera, and every read after the first normally returns long before this expires.
    private static readonly TimeSpan ReadTimeout = TimeSpan.FromSeconds(2);

    /// <summary>Nanoseconds in one millisecond, used to report the timestamp deltas.</summary>
    // The frame timestamps are nanoseconds on the monotonic clock, so every printed figure is
    // scaled through this one constant rather than by a literal at each site.
    private const double NanosecondsPerMillisecond = 1000000.0;

    /// <summary>Reads ten frames and reports the sequence number and timestamp delta of each.</summary>
    /// <param name="arguments">arguments[0] is the optional path to the native library file.</param>
    /// <returns>Zero on success and on every machine without a usable camera.</returns>
    public static int Main(string[] arguments)
    {
        if (arguments.Length > 0)
        {
            WseRuntime.SetNativeLibraryPath(arguments[0]);
        }

        using var runtime = new WseRuntime();
        if (!runtime.Info().Components.HasTmr)
        {
            Console.Error.WriteLine("This WSE build does not include the Tmr component.");
            return 1;
        }

        // Every enumerated device is a native description the caller owns, which is why the whole
        // body sits in a try whose finally disposes each one.
        IReadOnlyList<CameraDevice> devices = WebCamera.Enumerate();
        try
        {
            // An empty list is the ordinary answer on a machine with no camera, not a failure.
            if (devices.Count == 0)
            {
                Console.WriteLine("No USB/UVC web camera is attached.");
                return 0;
            }

            // Capabilities are readable without opening the device, so the first camera offering
            // any output format can be chosen before one is claimed. Each capability set is a
            // native object of its own; the using releases it per iteration. selectedDevice only
            // borrows from the list, which keeps ownership.
            CameraDevice? selectedDevice = null;
            CameraStreamProfile selectedProfile = default;
            foreach (CameraDevice device in devices)
            {
                using CameraCapability capability = WebCamera.Capabilities(device);
                foreach (CameraStreamProfile profile in capability.StreamProfiles())
                {
                    if (profile.OutputFormats.Count == 0) continue;
                    selectedDevice = device;
                    selectedProfile = profile;
                    break;
                }
                if (selectedDevice is not null) break;
            }

            if (selectedDevice is null)
            {
                Console.WriteLine("No enumerated camera has a usable stream profile.");
                return 0;
            }

            Console.WriteLine($"device: {selectedDevice.DisplayName}");
            Console.WriteLine(
                $"profile: {selectedProfile.NativeFormat.Width}x{selectedProfile.NativeFormat.Height}"
                + $" @ {selectedProfile.NativeFormat.FramesPerSecond:0.##} fps");

            // The trailing true allows conversion, so Tmr may transcode the device's native format
            // into the requested output. Passing false would demand the device produce that format
            // directly and fail when it cannot.
            var configuration = new CameraStreamConfiguration(
                selectedProfile.NativeFormat, selectedProfile.OutputFormats[0], true);

            // The camera owns the native session. Start without a callback selects the pull mode
            // ReadFrame belongs to; the frames stay queued until they are read.
            using var camera = new WebCamera();
            try
            {
                camera.Open(selectedDevice, configuration);
                camera.Start();
                try
                {
                    ReadStream(camera);
                }
                finally
                {
                    // Streaming is stopped and the device released here rather than being left to
                    // Dispose, so it frees up as soon as the run is over even if a read threw.
                    camera.Stop();
                    camera.Close();
                }
                return 0;
            }
            catch (WseException failure)
            {
                // A camera that was present and then failed mid-session is a genuine error, unlike
                // the absent-hardware cases above, so it is reported through the exit code.
                Console.Error.WriteLine(
                    $"ERROR: the camera session failed. category={failure.Category}"
                    + $" code={failure.Code} native={failure.NativeCode} message={failure.Message}");
                return 3;
            }
        }
        finally
        {
            // Enumerate handed over one native description per device, so each is released here
            // whichever way the body above left.
            foreach (CameraDevice device in devices)
            {
                device.Dispose();
            }
        }
    }

    /// <summary>Reads the target number of frames and prints one line per frame plus a summary.</summary>
    /// <param name="camera">A camera that is already open and streaming.</param>
    private static void ReadStream(WebCamera camera)
    {
        // Each frame owns native pixels and is released at the end of its iteration, so the loop
        // holds one frame at a time however long it runs.
        // The timestamps come from the capture clock, not from the moment of the read, so the
        // deltas below measure the device's cadence and not the speed of this loop.
        long firstTimestamp = 0;
        long previousTimestamp = 0;
        for (int index = 0; index < TargetFrames; ++index)
        {
            using CameraFrame frame = camera.ReadFrame(ReadTimeout);
            // The first frame has no predecessor, so it seeds both marks and contributes a delta
            // of zero rather than a meaningless jump from an unset value.
            if (index == 0)
            {
                firstTimestamp = frame.MonotonicTimestampNs;
                previousTimestamp = frame.MonotonicTimestampNs;
            }

            long delta = frame.MonotonicTimestampNs - previousTimestamp;
            previousTimestamp = frame.MonotonicTimestampNs;
            Console.WriteLine(
                $"frame {index + 1}/{TargetFrames}: sequence={frame.Sequence}"
                + $" timestamp_ns={frame.MonotonicTimestampNs}"
                + $" delta_ms={delta / NanosecondsPerMillisecond:0.###}"
                + $" {frame.Description.Width}x{frame.Description.Height}"
                + $" bytes={frame.ToArray().Length}");
        }

        // Reported so a reader can see the fps actually delivered, which a device is free to lower
        // from the advertised rate under poor light or a saturated bus.
        // The first frame opens the window, so nine intervals separate ten frames.
        double elapsedMs = (previousTimestamp - firstTimestamp) / NanosecondsPerMillisecond;
        double averageMs = TargetFrames > 1 ? elapsedMs / (TargetFrames - 1) : 0.0;
        double measuredFps = averageMs > 0.0 ? 1000.0 / averageMs : 0.0;
        Console.WriteLine(
            $"stream complete: {TargetFrames} frames received over {elapsedMs:0.###} ms"
            + $" (mean interval {averageMs:0.###} ms, {measuredFps:0.##} fps)");
    }
}

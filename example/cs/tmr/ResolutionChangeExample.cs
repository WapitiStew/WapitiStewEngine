//*****************************************************************************************************************
//!
//! @file    ResolutionChangeExample.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Close／再Openによる2つのStream ProfileでのFrame取得.
//! @brief   \~english  Capturing one frame at each of two stream profiles by closing and reopening.
//!
//! @details
//!     \~japanese
//!     @n 引数順序は arguments[0] がNative Library Pathである。追加引数は取らない。
//!     @n 解像度はOpen時に固定される。変更にはCloseと別Profileでの再Openが必要である。
//!     @n Cameraが無い環境でも失敗しない。列挙結果が空であることを報告して終了する。
//!     @n C++版はBMPを保存するが、BindingにImage Writerが無い。代わりに両Frameの寸法と
//!        Byte数を出力する。
//!     \~english
//!     @n Argument order: arguments[0] is the native library path. The sample takes no further
//!        arguments.
//!     @n The resolution is fixed at open, so changing it means closing the session and reopening it
//!        with a second stream profile.
//!     @n A machine with no camera does not fail: the sample reports the empty enumeration and
//!        stops.
//!     @n The C++ counterpart saves each capture as a BMP. The binding exposes no image writer, so
//!        this sample reports each frame's size and byte count instead.
//!
//! @date
//!   Sep-06, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Collections.Generic;
using System.Linq;
using WapitiStew.Wse;

/// <summary>One frame at each of two stream profiles of the same camera.</summary>
public static class ResolutionChangeExample
{
    /// <summary>How long one frame read may take.</summary>
    // Both captures here are the first frame of a fresh session, which is the slowest kind: the
    // device has just been opened and is still negotiating the stream. Two seconds absorbs that
    // where a deadline near the frame interval would report TimedOut on a healthy camera.
    private static readonly TimeSpan ReadTimeout = TimeSpan.FromSeconds(2);

    /// <summary>Captures at the first profile, reopens at a different one, and captures again.</summary>
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

            // One device is enough here, because the point is two profiles of the same camera
            // rather than a choice between cameras. It stays owned by the list.
            CameraDevice device = devices[0];
            Console.WriteLine($"device: {device.DisplayName}");

            // Two profiles with different frame sizes; both have to offer a usable output format.
            // Only the frame size is compared, because a profile that differs solely in frame rate
            // or in native format would not demonstrate a resolution change. The whole search runs
            // inside one capability scope so the native capability set is released before the
            // device is opened.
            CameraStreamConfiguration first = default;
            CameraStreamConfiguration second = default;
            bool firstFound = false;
            bool secondFound = false;
            using (CameraCapability capability = WebCamera.Capabilities(device))
            {
                foreach (CameraStreamProfile profile in capability.StreamProfiles())
                {
                    if (profile.OutputFormats.Count == 0) continue;
                    if (!firstFound)
                    {
                        first = new CameraStreamConfiguration(
                            profile.NativeFormat, profile.OutputFormats[0], true);
                        firstFound = true;
                        continue;
                    }
                    bool differentSize =
                        profile.NativeFormat.Width != first.NativeFormat.Width ||
                        profile.NativeFormat.Height != first.NativeFormat.Height;
                    if (!differentSize) continue;

                    // Keep the first output format when the second profile can also produce it.
                    // Holding the format steady leaves the frame size as the only difference
                    // between the two captures, which is what the printed comparison is about.
                    CameraPixelFormat output = profile.OutputFormats.Contains(first.OutputFormat)
                        ? first.OutputFormat
                        : profile.OutputFormats[0];
                    second = new CameraStreamConfiguration(profile.NativeFormat, output, true);
                    secondFound = true;
                    break;
                }
            }

            if (!firstFound)
            {
                Console.WriteLine("No enumerated camera has a usable stream profile.");
                return 0;
            }

            // One camera object serves both sessions. Close ends a session without ending the
            // object, so the same instance opens again with the second configuration; only Dispose
            // releases the native camera, and the using does that once at the end.
            using var camera = new WebCamera();
            try
            {
                Console.WriteLine($"first resolution: {Describe(first.NativeFormat)}");
                string firstResult = CaptureOnce(camera, device, first);
                Console.WriteLine($"first capture: {firstResult}");

                if (!secondFound)
                {
                    Console.WriteLine(
                        "the camera advertises only one resolution; nothing to change");
                    return 0;
                }

                Console.WriteLine($"second resolution: {Describe(second.NativeFormat)}");
                string secondResult = CaptureOnce(camera, device, second);
                Console.WriteLine($"second capture: {secondResult}");
                Console.WriteLine(
                    $"captured both resolutions: {Describe(first.NativeFormat)}"
                    + $" and {Describe(second.NativeFormat)}");
                return 0;
            }
            catch (WseException failure)
            {
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
            foreach (CameraDevice item in devices)
            {
                item.Dispose();
            }
        }
    }

    /// <summary>Opens the device with one configuration, reads a frame, and closes again.</summary>
    /// <param name="camera">The camera owner reused across both sessions.</param>
    /// <param name="device">The device to open.</param>
    /// <param name="configuration">Native format, output format, and conversion policy.</param>
    /// <returns>A one-line description of the captured frame.</returns>
    private static string CaptureOnce(
        WebCamera camera, CameraDevice device, CameraStreamConfiguration configuration)
    {
        camera.Open(device, configuration);
        camera.Start();
        try
        {
            // The frame owns native pixels; the using releases them once the description has been
            // formatted. ToArray copies the bytes out, so its length is the real payload size and
            // reflects the resolution just opened.
            using CameraFrame frame = camera.ReadFrame(ReadTimeout);
            return $"{frame.Description.Width}x{frame.Description.Height}"
                + $" format={frame.Description.PixelFormat} stride={frame.Description.RowStride}"
                + $" bytes={frame.ToArray().Length}";
        }
        finally
        {
            // Close ends the session and is idempotent, and it releases the device before the next
            // open. It does not end the camera object: this same instance is opened again by the
            // second call, which is the whole mechanism the sample demonstrates.
            camera.Stop();
            camera.Close();
        }
    }

    /// <summary>Formats a stream format as its pixel dimensions.</summary>
    /// <param name="format">The format to describe.</param>
    /// <returns>The formatted size.</returns>
    private static string Describe(CameraFormat format) => $"{format.Width}x{format.Height}";
}

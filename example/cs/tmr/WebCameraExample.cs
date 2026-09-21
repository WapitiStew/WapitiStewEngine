//*****************************************************************************************************************
//!
//! @file    WebCameraExample.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Portable なWebCamera列挙、Profile選択および1 Frame取得.
//! @brief   \~english  Portable WebCamera enumeration, profile selection, and one-frame capture.
//!
//! @details
//!     \~japanese
//!     @n Cameraが無い環境でも失敗しない。列挙結果が空であることを報告して終了する。
//!     @n Media Foundation、V4L2、libcameraのHandleへは触れない。
//!     @n 引数順序は arguments[0] がNative Library Pathである。追加引数は取らない。
//!     @n Cameraが1台も無い場合は0を返すが、列挙できたCameraに使えるProfileが無い場合は2を返す。
//!        後者はHardwareの不在ではなく、Deviceの申告と本Sampleの要求が噛み合わない状態である。
//!     @n Callback版へ変更する場合も受信Frameをusing等で解放し、外部OwnerのStop後に
//!        CallbackExceptionを確認する。Callback内ではCameraをClose／Disposeしない。
//!     \~english
//!     @n A machine with no camera does not fail: the sample reports the empty enumeration and
//!        stops. It never touches a Media Foundation, V4L2, or libcamera handle.
//!     @n Argument order: arguments[0] is the native library path. The sample takes no further
//!        arguments.
//!     @n No camera at all returns zero, but an enumerated camera with no usable stream profile
//!        returns 2. That second case is not absent hardware; it is a device whose advertised
//!        profiles do not meet what this sample asks for, and it deserves to be noticed.
//!     @n When adapting this to Start(callback), dispose each received frame, stop from an
//!        external owner, then inspect CallbackException. Do not Close/Dispose inside the callback.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Collections.Generic;
using WapitiStew.Wse;

/// <summary>Portable WebCamera enumeration, profile selection, and one-frame capture.</summary>
public static class WebCameraExample
{
    /// <summary>Captures one frame from the first camera with a usable stream profile.</summary>
    /// <param name="arguments">Optional path to the native library file.</param>
    /// <returns>Zero on success.</returns>
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
        // body sits in a try whose finally disposes each one. The native list itself was already
        // released inside Enumerate.
        IReadOnlyList<CameraDevice> devices = WebCamera.Enumerate();
        try
        {
            // An empty list is the ordinary answer on a machine with no camera, not a failure, so
            // the sample says so and succeeds. This is what lets it run on a build machine.
            if (devices.Count == 0)
            {
                Console.WriteLine("No USB/UVC web camera is attached.");
                return 0;
            }

            // Capabilities can be read without opening the device, so a machine with several
            // cameras can be surveyed before one is claimed. Each capability set is its own native
            // object; the using releases it at the end of each iteration.
            // selectedDevice only borrows from the list above, which keeps ownership, so it must
            // not be disposed here.
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
                Console.Error.WriteLine("No enumerated camera has a usable stream profile.");
                return 2;
            }

            Console.WriteLine($"device: {selectedDevice.DisplayName}");
            Console.WriteLine(
                $"profile: {selectedProfile.NativeFormat.Width}x{selectedProfile.NativeFormat.Height}"
                + $" @ {selectedProfile.NativeFormat.FramesPerSecond:0.##} fps");

            // The first advertised output format is taken because any of them will do here; a real
            // caller picks the one its pipeline wants. The trailing true allows conversion, so Tmr
            // may transcode the device's native format into that output. Passing false would
            // demand the device produce it directly and fail when it cannot.
            var configuration = new CameraStreamConfiguration(
                selectedProfile.NativeFormat, selectedProfile.OutputFormats[0], true);

            // The camera is the single RAII owner; disposing stops streaming and closes the device.
            // Open fixes the resolution and the format for the whole session: changing either
            // means closing and opening again.
            using var camera = new WebCamera();
            camera.Open(selectedDevice, configuration);
            camera.Start();
            try
            {
                // Two seconds is generous against a frame interval of tens of milliseconds. The
                // slack is for the first frame after Start, where a UVC device still has to
                // negotiate the stream and settle its automatic exposure. A deadline near the
                // frame interval would report TimedOut on a perfectly healthy camera.
                // The frame owns native pixels, so it is disposed as soon as it has been read;
                // ToArray copies those pixels into managed memory.
                CameraFrame frame = camera.ReadFrame(TimeSpan.FromSeconds(2));
                using (frame)
                {
                    Console.WriteLine(
                        $"{frame.Description.Width}x{frame.Description.Height}"
                        + $" bytes={frame.ToArray().Length}");
                }
            }
            finally
            {
                // The device is released here rather than being left to Dispose, so it is free for
                // another process the moment the capture is over. Both calls are safe to repeat.
                camera.Stop();
                camera.Close();
            }
            return 0;
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
}

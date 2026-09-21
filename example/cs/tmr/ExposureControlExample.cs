//*****************************************************************************************************************
//!
//! @file    ExposureControlExample.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Exposure Capabilityの確認とManual書込による前後1 Frameずつの比較.
//! @brief   \~english  Reading the exposure capability and comparing one frame before and after a manual write.
//!
//! @details
//!     \~japanese
//!     @n 引数順序は arguments[0] がNative Library Pathである。追加引数は取らない。
//!     @n Cameraが無い環境でも失敗しない。列挙結果が空であることを報告して終了する。
//!     @n C++版はBMPを保存するが、BindingにImage Writerが無い。代わりに前後Frameの記述と
//!        Sample平均値を出力し、変化を可視化する。
//!     @n C++版の physicalFromValue／valueFromNormalized はBindingに無い。Capabilityの
//!        Minimum、MaximumおよびStepから目標値を算出する。
//!     \~english
//!     @n Argument order: arguments[0] is the native library path. The sample takes no further
//!        arguments.
//!     @n A machine with no camera does not fail: the sample reports the empty enumeration and
//!        stops.
//!     @n The C++ counterpart saves a BMP on either side of the write. The binding exposes no image
//!        writer, so this sample reports each frame's description and mean sample value instead,
//!        which is what makes the exposure change visible in the output.
//!     @n The C++ helpers physicalFromValue and valueFromNormalized are not bound, so the target
//!        value is derived here from the capability's Minimum, Maximum, and Step.
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

/// <summary>Exposure capability report and a manual exposure change with a frame on either side.</summary>
public static class ExposureControlExample
{
    /// <summary>How long one frame read may take.</summary>
    // This bounds one read, not the session. Two seconds is generous against a frame interval of
    // tens of milliseconds; the slack is for the first read after Start, where the device is still
    // negotiating the stream. Note that a long exposure written below lengthens the frame interval
    // itself, so a deadline close to the advertised rate could expire on a healthy camera.
    private static readonly TimeSpan ReadTimeout = TimeSpan.FromSeconds(2);

    /// <summary>Frames discarded after the write so the sensor settles on the new exposure.</summary>
    // A UVC device applies a control change over the next few frames rather than instantly, so the
    // frames captured immediately after the write may still carry the old exposure. Discarding
    // five is what makes the before-and-after comparison honest. Fewer risks measuring the old
    // setting; more only lengthens the run.
    private const int SettleFrames = 5;

    /// <summary>Reports the exposure range and captures one frame on either side of a write.</summary>
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
            // borrows from the list, which keeps ownership. Note that this is the stream
            // capability: whether the device has an exposure control at all is only knowable once
            // it is open, and that question is asked inside RunSession.
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

            // The camera is the single RAII owner; disposing stops streaming and closes the device.
            // Controls are only reachable while a device is open, so the whole exposure exchange
            // has to live inside this scope.
            using var camera = new WebCamera();
            try
            {
                camera.Open(selectedDevice, configuration);
                camera.Start();
                try
                {
                    return RunSession(camera);
                }
                finally
                {
                    // Runs even when RunSession threw part-way through, so the device is always
                    // released rather than left claimed by this process.
                    camera.Stop();
                    camera.Close();
                }
            }
            catch (WseException failure)
            {
                // A camera that was present and then failed is a genuine error, unlike the
                // absent-hardware cases above, so it is reported through the exit code.
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

    /// <summary>Reads the exposure capability, writes a new value, and captures a frame each side.</summary>
    /// <param name="camera">A camera that is already open and streaming.</param>
    /// <returns>Zero on success.</returns>
    private static int RunSession(WebCamera camera)
    {
        // ControlCapability carries the valid range, step, unit, and access. A device that has no
        // exposure control reports that as a structured error rather than as a sample defect.
        // The capability is queried before anything is written, because Minimum, Maximum, and Step
        // are what make a valid target computable; a value invented without them is refused by the
        // device. The values are device-native, and PhysicalScale is the multiplier onto Unit.
        CameraControlCapability exposure;
        try
        {
            exposure = camera.ControlCapability(CameraControl.Exposure);
        }
        catch (WseException failure)
        {
            Console.WriteLine(
                $"this camera exposes no exposure control: code={failure.Code}"
                + $" message={failure.Message}");
            return 0;
        }

        Console.WriteLine(
            $"exposure capability: range=[{exposure.Minimum},{exposure.Maximum}]"
            + $" step={exposure.Step} default={exposure.DefaultValue} unit={exposure.Unit}"
            + $" scale={exposure.PhysicalScale:0.####} manual={exposure.SupportsManual}"
            + $" readable={exposure.Readable} writable={exposure.Writable}");

        // Both guards are ordinary outcomes on real hardware, so both return zero: a camera that
        // only reports its exposure, or that offers a single value, has nothing to demonstrate but
        // is not broken.
        if (!exposure.Writable || !exposure.SupportsManual)
        {
            Console.WriteLine("this camera does not accept a manual exposure; nothing to change");
            return 0;
        }
        if (exposure.Maximum <= exposure.Minimum)
        {
            Console.WriteLine("the exposure range holds a single value; nothing to change");
            return 0;
        }

        // Read before anything is written. The value and the mode together are what the restore at
        // the end puts back, so a camera left in Automatic returns to Automatic and not merely to
        // the same number under Manual.
        CameraControlValue original = camera.GetControl(CameraControl.Exposure);
        Console.WriteLine(
            $"exposure before: value={original.Value} mode={original.Mode}");

        // One frame with the device's current exposure.
        // It is captured inside its own using so the native pixels are gone before the next read;
        // the two frames never have to be held at once. Where the C++ counterpart would write this
        // out as a BMP, the mean below stands in for the picture.
        using (CameraFrame before = camera.ReadFrame(ReadTimeout))
        {
            byte[] bytes = before.ToArray();
            Console.WriteLine(
                $"frame before: {Describe(before)} bytes={bytes.Length}"
                + $" mean_sample={MeanSample(bytes):0.###}");
        }

        // Manual is required for the write to stick: under Automatic the device would override the
        // value at the next frame, and the two mean samples would come out the same.
        long target = TargetValue(exposure, original.Value);
        camera.SetControl(
            new CameraControlValue(CameraControl.Exposure, CameraControlMode.Manual, target));
        Console.WriteLine($"exposure after: value={target} mode={CameraControlMode.Manual}");

        // Each discarded frame is disposed as it is read, so the settle loop holds nothing.
        for (int skipped = 0; skipped < SettleFrames; ++skipped)
        {
            using CameraFrame discarded = camera.ReadFrame(ReadTimeout);
        }

        // One frame with the new exposure. Comparing the two mean sample values shows the change.
        using (CameraFrame after = camera.ReadFrame(ReadTimeout))
        {
            byte[] bytes = after.ToArray();
            Console.WriteLine(
                $"frame after: {Describe(after)} bytes={bytes.Length}"
                + $" mean_sample={MeanSample(bytes):0.###}");
        }

        // Restore the setting the device had before this sample ran.
        // The control is device state, not session state, so it would outlive this process and be
        // inherited by whatever opens the camera next.
        camera.SetControl(original);
        Console.WriteLine("restored the original exposure");
        return 0;
    }

    /// <summary>Picks a valid exposure value that differs from the current one.</summary>
    /// <param name="capability">The advertised exposure capability.</param>
    /// <param name="current">The value the device holds now.</param>
    /// <returns>A value inside the advertised range, snapped to the advertised step.</returns>
    // A quarter of the way up the range is far enough from a typical current setting to move the
    // picture visibly, while staying clear of the extremes, where a device is most likely to clamp
    // the request or behave oddly. This is the arithmetic the C++ valueFromNormalized helper does;
    // it is not bound, so it is spelled out here.
    private static long TargetValue(CameraControlCapability capability, long current)
    {
        long span = capability.Maximum - capability.Minimum;
        long target = Snap(capability, capability.Minimum + (long)(span * 0.25));
        // Writing the value the device already holds would change nothing and make the comparison
        // meaningless, so a camera already sitting at the quarter mark is moved to three quarters.
        if (target == current)
        {
            target = Snap(capability, capability.Minimum + (long)(span * 0.75));
        }
        return target;
    }

    /// <summary>Clamps a value into the advertised range and onto the advertised step.</summary>
    /// <param name="capability">The advertised exposure capability.</param>
    /// <param name="value">The wanted value.</param>
    /// <returns>The nearest value the device accepts at or below <paramref name="value"/>.</returns>
    private static long Snap(CameraControlCapability capability, long value)
    {
        // The accepted values are Minimum plus whole steps, not multiples of Step, so the
        // remainder is taken from the distance above Minimum. Rounding down keeps the result
        // inside the range rather than one step past Maximum.
        long snapped = value;
        if (capability.Step > 1)
        {
            snapped -= (snapped - capability.Minimum) % capability.Step;
        }
        // Clamped last, because a device is entitled to advertise a Maximum that is not itself a
        // whole number of steps above Minimum.
        if (snapped < capability.Minimum) snapped = capability.Minimum;
        if (snapped > capability.Maximum) snapped = capability.Maximum;
        return snapped;
    }

    /// <summary>Formats one frame's description and identity.</summary>
    /// <param name="frame">The frame to describe.</param>
    /// <returns>The formatted description.</returns>
    private static string Describe(CameraFrame frame) =>
        $"{frame.Description.Width}x{frame.Description.Height}"
        + $" format={frame.Description.PixelFormat} stride={frame.Description.RowStride}"
        + $" sequence={frame.Sequence}";

    /// <summary>Averages the raw frame bytes, which tracks the exposure on any byte-wide format.</summary>
    /// <param name="bytes">The frame bytes.</param>
    /// <returns>The mean byte value, or zero for an empty frame.</returns>
    // This is the stand-in for the BMP pair the C++ counterpart writes, since no image writer is
    // bound. Raw bytes are averaged without regard to the pixel layout, which is enough because
    // only the two figures relative to each other matter. On a sixteen-bit format the high and low
    // bytes are averaged together, so the number is comparable between the frames but is not a
    // true sample mean.
    private static double MeanSample(byte[] bytes)
    {
        if (bytes.Length == 0)
        {
            return 0.0;
        }
        long total = 0;
        foreach (byte sample in bytes)
        {
            total += sample;
        }
        return (double)total / bytes.Length;
    }
}

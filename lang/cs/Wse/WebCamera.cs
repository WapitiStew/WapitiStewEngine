//*****************************************************************************************************************
//!
//! @file    WebCamera.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Tmrが公開する唯一のCamera Owner.
//! @brief   \~english  The sole public camera owner exposed by Tmr.
//!
//! @details
//!     \~japanese
//!     @n Media Foundation、V4L2、libcameraのHandleは公開しない。列挙、Profile選択、Open、
//!        Start／Stop、Frame取得、Control、Extension UnitおよびCloseを提供する。
//!     @n Closeは冪等にSessionを終了し、再Openできる。DisposeはOwnerを終端的に解放する。
//!     \~english
//!     @n No Media Foundation, V4L2, or libcamera handle is exposed. The surface covers
//!        enumeration, profile selection, open, start/stop, frames, controls, extension units,
//!        and close.
//!     @n Close ends the session idempotently and permits reopening. Dispose releases the owner terminally.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace WapitiStew.Wse;

/// <summary>Owns one native Tmr handle and releases it exactly once.</summary>
internal sealed class CameraHandle : SafeHandle
{
    private readonly Action<IntPtr> _release;
    internal NativeMethods.CameraFrameCallback? CallbackRoot;

    internal CameraHandle(Action<IntPtr> release)
        : base(IntPtr.Zero, ownsHandle: true)
    {
        _release = release;
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Attach(IntPtr value) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        _release(handle);
        GC.KeepAlive(CallbackRoot);
        CallbackRoot = null;
        return true;
    }
}

/// <summary>One camera as reported by <see cref="WebCamera.Enumerate"/>.</summary>
public sealed class CameraDevice : IDisposable
{
    private readonly CameraHandle _handle;

    internal CameraDevice(NativeOutput output)
    {
        _handle = new CameraHandle(NativeMethods.wse_capi_camera_device_destroy);
        _handle.Attach(output.Take());
    }

    /// <summary>Gets the backend the device belongs to.</summary>
    public CameraBackend Backend
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_device_backend(Handle, out int backend));
            return (CameraBackend)backend;
        }
    }

    /// <summary>Gets how the device is attached to the machine.</summary>
    public CameraTransport TransportType
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_device_transport_type(Handle, out int transport));
            return (CameraTransport)transport;
        }
    }

    /// <summary>Gets the stable device identifier.</summary>
    public string Id => ReadString(NativeMethods.wse_capi_camera_device_id);

    /// <summary>Gets the human-readable device name.</summary>
    public string DisplayName => ReadString(NativeMethods.wse_capi_camera_device_display_name);

    /// <summary>Gets the device transport description.</summary>
    public string Transport => ReadString(NativeMethods.wse_capi_camera_device_transport);

    /// <summary>Gets the USB identity of the device.</summary>
    public CameraUsbIdentity Usb
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_device_usb(Handle, out NativeCameraUsbIdentity usb));
            string serial = ReadString(NativeMethods.wse_capi_camera_device_usb_serial_number);
            return new CameraUsbIdentity(
                usb.VendorId, usb.ProductId, usb.UvcVersionBcd, serial, usb.Available != 0);
        }
    }

    /// <summary>Reads what this device supports.</summary>
    /// <returns>The capability set. Dispose it when finished.</returns>
    public CameraCapability Capabilities()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_camera_capability_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_capabilities(Handle, out output.Value));
        return new CameraCapability(output);
    }

    /// <summary>Releases the native device description.</summary>
    public void Dispose() => _handle.Dispose();

    internal SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(CameraDevice));
            }
            return _handle;
        }
    }

    private delegate NativeStatus DeviceStringGetter(
        SafeHandle device, byte[]? buffer, out nuint size, nuint capacity);

    private string ReadString(DeviceStringGetter getter)
    {
        SafeHandle raw = Handle;
        return TransportMarshal.ReadString((byte[]? buffer, nuint capacity, out nuint size) =>
            getter(raw, buffer, out size, capacity));
    }
}

/// <summary>One extension-unit selector advertised by a device.</summary>
public sealed class CameraExtensionUnitSelector : IDisposable
{
    private readonly CameraHandle _handle;

    internal CameraExtensionUnitSelector(NativeOutput output)
    {
        _handle = new CameraHandle(NativeMethods.wse_capi_camera_xu_selector_destroy);
        _handle.Attach(output.Take());
        try
        {
            NativeOutput.Checkpoint("selector-metadata");
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_xu_selector_describe(
                    _handle, out byte unitId, out byte selector,
                    out nuint minimumSize, out nuint maximumSize,
                    out int readable, out int writable));
            UnitId = unitId;
            Selector = selector;
            MinimumSize = checked((int)minimumSize);
            MaximumSize = checked((int)maximumSize);
            Readable = readable != 0;
            Writable = writable != 0;
        }
        catch
        {
            _handle.Dispose();
            throw;
        }
    }

    /// <summary>Gets the extension unit identifier.</summary>
    public int UnitId { get; }

    /// <summary>Gets the selector within the unit.</summary>
    public int Selector { get; }

    /// <summary>Gets the smallest accepted payload size in bytes.</summary>
    public int MinimumSize { get; }

    /// <summary>Gets the largest accepted payload size in bytes.</summary>
    public int MaximumSize { get; }

    /// <summary>Gets whether the selector can be read.</summary>
    public bool Readable { get; }

    /// <summary>Gets whether the selector can be written.</summary>
    public bool Writable { get; }

    /// <summary>Copies the 16-byte unit GUID.</summary>
    /// <returns>The unit GUID bytes.</returns>
    public byte[] Guid()
    {
        byte[] guid = new byte[16];
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_xu_selector_guid(Handle, guid, (nuint)guid.Length));
        return guid;
    }

    /// <summary>Gets the human-readable selector name.</summary>
    public string DisplayName
    {
        get
        {
            SafeHandle raw = Handle;
            return TransportMarshal.ReadString(
                (byte[]? buffer, nuint capacity, out nuint size) =>
                    NativeMethods.wse_capi_camera_xu_selector_display_name(
                        raw, buffer, out size, capacity));
        }
    }

    /// <summary>Releases the native selector.</summary>
    public void Dispose() => _handle.Dispose();

    internal SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(CameraExtensionUnitSelector));
            }
            return _handle;
        }
    }
}

/// <summary>Everything one device supports: formats, stream profiles, controls, extension units.</summary>
public sealed class CameraCapability : IDisposable
{
    private readonly CameraHandle _handle;

    internal CameraCapability(NativeOutput output)
    {
        _handle = new CameraHandle(NativeMethods.wse_capi_camera_capability_destroy);
        _handle.Attach(output.Take());
    }

    /// <summary>Reads the device this capability set describes.</summary>
    /// <returns>An owned device description. Dispose it when finished.</returns>
    public CameraDevice Device()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_camera_device_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_capability_device(Handle, out output.Value));
        return new CameraDevice(output);
    }

    /// <summary>Reads every advertised stream format.</summary>
    /// <returns>The supported formats.</returns>
    public IReadOnlyList<CameraFormat> Formats()
    {
        SafeHandle raw = Handle;
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_capability_format_count(raw, out nuint count));
        var formats = new List<CameraFormat>(checked((int)count));
        for (nuint index = 0; index < count; ++index)
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_capability_format_at(
                    raw, out NativeCameraFormat format, index));
            formats.Add(CameraFormat.FromNative(format));
        }
        return formats;
    }

    /// <summary>Reads every advertised stream profile.</summary>
    /// <returns>The stream profiles and the output formats each can produce.</returns>
    public IReadOnlyList<CameraStreamProfile> StreamProfiles()
    {
        SafeHandle raw = Handle;
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_capability_profile_count(raw, out nuint count));
        var profiles = new List<CameraStreamProfile>(checked((int)count));
        for (nuint index = 0; index < count; ++index)
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_capability_profile_native_format(
                    raw, out NativeCameraFormat format, index));
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_capability_profile_output_count(
                    raw, out nuint outputCount, index));

            var outputs = new List<CameraPixelFormat>(checked((int)outputCount));
            for (nuint output = 0; output < outputCount; ++output)
            {
                NativeMethods.ThrowIfFailed(
                    NativeMethods.wse_capi_camera_capability_profile_output_at(
                        raw, out int pixelFormat, index, output));
                outputs.Add((CameraPixelFormat)pixelFormat);
            }
            profiles.Add(new CameraStreamProfile(CameraFormat.FromNative(format), outputs));
        }
        return profiles;
    }

    /// <summary>Reads every advertised control capability.</summary>
    /// <returns>The control capabilities.</returns>
    public IReadOnlyList<CameraControlCapability> Controls()
    {
        SafeHandle raw = Handle;
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_capability_control_count(raw, out nuint count));
        var controls = new List<CameraControlCapability>(checked((int)count));
        for (nuint index = 0; index < count; ++index)
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_capability_control_at(
                    raw, out NativeCameraControlCapability control, index));
            nuint current = index;
            string displayName = TransportMarshal.ReadString(
                (byte[]? buffer, nuint capacity, out nuint size) =>
                    NativeMethods.wse_capi_camera_capability_control_display_name(
                        raw, buffer, out size, current, capacity));
            controls.Add(CameraControlCapability.FromNative(control, displayName));
        }
        return controls;
    }

    /// <summary>Reads every advertised extension-unit selector.</summary>
    /// <returns>The selectors. Dispose each when finished.</returns>
    public IReadOnlyList<CameraExtensionUnitSelector> ExtensionUnits()
    {
        SafeHandle raw = Handle;
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_capability_extension_unit_count(raw, out nuint count));
        return NativeOutput.Collect(checked((int)count), index =>
        {
            using var output = new NativeOutput(NativeMethods.wse_capi_camera_xu_selector_destroy);
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_capability_extension_unit_at(
                    raw, out output.Value, (nuint)index));
            return new CameraExtensionUnitSelector(output);
        });
    }

    /// <summary>Releases the native capability set.</summary>
    public void Dispose() => _handle.Dispose();

    internal SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(CameraCapability));
            }
            return _handle;
        }
    }
}

/// <summary>One captured frame. Dispose it to release the native pixels.</summary>
public sealed class CameraFrame : IDisposable
{
    private readonly CameraHandle _handle;

    internal CameraFrame(NativeOutput output)
    {
        _handle = new CameraHandle(NativeMethods.wse_capi_camera_frame_destroy);
        _handle.Attach(output.Take());
        try
        {
            NativeOutput.Checkpoint("frame-metadata");
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_frame_describe(
                    _handle, out NativeCameraFrameDescription description,
                    out ulong sequence, out long timestamp));
            Description = new CameraFrameDescription(
                (int)description.Width,
                (int)description.Height,
                (CameraPixelFormat)description.PixelFormat,
                (long)description.RowStride);
            Sequence = sequence;
            MonotonicTimestampNs = timestamp;
        }
        catch
        {
            _handle.Dispose();
            throw;
        }
    }

    /// <summary>Gets the frame dimensions and pixel layout.</summary>
    public CameraFrameDescription Description { get; }

    /// <summary>Gets the capture sequence number.</summary>
    public ulong Sequence { get; }

    /// <summary>Gets the capture timestamp on the monotonic clock, in nanoseconds.</summary>
    public long MonotonicTimestampNs { get; }

    /// <summary>Copies the frame pixels into a managed array.</summary>
    /// <returns>The frame bytes.</returns>
    public byte[] ToArray()
    {
        SafeHandle raw = Handle;
        return TransportMarshal.ReadBytes((byte[]? buffer, nuint capacity, out nuint size) =>
            NativeMethods.wse_capi_camera_frame_data(raw, buffer, out size, capacity));
    }

    /// <summary>
    /// Returns a new frame whose orientation has been corrected. A quarter turn exchanges the
    /// width and the height. A Bayer frame is refused, because moving its pixels puts them on
    /// sites of other colours; convert it with <see cref="Demosaic"/> first.
    /// </summary>
    /// <param name="orientation">The correction to apply.</param>
    /// <returns>The corrected frame, which the caller disposes.</returns>
    public CameraFrame ApplyOrientation(ImageOrientation orientation)
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_camera_frame_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_frame_apply_orientation(
                Handle, out output.Value, (int)orientation));
        return new CameraFrame(output);
    }

    /// <summary>
    /// Returns a new colour frame raised from this Bayer one. The result format is one of
    /// <see cref="CameraPixelFormat.Rgb8"/>, <see cref="CameraPixelFormat.Bgr8"/>,
    /// <see cref="CameraPixelFormat.Rgb16"/>, and <see cref="CameraPixelFormat.Bgr16"/>; an
    /// eight-bit result keeps the high eight bits of each sample.
    /// </summary>
    /// <param name="outputFormat">The format the result carries.</param>
    /// <param name="method">How the colour is raised.</param>
    /// <returns>The colour frame, which the caller disposes.</returns>
    public CameraFrame Demosaic(
        CameraPixelFormat outputFormat, DemosaicMethod method = DemosaicMethod.Bilinear)
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_camera_frame_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_frame_demosaic(
                Handle, out output.Value, (int)outputFormat, (int)method));
        return new CameraFrame(output);
    }

    /// <summary>Releases the native frame.</summary>
    public void Dispose() => _handle.Dispose();

    // The accumulator takes frames by their native handle, which is what the flat ABI expects.
    internal SafeHandle NativeHandle => Handle;

    private SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(CameraFrame));
            }
            return _handle;
        }
    }
}

/// <summary>
/// The sole public camera owner. Enumerate devices, open one, start streaming, read frames, and
/// close when finished. <see cref="Close"/> ends the device session and the same instance may be
/// opened again; <see cref="Dispose"/> releases the native camera and is terminal.
/// </summary>
public sealed class WebCamera : IDisposable
{
    private readonly CameraHandle _handle;
    private CameraCallbackRegistration? _callbackRegistration;

    /// <summary>
    /// The first managed callback/adoption exception from the current registration, or null.
    /// Such an exception suppresses further delivery and requests stop without crossing native code.
    /// Readable after Close/Dispose; cleared by a successful Start. A failed Start retains the previous value.
    /// </summary>
    public Exception? CallbackException => _callbackRegistration?.Failure;


    /// <summary>Creates a camera that has not opened a device yet.</summary>
    /// <exception cref="WseException">The native camera could not be created.</exception>
    public WebCamera()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_camera_destroy);
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_camera_create(out output.Value));
        _handle = new CameraHandle(NativeMethods.wse_capi_camera_destroy);
        _handle.Attach(output.Take());
    }

    /// <summary>Enumerates the available cameras.</summary>
    /// <param name="backend">Backend to enumerate, or <see cref="CameraBackend.Automatic"/>.</param>
    /// <returns>The discovered devices. Dispose each when finished.</returns>
    /// <exception cref="WseException">Enumeration failed.</exception>
    public static IReadOnlyList<CameraDevice> Enumerate(
        CameraBackend backend = CameraBackend.Automatic)
    {
        using var listOutput = new NativeOutput(NativeMethods.wse_capi_camera_device_list_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_enumerate(out listOutput.Value, (int)backend));
        IntPtr list = listOutput.Value;
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_device_list_count(list, out nuint count));
        return NativeOutput.Collect(checked((int)count), index =>
        {
            using var output = new NativeOutput(NativeMethods.wse_capi_camera_device_destroy);
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_device_list_at(list, out output.Value, (nuint)index));
            return new CameraDevice(output);
        });
    }

    /// <summary>Reads what a device supports without opening it.</summary>
    /// <param name="device">Device to describe.</param>
    /// <returns>The capability set. Dispose it when finished.</returns>
    public static CameraCapability Capabilities(CameraDevice device)
    {
        ArgumentNullException.ThrowIfNull(device);
        return device.Capabilities();
    }

    /// <summary>Opens a device with the backend default stream.</summary>
    /// <param name="device">Device to open.</param>
    public void Open(CameraDevice device)
    {
        ArgumentNullException.ThrowIfNull(device);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_open(Handle, device.Handle, IntPtr.Zero));
    }

    /// <summary>Opens a device with an explicit stream configuration.</summary>
    /// <param name="device">Device to open.</param>
    /// <param name="configuration">Native format, output format, and conversion policy.</param>
    public void Open(CameraDevice device, CameraStreamConfiguration configuration)
    {
        ArgumentNullException.ThrowIfNull(device);
        NativeCameraStreamConfiguration native = configuration.ToNative();
        GCHandle pin = GCHandle.Alloc(native, GCHandleType.Pinned);
        try
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_open(
                    Handle, device.Handle, pin.AddrOfPinnedObject()));
        }
        finally
        {
            pin.Free();
        }
    }

    /// <summary>Starts streaming. Collect frames with <see cref="ReadFrame"/>.</summary>
    public void Start()
    {
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_camera_start(Handle));
        _handle.CallbackRoot = null;
        _callbackRegistration = null;
    }

    /// <summary>
    /// Starts streaming and delivers each frame to <paramref name="onFrame"/>. The callback runs on
    /// the capture thread, so it owns its own thread safety, and it owns the frame it is given:
    /// dispose that frame, or keep it, exactly as with a frame from <see cref="ReadFrame"/>.
    /// Exceptions are retained in <see cref="CallbackException"/> and stop further delivery.
    /// A frame already passed to the callback remains its responsibility even if it throws.
    /// Call Stop from the owner to join before restarting; do not Close/Dispose from this callback.
    /// </summary>
    /// <param name="onFrame">
    /// Called once per capture. On a failed capture the frame is null and the exception describing
    /// the failure is passed instead; neither argument is null at the same time.
    /// </param>
    /// <exception cref="ArgumentNullException"><paramref name="onFrame"/> is null.</exception>
    /// <exception cref="WseException">The camera refused to start.</exception>
    public void Start(Action<CameraFrame?, WseException?> onFrame)
    {
        ArgumentNullException.ThrowIfNull(onFrame);

        var registration = new CameraCallbackRegistration(_handle, onFrame);
        NativeMethods.CameraFrameCallback bridge = registration.Bridge;
        CameraCallbackRegistration? previousRegistration = _callbackRegistration;
        _callbackRegistration = registration;
        NativeMethods.CameraFrameCallback? previous = _handle.CallbackRoot;
        _handle.CallbackRoot = bridge;
        try
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_start_with_callback(Handle, bridge, IntPtr.Zero));
        }
        catch
        {
            _handle.CallbackRoot = previous;
            _callbackRegistration = previousRegistration;
            throw;
        }
    }

    /// <summary>Stops streaming.</summary>
    public void Stop()
    {
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_camera_stop(Handle));
        // Callback-side Stop is not a join barrier. Retain the delegate until a new
        // successful Start, Close, or native destruction has finished using it.
    }

    /// <summary>Reads one frame with an explicit deadline.</summary>
    /// <param name="timeout">Deadline for the read.</param>
    /// <returns>The captured frame. Dispose it when finished.</returns>
    /// <exception cref="WseException">
    /// The read failed, including <see cref="ErrorCategory.Timeout"/> when no frame arrived.
    /// </exception>
    public CameraFrame ReadFrame(TimeSpan timeout)
    {
        if (timeout < TimeSpan.Zero)
        {
            throw new ArgumentOutOfRangeException(
                nameof(timeout), "The frame timeout must not be negative.");
        }
        using var output = new NativeOutput(NativeMethods.wse_capi_camera_frame_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_read_frame(
                Handle, out output.Value, (uint)timeout.TotalMilliseconds));
        return new CameraFrame(output);
    }

    /// <summary>
    /// Reads several frames and returns their average. The camera has to be streaming first, and
    /// the first failed read ends the loop.
    /// </summary>
    /// <param name="count">How many frames to read.</param>
    /// <param name="timeout">How long one frame may take.</param>
    /// <returns>The averaged frame, which the caller disposes.</returns>
    public CameraFrame ReadAveragedFrame(int count, TimeSpan timeout)
    {
        if (count <= 0)
        {
            throw new ArgumentOutOfRangeException(
                nameof(count), "At least one frame has to be read.");
        }
        if (timeout < TimeSpan.Zero)
        {
            throw new ArgumentOutOfRangeException(
                nameof(timeout), "The frame timeout must not be negative.");
        }
        using var output = new NativeOutput(NativeMethods.wse_capi_camera_frame_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_read_averaged_frame(
                Handle, out output.Value, (nuint)count, (uint)timeout.TotalMilliseconds));
        return new CameraFrame(output);
    }

    /// <summary>Reads what the currently open device supports.</summary>
    /// <returns>The capability set. Dispose it when finished.</returns>
    public CameraCapability CurrentCapabilities()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_camera_capability_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_current_capabilities(Handle, out output.Value));
        return new CameraCapability(output);
    }

    /// <summary>Reads what one control supports on the open device.</summary>
    /// <param name="control">Control to describe.</param>
    /// <returns>The control capability.</returns>
    public CameraControlCapability ControlCapability(CameraControl control)
    {
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_query_control_capability(
                Handle, out NativeCameraControlCapability capability, (int)control));
        return CameraControlCapability.FromNative(capability, string.Empty);
    }

    /// <summary>Reads the current value of one control.</summary>
    /// <param name="control">Control to read.</param>
    /// <returns>The current value.</returns>
    public CameraControlValue GetControl(CameraControl control)
    {
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_get_control(
                Handle, out NativeCameraControlValue value, (int)control));
        return new CameraControlValue(
            (CameraControl)value.Control, (CameraControlMode)value.Mode, value.Value);
    }

    /// <summary>Writes one control value.</summary>
    /// <param name="value">Control, mode, and device-native value.</param>
    public void SetControl(CameraControlValue value)
    {
        var native = new NativeCameraControlValue
        {
            Control = (int)value.Control,
            Mode = (int)value.Mode,
            Value = value.Value,
        };
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_set_control(Handle, ref native));
    }

    /// <summary>Reads an extension-unit value.</summary>
    /// <param name="selector">Selector to read.</param>
    /// <returns>The payload bytes.</returns>
    public byte[] GetExtensionUnit(CameraExtensionUnitSelector selector)
    {
        ArgumentNullException.ThrowIfNull(selector);
        SafeHandle raw = Handle;
        SafeHandle selectorHandle = selector.Handle;
        return TransportMarshal.ReadBytes((byte[]? buffer, nuint capacity, out nuint size) =>
            NativeMethods.wse_capi_camera_get_extension_unit(
                raw, buffer, out size, selectorHandle, capacity));
    }

    /// <summary>Writes an extension-unit value.</summary>
    /// <param name="selector">Selector to write.</param>
    /// <param name="payload">Payload bytes.</param>
    public void SetExtensionUnit(CameraExtensionUnitSelector selector, byte[] payload)
    {
        ArgumentNullException.ThrowIfNull(selector);
        ArgumentNullException.ThrowIfNull(payload);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_set_extension_unit(
                Handle, selector.Handle, payload, (nuint)payload.Length));
    }

    /// <summary>Gets a value indicating whether a device is open.</summary>
    public bool IsOpen
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_is_open(Handle, out int open));
            return open != 0;
        }
    }

    /// <summary>Gets a value indicating whether the camera is streaming.</summary>
    public bool IsStreaming
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_is_streaming(Handle, out int streaming));
            return streaming != 0;
        }
    }

    /// <summary>Stops streaming and closes the device. Calling this again is safe.</summary>
    public void Close()
    {
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_camera_close(Handle));
        _handle.CallbackRoot = null;
    }

    /// <summary>Closes the device and releases the native camera.</summary>
    public void Dispose() => _handle.Dispose();

    private SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(WebCamera));
            }
            return _handle;
        }
    }
}

/// <summary>
/// Questions about what a pixel format allows, and the layout a Bayer format carries.
/// </summary>
public static class CameraFrameOps
{
    /// <summary>Whether a pixel format can be reoriented. A Bayer format cannot.</summary>
    /// <param name="pixelFormat">The format to ask about.</param>
    /// <returns>True when the format can be reoriented.</returns>
    public static bool IsFrameOperationSupported(CameraPixelFormat pixelFormat) =>
        NativeMethods.wse_capi_camera_pixel_format_supports_orientation((int)pixelFormat);

    /// <summary>Whether a pixel format can be averaged. A Bayer format can.</summary>
    /// <param name="pixelFormat">The format to ask about.</param>
    /// <returns>True when the format can be averaged.</returns>
    public static bool IsFrameAveragingSupported(CameraPixelFormat pixelFormat) =>
        NativeMethods.wse_capi_camera_pixel_format_supports_averaging((int)pixelFormat);

    /// <summary>Whether a pixel format carries a Bayer layout.</summary>
    /// <param name="pixelFormat">The format to ask about.</param>
    /// <returns>True for a Bayer format.</returns>
    public static bool IsBayerFormat(CameraPixelFormat pixelFormat) =>
        NativeMethods.wse_capi_camera_pixel_format_is_bayer((int)pixelFormat);

    /// <summary>Reads the Bayer layout a format carries.</summary>
    /// <param name="pixelFormat">The format to ask about.</param>
    /// <returns>The layout of the top-left two-by-two block.</returns>
    public static BayerPattern BayerPatternOf(CameraPixelFormat pixelFormat)
    {
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_pixel_format_bayer_pattern(
                out int pattern, (int)pixelFormat));
        return (BayerPattern)pattern;
    }
}

/// <summary>
/// Accumulates camera frames of one shape and produces their average. Only an accumulation buffer
/// is held, independently of how many frames were added; adding a frame whose description differs
/// fails and leaves the accumulation unchanged.
/// </summary>
public sealed class CameraFrameAccumulator : IDisposable
{
    private readonly CameraHandle _handle;

    /// <summary>Creates an empty accumulator.</summary>
    public CameraFrameAccumulator()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_camera_frame_accumulator_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_frame_accumulator_create(out output.Value));
        _handle = new CameraHandle(NativeMethods.wse_capi_camera_frame_accumulator_destroy);
        _handle.Attach(output.Take());
    }

    /// <summary>Adds one frame.</summary>
    /// <param name="frame">The frame to add.</param>
    public void Add(CameraFrame frame)
    {
        ArgumentNullException.ThrowIfNull(frame);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_frame_accumulator_add(Handle, frame.NativeHandle));
    }

    /// <summary>Gets the number of accumulated frames.</summary>
    public long Count
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_camera_frame_accumulator_count(Handle, out nuint count));
            return (long)count;
        }
    }

    /// <summary>Returns the averaged frame, rounded to nearest.</summary>
    /// <returns>The averaged frame, which the caller disposes.</returns>
    public CameraFrame Average()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_camera_frame_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_camera_frame_accumulator_average(Handle, out output.Value));
        return new CameraFrame(output);
    }

    /// <summary>Discards the accumulation.</summary>
    public void Reset() => NativeMethods.wse_capi_camera_frame_accumulator_reset(Handle);

    /// <summary>Releases the native accumulator.</summary>
    public void Dispose() => _handle.Dispose();

    private SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(CameraFrameAccumulator));
            }
            return _handle;
        }
    }
}

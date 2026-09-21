//*****************************************************************************************************************
//!
//! @file    SerialPort.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese XPT Serial PortのOwner.
//! @brief   \~english  XPT serial port owner.
//!
//! @details
//!     \~japanese
//!     @n Portable なSerial Port APIを公開する。
//!     \~english
//!     @n Exposes the portable serial-port API.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Runtime.InteropServices;

namespace WapitiStew.Wse;

/// <summary>Owns the native serial port handle and releases it exactly once.</summary>
internal sealed class SerialPortHandle : SafeHandle
{
    internal SerialPortHandle()
        : base(IntPtr.Zero, ownsHandle: true)
    {
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Attach(IntPtr value) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        NativeMethods.wse_capi_serial_port_destroy(handle);
        return true;
    }
}

/// <summary>
/// A serial port. Open it by device name and baud rate, then send and receive with explicit
/// deadlines. Dispose it when finished; disposing closes an open port.
/// </summary>
public sealed class SerialPort : IDisposable
{
    private readonly SerialPortHandle _handle;

    /// <summary>Creates a closed port.</summary>
    /// <exception cref="WseException">The native port could not be created.</exception>
    public SerialPort()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_serial_port_destroy);
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_serial_port_create(out output.Value));
        _handle = new SerialPortHandle();
        _handle.Attach(output.Take());
    }

    /// <summary>Opens the port.</summary>
    /// <param name="deviceName">Platform device name, such as <c>COM3</c> or <c>/dev/ttyUSB0</c>.</param>
    /// <param name="baudRate">Baud rate.</param>
    /// <param name="context">Explicit deadline and optional cancellation.</param>
    /// <exception cref="ArgumentNullException"><paramref name="deviceName"/> is null.</exception>
    /// <exception cref="WseException">Opening the port failed.</exception>
    public void Open(string deviceName, int baudRate, OperationContext context)
    {
        ArgumentNullException.ThrowIfNull(deviceName);
        using var cancellationLease = context.Cancellation?.Acquire();
        NativeOperationContext native = context.ToNative(cancellationLease);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_serial_port_open(
                Handle, TransportMarshal.Utf8(deviceName), baudRate, ref native));
    }

    /// <summary>Closes the port. Calling this again is safe.</summary>
    public void Close()
    {
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_serial_port_close(Handle));
    }

    /// <summary>Gets a value indicating whether the port is open.</summary>
    public bool IsOpen
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_serial_port_is_open(Handle, out int open));
            return open != 0;
        }
    }

    /// <summary>Gets the opened device name, or an empty string while the port is closed.</summary>
    public string DeviceName
    {
        get
        {
            SafeHandle raw = Handle;
            return TransportMarshal.ReadString(
                (byte[]? buffer, nuint capacity, out nuint size) =>
                    NativeMethods.wse_capi_serial_port_device_name(raw, buffer, out size, capacity));
        }
    }

    /// <summary>Gets the configured baud rate.</summary>
    public int BaudRate
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_serial_port_baud_rate(Handle, out int baudRate));
            return baudRate;
        }
    }

    /// <summary>Sends the complete buffer synchronously.</summary>
    /// <param name="data">Bytes to send.</param>
    /// <param name="context">Explicit deadline and optional cancellation.</param>
    /// <returns>The number of bytes sent.</returns>
    /// <exception cref="ArgumentNullException"><paramref name="data"/> is null.</exception>
    /// <exception cref="TransferException">The send failed; BytesTransferred retains known progress.</exception>
    public int Send(byte[] data, OperationContext context)
    {
        ArgumentNullException.ThrowIfNull(data);
        using var cancellationLease = context.Cancellation?.Acquire();
        NativeOperationContext native = context.ToNative(cancellationLease);
        nuint sent = 0;
        NativeStatus status = NativeMethods.wse_capi_serial_port_send(
                Handle, out sent, data, (nuint)data.Length, ref native);
        return TransferMarshal.CompleteSend(status, sent);
    }

    /// <summary>Receives one chunk up to a maximum size synchronously.</summary>
    /// <param name="maximumSize">Maximum number of bytes to receive.</param>
    /// <param name="context">Explicit deadline and optional cancellation.</param>
    /// <returns>The received bytes.</returns>
    /// <exception cref="WseException">The receive failed.</exception>
    public byte[] Receive(int maximumSize, OperationContext context)
    {
        using var cancellationLease = context.Cancellation?.Acquire();
        NativeOperationContext native = context.ToNative(cancellationLease);
        using var output = new NativeOutput(NativeMethods.wse_capi_frame_buffer_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_serial_port_receive(
                Handle, out output.Value, (nuint)maximumSize, ref native));
        using var buffer = new FrameBuffer(output);
        return buffer.ToArray();
    }

    /// <summary>Closes the port and releases the native resource.</summary>
    public void Dispose() => _handle.Dispose();

    private SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(SerialPort));
            }
            return _handle;
        }
    }
}

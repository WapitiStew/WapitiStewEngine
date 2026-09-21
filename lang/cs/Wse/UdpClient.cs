//*****************************************************************************************************************
//!
//! @file    UdpClient.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese XPT UDP SocketのOwnerと受信Datagram.
//! @brief   \~english  XPT UDP socket owner and received datagram.
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

/// <summary>Owns the native UDP client handle and releases it exactly once.</summary>
internal sealed class UdpClientHandle : SafeHandle
{
    internal UdpClientHandle()
        : base(IntPtr.Zero, ownsHandle: true)
    {
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Attach(IntPtr value) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        NativeMethods.wse_capi_udp_client_destroy(handle);
        return true;
    }
}

/// <summary>Owns the native datagram handle and releases it exactly once.</summary>
internal sealed class UdpDatagramHandle : SafeHandle
{
    internal UdpDatagramHandle()
        : base(IntPtr.Zero, ownsHandle: true)
    {
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Attach(IntPtr value) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        NativeMethods.wse_capi_udp_datagram_destroy(handle);
        return true;
    }
}

/// <summary>One received UDP datagram with its source endpoint. Dispose it when finished.</summary>
public sealed class UdpDatagram : IDisposable
{
    private readonly UdpDatagramHandle _handle;

    internal UdpDatagram(NativeOutput output)
    {
        _handle = new UdpDatagramHandle();
        _handle.Attach(output.Take());
    }

    /// <summary>Gets the endpoint the datagram was received from.</summary>
    public Endpoint Source
    {
        get
        {
            SafeHandle raw = Handle;
            ushort port = 0;
            string host = TransportMarshal.ReadString(
                (byte[]? buffer, nuint capacity, out nuint size) =>
                {
                    NativeStatus status = NativeMethods.wse_capi_udp_datagram_source(
                        raw, buffer, out size, out ushort readPort, capacity);
                    port = readPort;
                    return status;
                });
            return new Endpoint(host, port);
        }
    }

    /// <summary>Copies the datagram payload into a managed array.</summary>
    /// <returns>The payload bytes.</returns>
    public byte[] Payload()
    {
        SafeHandle raw = Handle;
        return TransportMarshal.ReadBytes((byte[]? buffer, nuint capacity, out nuint size) =>
            NativeMethods.wse_capi_udp_datagram_payload(raw, buffer, out size, capacity));
    }

    /// <summary>Releases the native datagram.</summary>
    public void Dispose() => _handle.Dispose();

    private SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(UdpDatagram));
            }
            return _handle;
        }
    }
}

/// <summary>A UDP socket. Bind it to a local endpoint, then send and receive datagrams.</summary>
public sealed class UdpClient : IDisposable
{
    private readonly UdpClientHandle _handle;

    /// <summary>Creates an unbound client.</summary>
    /// <exception cref="WseException">The native client could not be created.</exception>
    public UdpClient()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_udp_client_destroy);
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_udp_client_create(out output.Value));
        _handle = new UdpClientHandle();
        _handle.Attach(output.Take());
    }

    /// <summary>Gets the maximum number of bytes one datagram can carry.</summary>
    public static int MaximumDatagramSize =>
        checked((int)NativeMethods.wse_capi_udp_maximum_datagram_size());

    /// <summary>
    /// Binds the socket to a local endpoint. A port of zero requests an operating-system-assigned
    /// port, which <see cref="LocalEndpoint"/> then reports.
    /// </summary>
    /// <param name="endpoint">Local host and port.</param>
    /// <param name="context">Explicit deadline and optional cancellation.</param>
    /// <exception cref="WseException">The bind failed.</exception>
    public void Bind(Endpoint endpoint, OperationContext context)
    {
        using var cancellationLease = context.Cancellation?.Acquire();
        NativeOperationContext native = context.ToNative(cancellationLease);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_udp_client_bind(
                Handle, TransportMarshal.Utf8(endpoint.Host), endpoint.Port, ref native));
    }

    /// <summary>Closes the socket. Calling this again is safe.</summary>
    public void Close()
    {
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_udp_client_close(Handle));
    }

    /// <summary>Gets a value indicating whether the socket is open.</summary>
    public bool IsOpen
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_udp_client_is_open(Handle, out int open));
            return open != 0;
        }
    }

    /// <summary>Gets the bound local endpoint.</summary>
    public Endpoint LocalEndpoint
    {
        get
        {
            SafeHandle raw = Handle;
            ushort port = 0;
            string host = TransportMarshal.ReadString(
                (byte[]? buffer, nuint capacity, out nuint size) =>
                {
                    NativeStatus status = NativeMethods.wse_capi_udp_client_local_endpoint(
                        raw, buffer, out size, out ushort readPort, capacity);
                    port = readPort;
                    return status;
                });
            return new Endpoint(host, port);
        }
    }

    /// <summary>Sends one datagram to a remote endpoint.</summary>
    /// <param name="endpoint">Remote host and port.</param>
    /// <param name="data">Payload bytes.</param>
    /// <param name="context">Explicit deadline and optional cancellation.</param>
    /// <returns>The number of bytes sent.</returns>
    /// <exception cref="ArgumentNullException"><paramref name="data"/> is null.</exception>
    /// <exception cref="TransferException">The send failed; BytesTransferred retains known progress.</exception>
    public int SendTo(Endpoint endpoint, byte[] data, OperationContext context)
    {
        ArgumentNullException.ThrowIfNull(data);
        using var cancellationLease = context.Cancellation?.Acquire();
        NativeOperationContext native = context.ToNative(cancellationLease);
        nuint sent = 0;
        NativeStatus status = NativeMethods.wse_capi_udp_client_send_to(
                Handle, out sent, TransportMarshal.Utf8(endpoint.Host), endpoint.Port,
                data, (nuint)data.Length, ref native);
        return TransferMarshal.CompleteSend(status, sent);
    }

    /// <summary>
    /// Receives one datagram. Truncation throws TransferException with a detached prefix and source.
    /// The discarded suffix cannot be read later; the entire datagram was consumed.
    /// </summary>
    /// <param name="maximumSize">Maximum payload size to accept.</param>
    /// <param name="context">Explicit deadline and optional cancellation.</param>
    /// <returns>The received datagram. Dispose it when finished.</returns>
    /// <exception cref="WseException">
    /// The receive failed, including <see cref="TransportErrorCode.TimedOut"/> when the deadline
    /// elapsed with no traffic.
    /// </exception>
    public UdpDatagram ReceiveFrom(int maximumSize, OperationContext context)
    {
        using var cancellationLease = context.Cancellation?.Acquire();
        NativeOperationContext native = context.ToNative(cancellationLease);
        using var output = new NativeOutput(NativeMethods.wse_capi_udp_datagram_destroy);
        NativeStatus status = NativeMethods.wse_capi_udp_client_receive_from_with_progress(
            Handle, out output.Value, (nuint)maximumSize, ref native);
        return TransferMarshal.CompleteReceive(status, output);
    }

    /// <summary>Closes the socket and releases the native client.</summary>
    public void Dispose() => _handle.Dispose();

    private SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(UdpClient));
            }
            return _handle;
        }
    }
}

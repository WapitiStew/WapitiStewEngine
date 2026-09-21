//*****************************************************************************************************************
//!
//! @file    TcpClient.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese 呼出元Thread前提のXPT TCP接続Owner.
//! @brief   \~english  Caller-confined XPT TCP connection owner.
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

/// <summary>Owns the native TCP client handle and releases it exactly once.</summary>
internal sealed class TcpClientHandle : SafeHandle
{
    internal TcpClientHandle()
        : base(IntPtr.Zero, ownsHandle: true)
    {
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Attach(IntPtr value) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        NativeMethods.wse_capi_tcp_client_destroy(handle);
        return true;
    }
}

/// <summary>
/// A caller-confined TCP connection. Use one instance from one thread at a time and dispose it
/// when finished; disposing closes an open connection.
/// </summary>
public sealed class TcpClient : IDisposable
{
    private readonly TcpClientHandle _handle;

    /// <summary>Creates a disconnected client.</summary>
    /// <exception cref="WseException">The native client could not be created.</exception>
    public TcpClient()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_tcp_client_destroy);
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_tcp_client_create(out output.Value));
        _handle = new TcpClientHandle();
        _handle.Attach(output.Take());
    }

    /// <summary>Connects synchronously to a remote endpoint.</summary>
    /// <param name="endpoint">Remote host and port.</param>
    /// <param name="context">Explicit deadline and optional cancellation.</param>
    /// <exception cref="ObjectDisposedException">The client was already disposed.</exception>
    /// <exception cref="WseException">The connection attempt failed.</exception>
    public void Connect(Endpoint endpoint, OperationContext context)
    {
        using var cancellationLease = context.Cancellation?.Acquire();
        NativeOperationContext native = context.ToNative(cancellationLease);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_tcp_client_connect(
                Handle, TransportMarshal.Utf8(endpoint.Host), endpoint.Port, ref native));
    }

    /// <summary>Closes the connection. Calling this again is safe.</summary>
    public void Disconnect()
    {
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_tcp_client_disconnect(Handle));
    }

    /// <summary>Gets a value indicating whether the local connection state is held.</summary>
    public bool IsConnected
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_tcp_client_is_connected(Handle, out int connected));
            return connected != 0;
        }
    }

    /// <summary>
    /// Checks for an operating-system-observable peer FIN or reset without consuming pending data.
    /// Success is not an end-to-end liveness proof.
    /// </summary>
    /// <exception cref="WseException">The peer closed or reset the connection, or none is open.</exception>
    public void CheckPeerConnection()
    {
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_tcp_client_check_peer_connection(Handle));
    }

    /// <summary>Gets the connected remote endpoint.</summary>
    public Endpoint RemoteEndpoint => ReadEndpoint(remote: true);

    /// <summary>Gets the operating-system-selected local endpoint.</summary>
    public Endpoint LocalEndpoint => ReadEndpoint(remote: false);

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
        NativeStatus status = NativeMethods.wse_capi_tcp_client_send(
                Handle, out sent, data, (nuint)data.Length, ref native);
        return TransferMarshal.CompleteSend(status, sent);
    }

    /// <summary>Receives one chunk up to a maximum size synchronously.</summary>
    /// <param name="maximumSize">Maximum number of bytes to receive.</param>
    /// <param name="context">Explicit deadline and optional cancellation.</param>
    /// <returns>The received bytes.</returns>
    /// <exception cref="WseException">
    /// The receive failed. A normal peer close reports <see cref="TransportErrorCode.RemoteClosed"/>.
    /// </exception>
    public byte[] Receive(int maximumSize, OperationContext context)
    {
        using var cancellationLease = context.Cancellation?.Acquire();
        NativeOperationContext native = context.ToNative(cancellationLease);
        using var output = new NativeOutput(NativeMethods.wse_capi_frame_buffer_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_tcp_client_receive(
                Handle, out output.Value, (nuint)maximumSize, ref native));
        using var buffer = new FrameBuffer(output);
        return buffer.ToArray();
    }

    /// <summary>Closes the connection and releases the native client.</summary>
    public void Dispose() => _handle.Dispose();

    private Endpoint ReadEndpoint(bool remote)
    {
        SafeHandle raw = Handle;
        ushort port = 0;
        string host = TransportMarshal.ReadString((byte[]? buffer, nuint capacity, out nuint size) =>
        {
            ushort readPort;
            NativeStatus status = remote
                ? NativeMethods.wse_capi_tcp_client_remote_endpoint(
                    raw, buffer, out size, out readPort, capacity)
                : NativeMethods.wse_capi_tcp_client_local_endpoint(
                    raw, buffer, out size, out readPort, capacity);
            port = readPort;
            return status;
        });
        return new Endpoint(host, port);
    }

    private SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(TcpClient));
            }
            return _handle;
        }
    }
}

//*****************************************************************************************************************
//!
//! @file    Transport.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese XPT Transportが共有するEndpoint、Operation制御およびError Code.
//! @brief   \~english  Endpoints, operation controls, and error codes shared by the XPT transports.
//!
//! @details
//!     \~japanese
//!     @n XPTはDefault Timeoutを持たない。全Operationが<see cref="OperationContext"/>を要求するため、
//!        呼出元が無限待機へ落ちることはない。
//!     \~english
//!     @n XPT has no default timeout. Every operation requires an <see cref="OperationContext"/>, so
//!        a caller can never fall into an unbounded wait.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Text;

namespace WapitiStew.Wse;

/// <summary>
/// Stable transport error codes. A <see cref="WseException.Code"/> raised by an XPT operation
/// carries one of these values, while <see cref="WseException.Category"/> is normalized to the
/// common binding categories.
/// </summary>
public enum TransportErrorCode
{
    /// <summary>No failure.</summary>
    None = 0,
    /// <summary>A public API argument was invalid.</summary>
    InvalidArgument = 1,
    /// <summary>The host could not be resolved.</summary>
    HostNotFound = 2,
    /// <summary>No usable address was available.</summary>
    AddressUnavailable = 3,
    /// <summary>The remote refused the connection.</summary>
    ConnectionRefused = 4,
    /// <summary>The connection was reset or aborted.</summary>
    ConnectionReset = 5,
    /// <summary>The network or host was unreachable.</summary>
    NetworkUnreachable = 6,
    /// <summary>The operation requires a connection that is not established.</summary>
    NotConnected = 7,
    /// <summary>The remote closed the connection normally.</summary>
    RemoteClosed = 8,
    /// <summary>The explicit deadline elapsed.</summary>
    TimedOut = 9,
    /// <summary>Cancellation was requested.</summary>
    Cancelled = 10,
    /// <summary>Binding to the local endpoint failed.</summary>
    BindFailed = 11,
    /// <summary>Sending failed.</summary>
    SendFailed = 12,
    /// <summary>Receiving failed.</summary>
    ReceiveFailed = 13,
    /// <summary>The message exceeded the transport limit.</summary>
    MessageTooLarge = 14,
    /// <summary>The datagram was truncated to the receive buffer.</summary>
    DatagramTruncated = 15,
    /// <summary>A handle, socket, or memory resource was exhausted.</summary>
    ResourceExhausted = 16,
    /// <summary>The platform or backend does not support the operation.</summary>
    Unsupported = 17,
    /// <summary>The failure did not fall into any other code.</summary>
    Unknown = 18,
    /// <summary>Opening the device or transport resource failed.</summary>
    OpenFailed = 19,
    /// <summary>Configuring the device or transport resource failed.</summary>
    ConfigurationFailed = 20,
    /// <summary>The server returned an HTTP 4xx or 5xx status.</summary>
    HttpStatusError = 21,
    /// <summary>The response body exceeded the caller ceiling.</summary>
    ResponseTooLarge = 22,
    /// <summary>TLS verification or another security step failed.</summary>
    SecurityFailed = 23,
}

/// <summary>A host name or numeric address paired with a TCP or UDP port.</summary>
/// <param name="Host">Host name, IPv4 address, or IPv6 address.</param>
/// <param name="Port">Port number.</param>
public readonly record struct Endpoint(string Host, ushort Port)
{
    /// <summary>
    /// Gets a value indicating whether this endpoint is usable as a remote endpoint, meaning a
    /// non-empty host and a non-zero port.
    /// </summary>
    public bool IsValid => !string.IsNullOrEmpty(Host) && Port != 0;

    /// <summary>Returns the endpoint as <c>host:port</c>.</summary>
    /// <returns>The formatted endpoint.</returns>
    public override string ToString() => $"{Host}:{Port}";
}

/// <summary>
/// The explicit controls that every XPT operation requires. There is deliberately no default
/// timeout, so a caller always states its own deadline.
/// </summary>
public readonly struct OperationContext
{
    /// <summary>Creates a context with a deadline and no cancellation.</summary>
    /// <param name="timeout">Non-negative operation deadline.</param>
    public OperationContext(TimeSpan timeout)
        : this(timeout, null)
    {
    }

    /// <summary>Creates a context with a deadline and a cancellation source to observe.</summary>
    /// <param name="timeout">Non-negative operation deadline.</param>
    /// <param name="cancellation">Cancellation source to observe, or null.</param>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="timeout"/> is negative.</exception>
    public OperationContext(TimeSpan timeout, CancellationSource? cancellation)
    {
        if (timeout < TimeSpan.Zero)
        {
            throw new ArgumentOutOfRangeException(
                nameof(timeout), "The operation timeout must not be negative.");
        }
        Timeout = timeout;
        Cancellation = cancellation;
    }

    /// <summary>Gets the operation deadline.</summary>
    public TimeSpan Timeout { get; }

    /// <summary>Gets the cancellation source observed by the operation, if any.</summary>
    public CancellationSource? Cancellation { get; }

    internal NativeOperationContext ToNative(NativeHandleLease? cancellationLease) => new()
    {
        TimeoutMilliseconds = (long)Timeout.TotalMilliseconds,
        Cancellation = cancellationLease?.Pointer ?? IntPtr.Zero,
    };
}

/// <summary>Marshalling helpers shared by the transport wrappers.</summary>
internal static class TransportMarshal
{
    /// <summary>Encodes a string as a NUL-terminated UTF-8 byte array.</summary>
    internal static byte[] Utf8(string value)
    {
        ArgumentNullException.ThrowIfNull(value);
        byte[] encoded = new byte[Encoding.UTF8.GetByteCount(value) + 1];
        Encoding.UTF8.GetBytes(value, 0, value.Length, encoded, 0);
        return encoded;
    }

    /// <summary>Reads a two-call UTF-8 string getter into a managed string.</summary>
    internal delegate NativeStatus StringGetter(byte[]? buffer, nuint capacity, out nuint size);

    /// <summary>Runs the two-call pattern for a UTF-8 string getter.</summary>
    internal static string ReadString(StringGetter getter)
    {
        NativeMethods.ThrowIfFailed(getter(null, 0, out nuint required));
        if (required <= 1)
        {
            return string.Empty;
        }
        byte[] buffer = new byte[checked((int)required)];
        NativeMethods.ThrowIfFailed(getter(buffer, required, out _));
        return Encoding.UTF8.GetString(buffer, 0, buffer.Length - 1);
    }

    /// <summary>Reads a two-call byte getter into a managed array.</summary>
    internal delegate NativeStatus BytesGetter(byte[]? buffer, nuint capacity, out nuint size);

    /// <summary>Runs the two-call pattern for a byte getter.</summary>
    internal static byte[] ReadBytes(BytesGetter getter)
    {
        NativeMethods.ThrowIfFailed(getter(null, 0, out nuint required));
        if (required == 0)
        {
            return Array.Empty<byte>();
        }
        byte[] buffer = new byte[checked((int)required)];
        NativeMethods.ThrowIfFailed(getter(buffer, required, out _));
        return buffer;
    }
}

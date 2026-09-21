//*****************************************************************************************************************
//!
//! @file    UdpLoopbackExample.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese 明示DeadlineによるXPT UDPのLoopback送受信.
//! @brief   \~english  XPT UDP send and receive over loopback with explicit deadlines.
//!
//! @details
//!     \~japanese
//!     @n 実機不要である。両EndpointはProcess内の127.0.0.1 UDP Socketである。
//!     @n XPTはDefault Timeoutを持たない。全OperationがOperationContextを要求するため、呼出元が
//!        意図せず無限待機へ落ちることはない。
//!     @n 引数順序は arguments[0] がNative Library Pathである。追加引数は取らない。
//!     @n XPTにUDP Listener／Multicast APIは無い。BindしたSocketが送受信の両方を担う。
//!     \~english
//!     @n Hardware-free: both endpoints are 127.0.0.1 UDP sockets inside this process.
//!     @n XPT has no default timeout. Every operation requires an OperationContext, so a caller can
//!        never fall into an unbounded wait by accident.
//!     @n Argument order: arguments[0] is the native library path. The sample takes no further
//!        arguments.
//!     @n There is no separate listener or multicast surface: one bound socket both sends and
//!        receives, which is why the sample creates two of them rather than a server and a client.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Text;
using WapitiStew.Wse;

/// <summary>XPT UDP loopback send and receive.</summary>
public static class UdpLoopbackExample
{
    /// <summary>Sends one datagram to a loopback socket and reads it back.</summary>
    /// <param name="arguments">Optional path to the native library file.</param>
    /// <returns>Zero on success.</returns>
    public static int Main(string[] arguments)
    {
        if (arguments.Length > 0)
        {
            WseRuntime.SetNativeLibraryPath(arguments[0]);
        }

        using var runtime = new WseRuntime();
        if (!runtime.Info().Components.HasXpt)
        {
            Console.Error.WriteLine("This WSE build does not include the XPT component.");
            return 1;
        }

        // One explicit deadline is reused for every operation in this sample.
        // A second is far more than a loopback bind, send, or receive needs, so it never expires
        // here; it exists so a stalled socket ends the sample instead of hanging it. A context is
        // a value, so reusing this one gives each call its own fresh deadline rather than a shared
        // budget that runs down.
        var context = new OperationContext(TimeSpan.FromSeconds(1));

        // Port zero asks the operating system to assign a free port to each socket.
        // Naming a fixed port would make the sample fail whenever that port is already taken; the
        // assigned one is read back below from LocalEndpoint.
        // Each client owns one native socket through its SafeHandle, so two clients means two
        // sockets and two independent lifetimes.
        using var receiver = new UdpClient();
        receiver.Bind(new Endpoint("127.0.0.1", 0), context);

        using var sender = new UdpClient();
        sender.Bind(new Endpoint("127.0.0.1", 0), context);

        // The receiver has to be bound before the send, otherwise the datagram has nowhere to land
        // and UDP drops it silently.
        Endpoint receiverEndpoint = receiver.LocalEndpoint;
        Console.WriteLine($"receiver endpoint: {receiverEndpoint}");
        Console.WriteLine($"maximum datagram size: {UdpClient.MaximumDatagramSize}");

        const string Message = "wse-xpt-udp";
        byte[] payload = Encoding.UTF8.GetBytes(Message);
        int sent = sender.SendTo(receiverEndpoint, payload, context);
        Console.WriteLine($"sent bytes: {sent}");

        // The ceiling is the transport's own maximum, so nothing can be truncated. A smaller value
        // is legitimate and cheaper, but a datagram larger than it reports DatagramTruncated
        // rather than silently losing its tail.
        // The datagram owns native memory; the using releases it, and Payload() copies the bytes
        // out into managed memory first.
        using (UdpDatagram datagram = receiver.ReceiveFrom(UdpClient.MaximumDatagramSize, context))
        {
            Console.WriteLine($"received from: {datagram.Source}");
            Console.WriteLine($"received payload: {Encoding.UTF8.GetString(datagram.Payload())}");
        }

        // A short receive consumes the whole datagram, but the exception keeps a managed prefix
        // and source. No native owner is attached to this exception; it needs no Dispose.
        sender.SendTo(receiverEndpoint, payload, context);
        try
        {
            using var unexpected = receiver.ReceiveFrom(3, context);
            return 2;
        }
        catch (TransferException failure) when (failure.Code == (int)TransportErrorCode.DatagramTruncated)
        {
            Console.WriteLine($"retained prefix bytes: {failure.BytesTransferred}");
            Console.WriteLine($"prefix source: {failure.SourceEndpoint}");
            if (failure.ReceivedData?.Length != 3) return 2;
        }

        // A short deadline with no traffic must report TimedOut rather than blocking.
        // The timeout here is the point of the check, not an accident: no third datagram was ever
        // sent, so this receive is guaranteed to expire and the sample proves the deadline is
        // honoured. Fifty milliseconds is simply the shortest wait that stays reliable on a loaded
        // machine; a longer one would only make the sample slower to reach the same conclusion.
        var shortContext = new OperationContext(TimeSpan.FromMilliseconds(50));
        try
        {
            receiver.ReceiveFrom(UdpClient.MaximumDatagramSize, shortContext);
            Console.WriteLine("idle receive timed out: False");
        }
        catch (WseException failure)
        {
            bool timedOut = failure.Code == (int)TransportErrorCode.TimedOut;
            Console.WriteLine($"idle receive timed out: {timedOut}");
        }

        // Close is idempotent and terminal; Dispose would do the same.
        // Closing explicitly only makes the order visible: the using declarations above still run
        // Dispose afterwards, which releases the native client handle itself.
        receiver.Close();
        sender.Close();
        return 0;
    }
}

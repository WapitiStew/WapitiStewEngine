//*****************************************************************************************************************
//!
//! @file    TcpClientExample.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese XPT TCPのConnect、Send、Receive、Endpoint報告およびDisconnect.
//! @brief   \~english  XPT TCP connect, send, receive, endpoint report, and disconnect.
//!
//! @details
//!     \~japanese
//!     @n 引数順序は arguments[0] がNative Library Path、arguments[1] がHost、arguments[2] が
//!        Portである。HostとPortが無い場合は、その旨を出力して0を返す。
//!     @n XPTにListener APIは無い。任意のTCP Echo Service、例えば "127.0.0.1 7" を指定する。
//!     @n Server不在は本Sampleの不具合ではなく報告可能な状態である。構造化Errorを出力して0を返す。
//!     @n XPTはDefault Timeoutを持たない。全OperationがOperationContextを要求する。
//!     \~english
//!     @n Argument order: arguments[0] is the native library path, arguments[1] is the host, and
//!        arguments[2] is the port. Without a host and a port the sample explains that and returns
//!        zero.
//!     @n XPT provides no listener API, so point the arguments at any TCP echo service, for example
//!        "127.0.0.1 7".
//!     @n No server on this machine is a reportable state, not a sample defect: the structured
//!        error is printed and the sample still returns zero.
//!     @n XPT has no default timeout. Every operation requires an OperationContext.
//!     @n The C++ counterpart reports every failure through the WSE log. No log facility is bound,
//!        so this sample prints the same structured fields to the console instead.
//!     @n Host and port arrive as arguments here, where the C++ counterpart holds them as source
//!        constants. Port 7 in the usage line is the conventional echo service.
//!
//! @date
//!   Sep-06, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Globalization;
using System.Text;
using WapitiStew.Wse;

/// <summary>XPT TCP client round trip against a caller-named endpoint.</summary>
public static class TcpClientExample
{
    /// <summary>Largest reply this sample reads in one receive.</summary>
    // The payload is eleven bytes, so 256 is ample. The ceiling only bounds one Receive call: a
    // longer reply is not an error, it simply needs a further call. Raising it costs a larger
    // allocation per receive, lowering it costs more round trips.
    private const int ReceiveCeiling = 256;

    /// <summary>Connects, sends one payload, reads the reply, and disconnects.</summary>
    /// <param name="arguments">
    /// arguments[0] is the optional path to the native library file, arguments[1] is the host, and
    /// arguments[2] is the port.
    /// </param>
    /// <returns>Zero on success and when no reachable service was named.</returns>
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

        // Naming no service is not an error. The sample has to run on a build machine with no echo
        // service and no network, so it explains what it wanted and returns success.
        if (arguments.Length < 3)
        {
            Console.WriteLine(
                "No TCP service was named. Pass the native library path, then a host and a port,"
                + " for example: TcpClientExample <library> 127.0.0.1 7");
            return 0;
        }
        if (!ushort.TryParse(
                arguments[2], NumberStyles.Integer, CultureInfo.InvariantCulture, out ushort port)
            || port == 0)
        {
            Console.WriteLine($"'{arguments[2]}' is not a TCP port number between 1 and 65535.");
            return 0;
        }

        // One explicit deadline is reused for every operation in this sample.
        // A second suits a loopback or same-subnet service and keeps an unreachable host from
        // stalling the sample. Reaching across the internet, or resolving a slow DNS name, wants a
        // longer one; the context is a value, so each call starts its own second afresh.
        var context = new OperationContext(TimeSpan.FromSeconds(1));
        var endpoint = new Endpoint(arguments[1], port);

        // The client owns one native connection through its SafeHandle. The using releases that
        // handle on every path out, including the early returns below.
        using var client = new TcpClient();
        try
        {
            client.Connect(endpoint, context);
        }
        catch (WseException failure)
        {
            // No server at that endpoint is a reportable state, not a sample defect.
            // ConnectionRefused, HostNotFound, and TimedOut all land here and all end in success,
            // so the sample runs unchanged on a machine with no network at all.
            Report($"connect {endpoint}", failure);
            Console.WriteLine("name a reachable TCP service to see the rest of the round trip");
            return 0;
        }

        Console.WriteLine($"connected: {client.LocalEndpoint} -> {client.RemoteEndpoint}");
        Console.WriteLine($"is connected: {client.IsConnected}");

        // Send loops until the complete buffer is written or the deadline expires.
        // So a short count is never returned on success, and the printed figure always equals the
        // payload length. A partially written buffer surfaces as a failure instead.
        byte[] payload = Encoding.UTF8.GetBytes("WSE-XPT-TCP");
        try
        {
            int sent = client.Send(payload, context);
            Console.WriteLine($"sent bytes: {sent}");
        }
        catch (WseException failure)
        {
            Report("send", failure);
            client.Disconnect();
            return 2;
        }

        // One receive returns a single chunk; an orderly peer close reports RemoteClosed.
        // TCP is a byte stream, so the reply may arrive split across several chunks. A real client
        // loops until it has framed a complete message; one call is enough to show the shape.
        // Against a peer that is not an echo service the deadline elapsing is the expected result,
        // which is why TimedOut is filtered out below and does not fail the sample.
        try
        {
            byte[] received = client.Receive(ReceiveCeiling, context);
            Console.WriteLine(
                $"received {received.Length} bytes: {Encoding.UTF8.GetString(received)}");
        }
        catch (WseException failure) when (failure.Code == (int)TransportErrorCode.TimedOut)
        {
            Console.WriteLine("no reply within the deadline (the peer is not an echo service)");
        }
        catch (WseException failure)
        {
            Report("receive", failure);
        }

        // A non-destructive peek for an operating-system-observable peer close or reset.
        // Nothing pending is consumed, so this can be called between receives. Success only means
        // the local stack has seen no FIN or reset; it is not proof the peer is still alive.
        try
        {
            client.CheckPeerConnection();
            Console.WriteLine("peer check: no close or reset observed");
        }
        catch (WseException failure)
        {
            Console.WriteLine(
                $"peer check: peer reported code={(TransportErrorCode)failure.Code}");
        }

        // Disconnect is idempotent and terminal; Dispose would do the same.
        client.Disconnect();
        Console.WriteLine("disconnected");
        return 0;
    }

    /// <summary>Prints the structured fields of a transport failure. Never branch on the text.</summary>
    /// <param name="operation">The operation that failed.</param>
    /// <param name="failure">The structured failure raised by the binding.</param>
    // Category is the portable classification shared by every binding, Code is the XPT-specific
    // TransportErrorCode, and NativeCode is the raw operating-system number. Only the message is
    // free text, and it is the one field a caller must not parse.
    private static void Report(string operation, WseException failure)
    {
        Console.WriteLine(
            $"{operation} failed: category={failure.Category}"
            + $" code={(TransportErrorCode)failure.Code} native={failure.NativeCode}"
            + $" message={failure.Message}");
    }
}

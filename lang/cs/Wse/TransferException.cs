using System;

namespace WapitiStew.Wse;

/// <summary>
/// A failed send or truncated UDP receive with known progress. Still catchable as WseException.
/// Progress describes completed native operations, not peer acknowledgement or permission to replay.
/// </summary>
public sealed class TransferException : WseException
{
    internal TransferException(NativeStatus status, string message, ulong bytesTransferred,
        byte[]? receivedData = null, Endpoint? source = null)
        : base((ErrorCategory)status.Category, status.Code, status.NativeCode, message)
    {
        BytesTransferred = bytesTransferred;
        ReceivedData = receivedData;
        SourceEndpoint = source;
    }

    /// <summary>
    /// Known bytes sent, or the retained UDP prefix length. Zero cannot prove absence of wire effects
    /// after a native exception or cancelled pending serial write. Never automatically resend a suffix.
    /// </summary>
    public ulong BytesTransferred { get; }

    /// <summary>
    /// Detached managed UDP prefix, including embedded zero bytes; null for a send failure.
    /// No native owner remains attached to the exception and no Dispose is required.
    /// </summary>
    public byte[]? ReceivedData { get; }

    /// <summary>The source of a truncated UDP datagram, or null for a send failure.</summary>
    public Endpoint? SourceEndpoint { get; }
}

internal static class TransferMarshal
{
    internal static int CompleteSend(NativeStatus status, nuint sent)
    {
        if (status.Category == (int)ErrorCategory.None) return checked((int)sent);
        string message = NativeMethods.LastErrorMessage();
        NativeOutput.Checkpoint("transfer-exception");
        throw new TransferException(status, message, (ulong)sent);
    }

    internal static UdpDatagram CompleteReceive(NativeStatus status, NativeOutput output)
    {
        if (status.Category == (int)ErrorCategory.None) return new UdpDatagram(output);
        // Getters overwrite the thread-local diagnostic, so save it before any getter/cleanup.
        string message = NativeMethods.LastErrorMessage();
        if (output.Value == IntPtr.Zero)
            throw new WseException((ErrorCategory)status.Category, status.Code, status.NativeCode, message);

        using var datagram = new UdpDatagram(output);
        NativeOutput.Checkpoint("transfer-payload");
        byte[] prefix = datagram.Payload();
        NativeOutput.Checkpoint("transfer-source");
        Endpoint source = datagram.Source;
        NativeOutput.Checkpoint("transfer-exception");
        throw new TransferException(status, message, (ulong)prefix.Length, prefix, source);
    }
}

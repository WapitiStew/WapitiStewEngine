//*****************************************************************************************************************
//!
//! @file    SerialSendReceiveExample.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese XPT Serial Portの8N1固定FramingでのOpen、Send、ReceiveおよびClose.
//! @brief   \~english  XPT serial port open with fixed 8N1 framing, send, receive, and close.
//!
//! @details
//!     \~japanese
//!     @n 引数順序は arguments[0] がNative Library Path、arguments[1] がDevice名、arguments[2] が
//!        Baud Rateである。Device名とBaud Rateが無い場合は、その旨を出力して0を返す。
//!     @n Device名はWindowsでは "COM3"、Linuxでは "/dev/ttyUSB0" の形式である。Baud Rateは
//!        1200／2400／4800／9600／19200／38400／57600／115200のいずれかである。
//!     @n 実Deviceが無い場合も失敗しない。構造化されたOpenFailedを出力して0を返す。
//!     @n XPTはDefault Timeoutを持たない。全OperationがOperationContextを要求する。
//!     \~english
//!     @n Argument order: arguments[0] is the native library path, arguments[1] is the device name,
//!        and arguments[2] is the baud rate. Without a device name and a baud rate the sample
//!        explains that and returns zero.
//!     @n Windows uses "COM3"-style names and Linux uses "/dev/ttyUSB0"-style paths. The baud rate
//!        is one of 1200/2400/4800/9600/19200/38400/57600/115200.
//!     @n A machine without the physical device does not fail: the structured OpenFailed is printed
//!        and the sample still returns zero, which is the documented no-device behaviour.
//!     @n XPT has no default timeout. Every operation requires an OperationContext.
//!     @n The framing is fixed at eight data bits, no parity, one stop bit. Open takes a device
//!        name and a baud rate and nothing else, so there is no parity or stop-bit argument to
//!        get wrong, and no way to talk to a device that needs different framing.
//!     @n The portable send and receive pair below is the whole serial surface of this binding.
//!     @n The C++ counterpart reports every failure through the WSE log. No log facility is bound,
//!        so this sample prints the same structured fields to the console instead.
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

/// <summary>XPT serial port send and receive against a caller-named device.</summary>
public static class SerialSendReceiveExample
{
    /// <summary>Largest reply this sample reads in one receive.</summary>
    // One receive returns whatever has arrived, up to this many bytes; it does not wait for the
    // buffer to fill. A device that answers with more simply needs a further call, so the ceiling
    // is a per-call allocation bound rather than a protocol limit.
    private const int ReceiveCeiling = 256;

    /// <summary>Opens a serial port, sends one payload, and reads whatever answers.</summary>
    /// <param name="arguments">
    /// arguments[0] is the optional path to the native library file, arguments[1] is the device
    /// name, and arguments[2] is the baud rate.
    /// </param>
    /// <returns>Zero on success and when no usable device was named.</returns>
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

        // Naming no device is not an error. The sample has to run on a build machine with no
        // serial hardware, so it explains what it wanted and returns success.
        if (arguments.Length < 3)
        {
            Console.WriteLine(
                "No serial device was named. Pass the native library path, then a device name and"
                + " a baud rate, for example: SerialSendReceiveExample <library> COM3 115200");
            return 0;
        }
        // Only the sign and the shape are checked here. Whether the platform accepts this
        // particular rate is the driver's decision, and a rejected one surfaces from Open as a
        // structured transport failure rather than as a bad argument.
        if (!int.TryParse(
                arguments[2], NumberStyles.Integer, CultureInfo.InvariantCulture, out int baudRate)
            || baudRate <= 0)
        {
            Console.WriteLine($"'{arguments[2]}' is not a positive baud rate.");
            return 0;
        }

        // One explicit deadline is reused for every operation in this sample.
        // A second covers opening the device and clocking fifteen bytes out at any of the usual
        // rates, and it bounds the receive below, where no answer is the normal outcome. A device
        // that answers slowly, or a much lower baud rate, wants a longer one.
        var context = new OperationContext(TimeSpan.FromSeconds(1));
        string deviceName = arguments[1];

        // The port owns one native device handle through its SafeHandle. The using releases it on
        // every path out, so the explicit Close calls below only make the ordering visible.
        using var port = new SerialPort();
        try
        {
            port.Open(deviceName, baudRate, context);
        }
        catch (WseException failure)
        {
            // No device on this machine is a reportable state, not a sample defect.
            Report($"open {deviceName}", failure);
            Console.WriteLine("name a serial port that exists on this machine");
            return 0;
        }

        Console.WriteLine($"opened {port.DeviceName} at {port.BaudRate} baud (8N1)");
        Console.WriteLine($"is open: {port.IsOpen}");

        // The trailing carriage return terminates the line for the many devices that frame on CR.
        // Send writes the whole buffer or fails, so the count printed always equals its length.
        byte[] payload = Encoding.UTF8.GetBytes("WSE-XPT-SERIAL\r");
        try
        {
            int sent = port.Send(payload, context);
            Console.WriteLine($"sent bytes: {sent}");
        }
        catch (WseException failure)
        {
            // A device that opened and then refused the write is a real failure, unlike the
            // missing-device case above, so this one is reported through the exit code.
            Report("send", failure);
            port.Close();
            return 2;
        }

        // One receive returns whatever the peer produced within the deadline. Without a loopback
        // plug or an answering device, TimedOut is the expected structured result.
        // That is why TimedOut is caught by its code in its own filter and reported as an ordinary
        // outcome; every other code falls through to the failure handler below.
        try
        {
            byte[] received = port.Receive(ReceiveCeiling, context);
            Console.WriteLine(
                $"received {received.Length} bytes: {Encoding.UTF8.GetString(received)}");
        }
        catch (WseException failure) when (failure.Code == (int)TransportErrorCode.TimedOut)
        {
            Console.WriteLine("no reply within the deadline (expected without an answering device)");
        }
        catch (WseException failure)
        {
            Report("receive", failure);
            port.Close();
            return 3;
        }

        // Close is idempotent and terminal; Dispose would do the same.
        port.Close();
        Console.WriteLine("closed");
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

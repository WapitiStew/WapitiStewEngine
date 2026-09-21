//*****************************************************************************************************************
//!
//! @file    BindingContract.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese C# Bindingの公開契約とOwnership契約を検証する.
//! @brief   \~english  Verifies the public and ownership contracts of the C# binding.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Diagnostics;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using WapitiStew.Wse;

/// <summary>Runtime contract test for the WonderStewEngine C# binding.</summary>
public static class BindingContract
{
    /// <summary>Runs the contract assertions.</summary>
    /// <param name="arguments">Path to the native library file.</param>
    /// <returns>Zero when every assertion passed.</returns>
    public static int Main(string[] arguments)
    {
        if (arguments.Length < 1)
        {
            Console.Error.WriteLine("Pass the WSE native C ABI library path.");
            return 2;
        }
        WseRuntime.SetNativeLibraryPath(arguments[0]);
        if (arguments.Length > 1 && arguments[1] == "--ownership")
            return OwnershipContract.Verify(arguments[0]);
        if (arguments.Length > 1 && arguments[1] == "--transfer")
            return TransferContract.Verify(arguments[0]);

        Assert(WseRuntime.NativeAbiVersion == 1, "The flat C ABI version must be 1.");
        if (arguments.Length < 2)
        {
            throw new ArgumentException("Pass the C layout fixture executable as the second argument.");
        }
        CapiContract.Verify(arguments[1]);

        using (var runtime = new WseRuntime())
        {
            RuntimeInfo info = runtime.Info();
            Assert(
                Regex.IsMatch(info.Version, @"^\d+\.\d+\.\d+$"),
                "The runtime version must be a semantic version.");
            Assert(info.BindingAbiVersion == 1, "The binding ABI version must be 1.");

            VerifyCopyFrame(runtime);
            VerifyWait(runtime);
            VerifyCancellation(runtime);
            VerifyArgumentContracts(runtime);

            if (info.Components.HasIui)
            {
                VerifyKeyboard();
            }

            if (info.Components.HasXpt)
            {
                VerifyUdpLoopback();
                TransferContract.VerifyLoopback();
                VerifySerialAndHttpContracts();
                foreach (int status in new[] { 200, 404, 503 })
                {
                    VerifyHttpResponse(status, authenticated: false);
                    VerifyHttpResponse(status, authenticated: true);
                }
            }

            if (info.Components.HasOui)
            {
                VerifyProjection();
            }

            if (info.Components.HasTmr)
            {
                VerifyWebCamera();
                VerifyFrameOperations();
            }

            if (info.Components.HasVpj)
            {
                // The contract for an access-controlled component ships with that component, so
                // this type may not be compiled in. Naming it directly would make this file
                // uncompilable without it. The runtime says the component is present, so its
                // contract has to be there: a miss is a failure.
                System.Type? contract = System.Type.GetType("VpjContract");
                System.Reflection.MethodInfo? verify = contract?.GetMethod("Verify");
                Assert(verify is not null,
                    "the projector contract is present when the runtime reports the component");
                verify!.Invoke(null, new object[]
                {
                    (System.Action<bool, string>)Assert,
                    (System.Func<System.Type, System.Action, string, System.Exception>)
                        AssertThrowsType,
                });
            }
        }

        VerifyDisposedContract();

        Console.WriteLine("C# binding contract passed.");
        return 0;
    }

    private static void VerifyCopyFrame(WseRuntime runtime)
    {
        byte[] source = { 0, 1, 127, 255 };
        using (FrameBuffer copied = runtime.CopyFrame(source))
        {
            Assert(copied.Size == source.Length, "The copied buffer must keep its length.");

            // The runtime must not retain caller memory.
            source[1] = 99;
            byte[] roundTrip = copied.ToArray();
            AssertSequenceEqual(new byte[] { 0, 1, 127, 255 }, roundTrip);
        }

        using (FrameBuffer empty = runtime.CopyFrame(Array.Empty<byte>()))
        {
            Assert(empty.Size == 0, "An empty source must produce an empty buffer.");
            Assert(empty.IsEmpty, "An empty buffer must report IsEmpty.");
            Assert(empty.ToArray().Length == 0, "An empty buffer must copy zero bytes.");
        }

        for (int index = 0; index < 50; ++index)
        {
            using FrameBuffer frame = runtime.CopyFrame(new byte[] { (byte)index });
            Assert(frame.ToArray()[0] == (byte)index, "Repeated copies must stay independent.");
        }
    }

    private static void VerifyWait(WseRuntime runtime)
    {
        var stopwatch = Stopwatch.StartNew();
        runtime.Wait(TimeSpan.FromMilliseconds(20));
        stopwatch.Stop();
        Assert(stopwatch.ElapsedMilliseconds >= 10, "A finite wait must actually elapse.");

        runtime.Wait(TimeSpan.Zero);
    }

    private static void VerifyCancellation(WseRuntime runtime)
    {
        using var cancellation = new CancellationSource();
        Assert(!cancellation.IsCancellationRequested, "A new source must not be cancelled.");

        cancellation.Cancel();
        Assert(cancellation.IsCancellationRequested, "Cancel must be observable.");
        cancellation.Cancel();

        WseException failure = AssertThrows<WseException>(
            () => runtime.Wait(TimeSpan.FromSeconds(5), cancellation),
            "A cancelled wait must report a failure.");
        Assert(
            failure.Category == ErrorCategory.Cancellation,
            "A cancelled wait must report the cancellation category.");

        using var pending = new CancellationSource();
        runtime.Wait(TimeSpan.FromMilliseconds(5), pending);
    }

    private static void VerifyArgumentContracts(WseRuntime runtime)
    {
        AssertThrows<ArgumentOutOfRangeException>(
            () => runtime.Wait(TimeSpan.FromMilliseconds(-1)),
            "A negative wait must be rejected.");
        AssertThrows<ArgumentNullException>(
            () => runtime.CopyFrame(null!),
            "A null source must be rejected.");
    }

    private static void VerifyKeyboard()
    {
        // Hardware-free: a machine without a readable keyboard must report that through the
        // readiness state and a structured error, never through a crash or a silent empty snapshot.
        using (var keyboard = new Keyboard())
        {
            KeyboardAccessState state = keyboard.AccessState;
            Assert(
                Enum.IsDefined(typeof(KeyboardAccessState), state),
                "The keyboard readiness state must be a defined value.");
            // Each observer samples independently; startup/hotplug may occur between calls.
            _ = keyboard.IsAvailable;
            try
            {
                KeyboardState snapshot = keyboard.Snapshot();
                Assert(
                    snapshot.Ascii.Length == KeyboardState.AsciiCount
                    && snapshot.Function.Length == KeyboardState.FunctionCount
                    && snapshot.Arrow.Length == KeyboardState.ArrowCount
                    && snapshot.Lock.Length == KeyboardState.LockCount
                    && snapshot.Command.Length == KeyboardState.CommandCount,
                    "Each key group must keep its documented length.");
            }
            catch (WseException failure)
            {
                VerifyKeyboardFailure(failure);
            }
            try { keyboard.PressedAscii(); }
            catch (WseException failure) { VerifyKeyboardFailure(failure); }
        }

        var disposed = new Keyboard();
        disposed.Dispose();
        disposed.Dispose();
        AssertThrows<ObjectDisposedException>(
            () => disposed.Snapshot(),
            "A disposed keyboard must be rejected.");
    }

    private static void VerifyKeyboardFailure(WseException failure)
    {
        // The error's own code/category form one observation; a preceding AccessState does not.
        ErrorCategory expected = (KeyboardAccessState)failure.Code switch
        {
            KeyboardAccessState.Starting => ErrorCategory.InvalidState,
            KeyboardAccessState.Unavailable => ErrorCategory.NotFound,
            KeyboardAccessState.PermissionDenied => ErrorCategory.Security,
            KeyboardAccessState.Disconnected => ErrorCategory.InputOutput,
            _ => throw new InvalidOperationException("Invalid failing keyboard state.")
        };
        Assert(failure.Category == expected, "The keyboard failure must preserve its own state/category mapping.");
    }

    private static void VerifyUdpLoopback()
    {
        // Hardware-free: both endpoints are loopback sockets inside this process.
        var context = new OperationContext(TimeSpan.FromSeconds(1));

        using var receiver = new UdpClient();
        receiver.Bind(new Endpoint("127.0.0.1", 0), context);
        Assert(receiver.IsOpen, "A bound UDP socket must report itself open.");

        Endpoint local = receiver.LocalEndpoint;
        Assert(local.Port != 0, "Binding to port zero must report the assigned port.");
        Assert(local.IsValid, "The bound endpoint must be valid.");

        using var sender = new UdpClient();
        sender.Bind(new Endpoint("127.0.0.1", 0), context);

        byte[] payload = { 1, 2, 3, 4, 5 };
        int sent = sender.SendTo(local, payload, context);
        Assert(sent == payload.Length, "sendTo must report every byte sent.");

        using (UdpDatagram datagram = receiver.ReceiveFrom(UdpClient.MaximumDatagramSize, context))
        {
            AssertSequenceEqual(payload, datagram.Payload());
            Assert(datagram.Source.Port == sender.LocalEndpoint.Port, "The source port must match.");
        }

        // XPT has no default timeout, so an idle receive must report TimedOut rather than block.
        var shortContext = new OperationContext(TimeSpan.FromMilliseconds(50));
        WseException timeout = AssertThrows<WseException>(
            () => receiver.ReceiveFrom(UdpClient.MaximumDatagramSize, shortContext),
            "An idle receive must fail once its deadline elapses.");
        Assert(
            timeout.Code == (int)TransportErrorCode.TimedOut,
            "An elapsed deadline must report the TimedOut transport code.");
        Assert(
            timeout.Category == ErrorCategory.Timeout,
            "An elapsed deadline must normalize to the Timeout category.");

        // A cancelled context must be observed cooperatively rather than ignored.
        using var cancellation = new CancellationSource();
        cancellation.Cancel();
        var cancelledContext = new OperationContext(TimeSpan.FromSeconds(5), cancellation);
        WseException cancelled = AssertThrows<WseException>(
            () => receiver.ReceiveFrom(UdpClient.MaximumDatagramSize, cancelledContext),
            "A cancelled receive must fail.");
        Assert(
            cancelled.Code == (int)TransportErrorCode.Cancelled,
            "A cancelled receive must report the Cancelled transport code.");

        receiver.Close();
        Assert(!receiver.IsOpen, "close must be observable.");
        receiver.Close();
    }

    private static void VerifySerialAndHttpContracts()
    {
        var context = new OperationContext(TimeSpan.FromMilliseconds(200));

        // No serial hardware is required: opening a device that cannot exist must report a
        // structured failure rather than crash.
        using (var port = new SerialPort())
        {
            Assert(!port.IsOpen, "A new serial port must be closed.");
            WseException failure = AssertThrows<WseException>(
                () => port.Open("WSE_NONEXISTENT_PORT", 9600, context),
                "Opening a missing serial device must fail.");
            Assert(failure.Category != ErrorCategory.None, "The failure must carry a category.");
            Assert(!port.IsOpen, "A failed open must leave the port closed.");
            port.Close();
        }

        // A transport failure must not fabricate a completed HTTP response.
        using (var request = new HttpRequest(HttpMethod.Post, "http://127.0.0.1:1/wse"))
        {
            request.AddHeader("Content-Type", "application/octet-stream");
            request.SetBody(new byte[] { 0, 1, 2 });

            WseException failure = AssertThrows<WseException>(
                () => HttpClient.Execute(request, 4096, context),
                "A request to a closed port must fail.");
            Assert(failure.Category != ErrorCategory.None, "The failure must carry a category.");
            Assert(failure is not HttpStatusException,
                "A refused connection must not carry an HTTP response.");
        }

        AssertThrows<ArgumentNullException>(
            () => new HttpRequest(HttpMethod.Get, null!),
            "A null URL must be rejected.");
        AssertThrows<ArgumentOutOfRangeException>(
            () => new OperationContext(TimeSpan.FromMilliseconds(-1)),
            "A negative deadline must be rejected.");
    }

    /// <summary>Checks both HTTP result states and native response ownership on loopback.</summary>
    private static void VerifyHttpResponse(int status, bool authenticated)
    {
        byte[] body = { 0, 1, 127, 255 };
        using var listener = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 0);
        listener.Start();
        int port = ((System.Net.IPEndPoint)listener.LocalEndpoint).Port;
        using var deadline = new System.Threading.CancellationTokenSource(TimeSpan.FromSeconds(5));
        System.Threading.Tasks.Task server = System.Threading.Tasks.Task.Run(async () =>
        {
            using var peer = await listener.AcceptTcpClientAsync(deadline.Token);
            using var stream = peer.GetStream();
            byte[] buffer = new byte[4096];
            string requestHead = "";
            while (!requestHead.Contains("\r\n\r\n", StringComparison.Ordinal))
            {
                int count = await stream.ReadAsync(buffer.AsMemory(), deadline.Token);
                Assert(count != 0, "The client must send a complete HTTP request.");
                requestHead += System.Text.Encoding.ASCII.GetString(buffer, 0, count);
            }
            byte[] head = System.Text.Encoding.ASCII.GetBytes(
                $"HTTP/1.1 {status} Test\r\nContent-Length: {body.Length}\r\n"
                + "Content-Type: application/octet-stream\r\nConnection: close\r\n\r\n");
            await stream.WriteAsync(head.AsMemory(), deadline.Token);
            await stream.WriteAsync(body.AsMemory(), deadline.Token);
        });

        try
        {
            using var request = new HttpRequest(HttpMethod.Get, $"http://127.0.0.1:{port}/result");
            var context = new OperationContext(TimeSpan.FromSeconds(5));
            HttpResponse response;
            try
            {
                response = authenticated
                    ? HttpClient.ExecuteAuthenticated(request, 4096, "wse", "secret", context)
                    : HttpClient.Execute(request, 4096, context);
            }
            catch (HttpStatusException failure)
            {
                using HttpResponse carried = failure.Response;
                Assert(status >= 400, "Only a failure status may raise HttpStatusException.");
                Assert(failure.Code == (int)TransportErrorCode.HttpStatusError,
                    "The stable HTTP error code must survive.");
                Assert(failure.NativeCode == status, "The native HTTP status must survive.");
                VerifyHttpResponseValue(carried, status, body);
                return;
            }
            using (response)
            {
                Assert(status == 200, "An HTTP status failure must raise its error.");
                VerifyHttpResponseValue(response, status, body);
            }
        }
        finally
        {
            server.GetAwaiter().GetResult();
        }
    }

    /// <summary>Reads a response before its deterministic disposal on either result path.</summary>
    private static void VerifyHttpResponseValue(HttpResponse response, int status, byte[] body)
    {
        Assert(response.StatusCode == status, "The response status must survive.");
        Assert(response.AttemptCount == 1, "The binding must not retry.");
        AssertSequenceEqual(body, response.Body());
        Assert(response.Headers().Count >= 2, "Response headers must remain readable.");
    }

    private static void VerifyProjection()
    {
        // Hardware-free: the software adapter renders without a discrete GPU.
        byte[] source =
        {
            255, 0, 0, 255,   0, 255, 0, 255,
            0, 0, 255, 255,   255, 255, 255, 255,
        };
        var layer = new ProjectionLayer(
            2,
            2,
            source,
            new List<ProjectionVertex>
            {
                new(-1.0f, 1.0f, 0.0f, 0.0f),
                new(1.0f, 1.0f, 1.0f, 0.0f),
                new(-1.0f, -1.0f, 0.0f, 1.0f),
                new(1.0f, -1.0f, 1.0f, 1.0f),
            },
            new List<int> { 0, 1, 2, 2, 1, 3})
        {
            SamplingFilter = TextureSamplingFilter.Nearest,
        };
        var request = new ProjectionRequest(4, 4, new List<ProjectionLayer> { layer })
        {
            Backend = OperatingSystem.IsWindows()
                ? RendererBackend.Direct3D12
                : RendererBackend.Vulkan12,
            UseSoftwareAdapter = true,
        };

        using (ProjectionFrame frame = Projection.Render(request))
        {
            Assert(frame.Width == 4 && frame.Height == 4, "The frame must match the requested size.");
            Assert(frame.RowPitch == 16, "The row pitch must be four bytes per pixel.");
            // Which adapter drew the frame is reported whether or not one was named.
            Assert(
                !string.IsNullOrEmpty(frame.AdapterName),
                "The adapter that drew the frame must be named.");
            byte[] pixels = frame.ToArray();
            Assert(pixels.Length == 64, "The frame must hold width * height * 4 bytes.");
            AssertSequenceEqual(new byte[] { 255, 0, 0, 255 }, pixels[0..4]);
        }

        AssertThrows<ArgumentException>(
            () => Projection.Render(new ProjectionRequest(4, 4, new List<ProjectionLayer>())),
            "A projection without layers must be rejected.");
    }

    private static void VerifyFrameOperations()
    {
        // The formats added for a Bayer sensor reach C# with the same names the C++ side uses.
        Assert((int)CameraPixelFormat.Bayer16Gbrg == 14, "Bayer formats must be appended.");
        Assert((int)CameraPixelFormat.Uyvy422 == 15, "UYVY must retain its own byte layout.");
        Assert((int)CameraPixelFormat.Gray16 == 8, "The wide grey format must be appended.");
        Assert((int)CameraControl.FrameRate == 18, "The frame-rate control must be appended.");
        Assert((int)ImageOrientation.Rotate90Cw == 1, "Orientation must match the engine.");
        Assert((int)DemosaicMethod.Block2x2 == 0, "Demosaic method must match the engine.");

        // Orientation moves whole pixels; averaging combines a site with itself. A Bayer frame
        // therefore refuses the first and accepts the second, so the two questions differ.
        Assert(
            CameraFrameOps.IsFrameOperationSupported(CameraPixelFormat.Rgb8),
            "A colour frame can be reoriented.");
        Assert(
            !CameraFrameOps.IsFrameOperationSupported(CameraPixelFormat.Bayer16Rggb),
            "A Bayer frame cannot be reoriented.");
        Assert(
            CameraFrameOps.IsFrameAveragingSupported(CameraPixelFormat.Bayer16Rggb),
            "A Bayer frame can be averaged.");
        Assert(
            !CameraFrameOps.IsFrameAveragingSupported(CameraPixelFormat.Mjpeg),
            "A compressed frame cannot be averaged.");
        Assert(
            CameraFrameOps.IsBayerFormat(CameraPixelFormat.Bayer16Bggr),
            "A Bayer format must answer the Bayer question.");
        Assert(
            CameraFrameOps.BayerPatternOf(CameraPixelFormat.Bayer16Grbg) == BayerPattern.Grbg,
            "A Bayer format must report its own layout.");
        WseException failure = AssertThrows<WseException>(
            () => CameraFrameOps.BayerPatternOf(CameraPixelFormat.Rgb8),
            "A format with no Bayer layout must fail.");
        Assert(
            failure.Category != ErrorCategory.None,
            "A refused Bayer question must report a failure category.");

        // An accumulator with nothing in it has no average to give.
        using var accumulator = new CameraFrameAccumulator();
        Assert(accumulator.Count == 0, "A new accumulator holds nothing.");
        AssertThrows<WseException>(
            () => accumulator.Average(),
            "An empty accumulator must not produce an average.");
        AssertThrows<ArgumentNullException>(
            () => accumulator.Add(null!),
            "A null frame must be rejected.");
        accumulator.Reset();
        Assert(accumulator.Count == 0, "Reset leaves the accumulator empty.");

        // Reading an average needs a camera that is streaming, which no machine here guarantees;
        // what is checked without one is that the argument guard fires before the device does.
        using var camera = new WebCamera();
        AssertThrows<ArgumentOutOfRangeException>(
            () => camera.ReadAveragedFrame(0, TimeSpan.FromMilliseconds(50)),
            "Averaging zero frames must be rejected.");
        AssertThrows<ArgumentOutOfRangeException>(
            () => camera.ReadAveragedFrame(2, TimeSpan.FromMilliseconds(-1)),
            "A negative frame timeout must be rejected.");
    }

    private static void VerifyWebCamera()
    {
        // Hardware-free: a machine with no camera must enumerate an empty list rather than fail,
        // and an unopened camera must reject frame reads with a structured error.
        IReadOnlyList<CameraDevice> devices = WebCamera.Enumerate();
        try
        {
            foreach (CameraDevice device in devices)
            {
                Assert(device.Id.Length > 0, "Every device must report a stable id.");
                Assert(
                    Enum.IsDefined(typeof(CameraBackend), device.Backend),
                    "Every device must report a defined backend.");
                Assert(
                    Enum.IsDefined(typeof(CameraTransport), device.TransportType),
                    "Every device must report a defined transport.");

                using CameraCapability capability = WebCamera.Capabilities(device);
                foreach (CameraFormat format in capability.Formats())
                {
                    Assert(format.Width > 0 && format.Height > 0, "A format must have a size.");
                }
                foreach (CameraStreamProfile profile in capability.StreamProfiles())
                {
                    Assert(
                        profile.NativeFormat.Width > 0,
                        "A stream profile must advertise a native size.");
                }
                foreach (CameraControlCapability control in capability.Controls())
                {
                    Assert(
                        control.Minimum <= control.Maximum,
                        "A control range must not be inverted.");
                }
                foreach (CameraExtensionUnitSelector selector in capability.ExtensionUnits())
                {
                    Assert(selector.Guid().Length == 16, "A unit GUID must hold 16 bytes.");
                    selector.Dispose();
                }
            }

            using (var camera = new WebCamera())
            {
                Assert(!camera.IsOpen, "A new camera must not be open.");
                Assert(!camera.IsStreaming, "A new camera must not be streaming.");

                WseException failure = AssertThrows<WseException>(
                    () => camera.ReadFrame(TimeSpan.FromMilliseconds(50)),
                    "Reading from an unopened camera must fail.");
                Assert(
                    failure.Category != ErrorCategory.None,
                    "An unopened camera must report a failure category.");

                AssertThrows<ArgumentOutOfRangeException>(
                    () => camera.ReadFrame(TimeSpan.FromMilliseconds(-1)),
                    "A negative frame timeout must be rejected.");
                AssertThrows<ArgumentNullException>(
                    () => camera.Open(null!),
                    "A null device must be rejected.");

                // Close is terminal and idempotent even when nothing was opened.
                camera.Close();
                camera.Close();
            }
        }
        finally
        {
            foreach (CameraDevice device in devices)
            {
                device.Dispose();
            }
        }
    }

    private static void VerifyDisposedContract()
    {
        var runtime = new WseRuntime();
        runtime.Dispose();
        runtime.Dispose();
        AssertThrows<ObjectDisposedException>(
            () => runtime.Info(),
            "A disposed runtime must be rejected.");

        var cancellation = new CancellationSource();
        cancellation.Dispose();
        cancellation.Dispose();
        AssertThrows<ObjectDisposedException>(
            () => cancellation.Cancel(),
            "A disposed cancellation source must be rejected.");
    }

    //! Type-erased wrapper so a separate contract file can reuse the assertion helpers.
    private static Exception AssertThrowsType(Type expected, Action action, string message)
    {
        try
        {
            action();
        }
        catch (Exception failure) when (expected.IsInstanceOfType(failure))
        {
            return failure;
        }
        throw new InvalidOperationException(message);
    }

    private static void Assert(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException(message);
        }
    }

    private static void AssertSequenceEqual(byte[] expected, byte[] actual)
    {
        Assert(expected.Length == actual.Length, "Byte sequences must have the same length.");
        for (int index = 0; index < expected.Length; ++index)
        {
            Assert(expected[index] == actual[index], $"Byte {index} must match.");
        }
    }

    private static TException AssertThrows<TException>(Action action, string message)
        where TException : Exception
    {
        try
        {
            action();
        }
        catch (TException expected)
        {
            return expected;
        }
        throw new InvalidOperationException(message);
    }
}

//*****************************************************************************************************************
//!
//! @file    NativeMethods.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese 平坦C ABIへのP/Invoke宣言とNative Library解決.
//! @brief   \~english  P/Invoke declarations for the flat C ABI and native-library resolution.
//!
//! @details
//!     \~japanese
//!     @n Native Libraryは、明示Path、環境変数`WSE_CAPI_LIBRARY`、既定名の順で解決する。
//!        Node/Python/Java Bindingが実行時にRuntime Pathを受け取れる契約と同じ意図である。
//!     \~english
//!     @n The native library resolves in order: an explicit path, the `WSE_CAPI_LIBRARY`
//!        environment variable, then the default name. This mirrors the runtime-path contract
//!        already offered by the Node, Python, and Java bindings.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.IO;
using System.Runtime.InteropServices;

namespace WapitiStew.Wse;

/// <summary>Error contract returned by every flat C ABI function.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeStatus
{
    public int Category;
    public int Code;
    public long NativeCode;
}

/// <summary>Availability of the optional components compiled into the loaded native build.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeComponents
{
    public int HasXpt;
    public int HasTmr;
    public int HasOui;
    public int HasGef;
    public int HasIui;
    public int HasVpj;
}

/// <summary>Native layout of one projection vertex.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeProjectionVertex
{
    public float PositionX;
    public float PositionY;
    public float TextureU;
    public float TextureV;
}

/// <summary>Native layout of a linear RGBA clear color.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeRendererColor
{
    public float Red;
    public float Green;
    public float Blue;
    public float Alpha;
}

/// <summary>Native layout of the edge-blend widths.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeEdgeBlend
{
    public float Left;
    public float Right;
    public float Top;
    public float Bottom;
    public int Curve;
}

/// <summary>Native layout of one projection layer. Pointers are valid for the call only.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeProjectionLayer
{
    public uint Width;
    public uint Height;
    public IntPtr Rgba;
    public nuint RgbaSize;
    public IntPtr Alpha;
    public nuint AlphaSize;
    public IntPtr Vertices;
    public nuint VertexCount;
    public IntPtr Indices;
    public nuint IndexCount;
    public int SamplingFilter;
    public float Opacity;
    public NativeEdgeBlend EdgeBlend;
}

/// <summary>Native layout of a projection render request.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeProjectionRequest
{
    public uint OutputWidth;
    public uint OutputHeight;
    public NativeRendererColor ClearColor;
    public uint SupersampleScale;
    public int Backend;
    public int UseSoftwareAdapter;
    public int EnableValidation;
    public IntPtr AdapterName;
    public uint TimeoutMilliseconds;
    public IntPtr Layers;
    public nuint LayerCount;
}

/// <summary>Native layout of the rendered frame dimensions.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeProjectionFrame
{
    public uint Width;
    public uint Height;
    public nuint RowPitch;

    // The name is held inline rather than behind a handle, so the marshaller copies it as a
    // fixed-size UTF-8 array and the managed side reads up to the terminator.
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = AdapterNameCapacity)]
    public byte[] AdapterName;

    internal const int AdapterNameCapacity = 256;
}

/// <summary>Native layout of the explicit controls required by every XPT operation.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeOperationContext
{
    public long TimeoutMilliseconds;
    public IntPtr Cancellation;
}

/// <summary>Native layout of one camera stream format.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeCameraFormat
{
    public uint Width;
    public uint Height;
    public uint FrameRateNumerator;
    public uint FrameRateDenominator;
    public int PixelFormat;
}

/// <summary>Native layout of a camera stream configuration.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeCameraStreamConfiguration
{
    public NativeCameraFormat NativeFormat;
    public int OutputFormat;
    public int AllowConversion;
}

/// <summary>Native layout of one camera control value.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeCameraControlValue
{
    public int Control;
    public int Mode;
    public long Value;
}

/// <summary>Native layout of one camera control capability.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeCameraControlCapability
{
    public int Control;
    public long Minimum;
    public long Maximum;
    public long Step;
    public long DefaultValue;
    public int SupportsManual;
    public int SupportsAutomatic;
    public int Unit;
    public double PhysicalScale;
    public int Readable;
    public int Writable;
}

/// <summary>Native layout of a USB identity.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeCameraUsbIdentity
{
    public ushort VendorId;
    public ushort ProductId;
    public ushort UvcVersionBcd;
    public int Available;
}

/// <summary>Native layout of a frame description.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeCameraFrameDescription
{
    public uint Width;
    public uint Height;
    public int PixelFormat;
    public nuint RowStride;
}

/// <summary>Native layout of the execution constraints declared by a model profile.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeModelConstraints
{
    public uint MaximumResponseBytes;
    public uint OperationTimeoutMilliseconds;
}

/// <summary>Native layout of a control endpoint. Strings are valid for the call only.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeProfileControlEndpoint
{
    public IntPtr Address;
    public ushort Port;
    public int BaudRate;
    public IntPtr StableDeviceId;
    public uint ReconnectAttempts;
    public uint ReconnectDelayMilliseconds;
}

internal static class NativeMethods
{
    /// <summary>Default native library name, resolved per platform by the runtime.</summary>
    internal const string LibraryName = "wse_capi";

    /// <summary>Environment variable that names an explicit native library file.</summary>
    internal const string LibraryPathVariable = "WSE_CAPI_LIBRARY";

    private static readonly object s_gate = new();
    private static string? s_explicitLibraryPath;
    private static bool s_resolverRegistered;

    static NativeMethods()
    {
        RegisterResolver();
    }

    /// <summary>
    /// Sets an explicit native library file. Must be called before the first native call; the
    /// runtime caches the resolved handle for the lifetime of the process.
    /// </summary>
    /// <param name="path">Full path to the native library file.</param>
    internal static void SetLibraryPath(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            throw new ArgumentException("The native library path must not be empty.", nameof(path));
        }
        if (!File.Exists(path))
        {
            throw new FileNotFoundException("The native library file does not exist.", path);
        }

        lock (s_gate)
        {
            s_explicitLibraryPath = path;
        }
        RegisterResolver();
    }

    private static void RegisterResolver()
    {
        lock (s_gate)
        {
            if (s_resolverRegistered)
            {
                return;
            }
            NativeLibrary.SetDllImportResolver(typeof(NativeMethods).Assembly, Resolve);
            s_resolverRegistered = true;
        }
    }

    private static IntPtr Resolve(string libraryName, System.Reflection.Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (!string.Equals(libraryName, LibraryName, StringComparison.Ordinal))
        {
            return IntPtr.Zero;
        }

        string? candidate;
        lock (s_gate)
        {
            candidate = s_explicitLibraryPath;
        }
        candidate ??= Environment.GetEnvironmentVariable(LibraryPathVariable);

        if (!string.IsNullOrWhiteSpace(candidate) && NativeLibrary.TryLoad(candidate, out IntPtr handle))
        {
            return handle;
        }

        // Fall through to the default probing order.
        return IntPtr.Zero;
    }

    /// <summary>Reads the thread-local message describing the most recent failure.</summary>
    internal static string LastErrorMessage()
    {
        IntPtr text = wse_capi_last_error_message();
        return text == IntPtr.Zero ? string.Empty : (Marshal.PtrToStringUTF8(text) ?? string.Empty);
    }

    /// <summary>Throws <see cref="WseException"/> when <paramref name="status"/> reports a failure.</summary>
    internal static void ThrowIfFailed(NativeStatus status)
    {
        if (status.Category == (int)ErrorCategory.None)
        {
            return;
        }

        string message = LastErrorMessage();
        if (message.Length == 0)
        {
            message = "The WonderStewEngine native call failed.";
        }
        throw new WseException((ErrorCategory)status.Category, status.Code, status.NativeCode, message);
    }

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint wse_capi_abi_version();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr wse_capi_last_error_message();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_runtime_create(out IntPtr runtime);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_runtime_destroy(IntPtr runtime);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_runtime_version(
        SafeHandle runtime, byte[]? buffer, out nuint size, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_runtime_binding_abi_version(
        SafeHandle runtime, out uint version);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_runtime_components(
        SafeHandle runtime, out NativeComponents components);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_runtime_copy_frame(
        SafeHandle runtime, out IntPtr buffer, byte[]? bytes, nuint size);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_runtime_wait(
        SafeHandle runtime, long milliseconds, IntPtr cancellation);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_frame_buffer_destroy(IntPtr buffer);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_frame_buffer_size(SafeHandle buffer, out nuint size);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_frame_buffer_copy_to(
        SafeHandle buffer, byte[]? destination, out nuint written, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_cancellation_create(out IntPtr cancellation);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_cancellation_destroy(IntPtr cancellation);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_cancellation_cancel(SafeHandle cancellation);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_cancellation_is_requested(
        SafeHandle cancellation, out int requested);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_keyboard_create(out IntPtr keyboard);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_keyboard_destroy(IntPtr keyboard);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_keyboard_access_state(
        SafeHandle keyboard, out int state);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_keyboard_is_available(
        SafeHandle keyboard, out int available);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_keyboard_snapshot(
        SafeHandle keyboard, out NativeKeyboardState state);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_keyboard_pressed_ascii(
        SafeHandle keyboard, out sbyte ascii);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_render_projection(
        out NativeProjectionFrame frame,
        out IntPtr data,
        ref NativeProjectionRequest request);

    // ---------------------------------------------------------------------------------------
    // XPT: TCP
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_tcp_client_create(out IntPtr client);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_tcp_client_destroy(IntPtr client);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_tcp_client_connect(
        SafeHandle client, byte[] host, ushort port, ref NativeOperationContext context);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_tcp_client_disconnect(SafeHandle client);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_tcp_client_is_connected(
        SafeHandle client, out int connected);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_tcp_client_check_peer_connection(SafeHandle client);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_tcp_client_remote_endpoint(
        SafeHandle client, byte[]? host, out nuint size, out ushort port, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_tcp_client_local_endpoint(
        SafeHandle client, byte[]? host, out nuint size, out ushort port, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_tcp_client_send(
        SafeHandle client, out nuint sent, byte[] data, nuint size, ref NativeOperationContext context);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_tcp_client_receive(
        SafeHandle client, out IntPtr data, nuint maximumSize, ref NativeOperationContext context);

    // ---------------------------------------------------------------------------------------
    // XPT: UDP
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern nuint wse_capi_udp_maximum_datagram_size();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_udp_client_create(out IntPtr client);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_udp_client_destroy(IntPtr client);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_udp_client_bind(
        SafeHandle client, byte[] host, ushort port, ref NativeOperationContext context);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_udp_client_close(SafeHandle client);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_udp_client_is_open(SafeHandle client, out int open);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_udp_client_local_endpoint(
        SafeHandle client, byte[]? host, out nuint size, out ushort port, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_udp_client_send_to(
        SafeHandle client, out nuint sent, byte[] host, ushort port, byte[] data, nuint size,
        ref NativeOperationContext context);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_udp_client_receive_from_with_progress(
        SafeHandle client, out IntPtr datagram, nuint maximumSize, ref NativeOperationContext context);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_udp_datagram_destroy(IntPtr datagram);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_udp_datagram_source(
        SafeHandle datagram, byte[]? host, out nuint size, out ushort port, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_udp_datagram_payload(
        SafeHandle datagram, byte[]? buffer, out nuint size, nuint capacity);

    // ---------------------------------------------------------------------------------------
    // XPT: serial
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_serial_port_create(out IntPtr port);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_serial_port_destroy(IntPtr port);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_serial_port_open(
        SafeHandle port, byte[] deviceName, int baudRate, ref NativeOperationContext context);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_serial_port_close(SafeHandle port);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_serial_port_is_open(SafeHandle port, out int open);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_serial_port_device_name(
        SafeHandle port, byte[]? buffer, out nuint size, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_serial_port_baud_rate(
        SafeHandle port, out int baudRate);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_serial_port_send(
        SafeHandle port, out nuint sent, byte[] data, nuint size, ref NativeOperationContext context);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_serial_port_receive(
        SafeHandle port, out IntPtr data, nuint maximumSize, ref NativeOperationContext context);

    // ---------------------------------------------------------------------------------------
    // XPT: HTTP
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_http_request_create(
        out IntPtr request, int method, byte[] url);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_http_request_destroy(IntPtr request);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_http_request_add_header(
        SafeHandle request, byte[] name, byte[] value);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_http_request_set_body(
        SafeHandle request, byte[] body, nuint size);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_http_execute(
        SafeHandle request, out IntPtr response, nuint maximumResponseBodySize,
        ref NativeOperationContext context);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_http_execute_authenticated(
        SafeHandle request, out IntPtr response, nuint maximumResponseBodySize,
        byte[] username, byte[] secret, ref NativeOperationContext context);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_http_response_destroy(IntPtr response);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_http_response_status_code(
        SafeHandle response, out ushort statusCode);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_http_response_attempt_count(
        SafeHandle response, out uint attemptCount);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_http_response_header_count(
        SafeHandle response, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_http_response_header_name(
        SafeHandle response, byte[]? buffer, out nuint size, nuint index, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_http_response_header_value(
        SafeHandle response, byte[]? buffer, out nuint size, nuint index, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_http_response_body(
        SafeHandle response, byte[]? buffer, out nuint size, nuint capacity);

    // ---------------------------------------------------------------------------------------
    // Tmr: enumeration and device description
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_enumerate(out IntPtr list, int backend);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_camera_device_list_destroy(IntPtr list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_device_list_count(
        IntPtr list, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_device_list_at(
        IntPtr list, out IntPtr device, nuint index);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_camera_device_destroy(IntPtr device);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_device_backend(
        SafeHandle device, out int backend);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_device_transport_type(
        SafeHandle device, out int transport);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_device_id(
        SafeHandle device, byte[]? buffer, out nuint size, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_device_display_name(
        SafeHandle device, byte[]? buffer, out nuint size, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_device_transport(
        SafeHandle device, byte[]? buffer, out nuint size, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_device_usb(
        SafeHandle device, out NativeCameraUsbIdentity usb);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_device_usb_serial_number(
        SafeHandle device, byte[]? buffer, out nuint size, nuint capacity);

    // ---------------------------------------------------------------------------------------
    // Tmr: capability
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capabilities(
        SafeHandle device, out IntPtr capability);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_camera_capability_destroy(IntPtr capability);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_format_count(
        SafeHandle capability, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_format_at(
        SafeHandle capability, out NativeCameraFormat format, nuint index);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_profile_count(
        SafeHandle capability, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_profile_native_format(
        SafeHandle capability, out NativeCameraFormat format, nuint index);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_profile_output_count(
        SafeHandle capability, out nuint count, nuint index);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_profile_output_at(
        SafeHandle capability, out int pixelFormat, nuint profileIndex, nuint outputIndex);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_control_count(
        SafeHandle capability, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_control_at(
        SafeHandle capability, out NativeCameraControlCapability control, nuint index);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_control_display_name(
        SafeHandle capability, byte[]? buffer, out nuint size, nuint index, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_extension_unit_count(
        SafeHandle capability, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_extension_unit_at(
        SafeHandle capability, out IntPtr selector, nuint index);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_capability_device(
        SafeHandle capability, out IntPtr device);

    // ---------------------------------------------------------------------------------------
    // Tmr: extension unit
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_camera_xu_selector_destroy(IntPtr selector);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_xu_selector_describe(
        SafeHandle selector, out byte unitId, out byte selectorId,
        out nuint minimumSize, out nuint maximumSize, out int readable, out int writable);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_xu_selector_guid(
        SafeHandle selector, byte[] buffer, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_xu_selector_display_name(
        SafeHandle selector, byte[]? buffer, out nuint size, nuint capacity);

    // ---------------------------------------------------------------------------------------
    // Tmr: frame
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_camera_frame_destroy(IntPtr frame);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_frame_describe(
        SafeHandle frame, out NativeCameraFrameDescription description,
        out ulong sequence, out long monotonicTimestampNs);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_frame_data(
        SafeHandle frame, byte[]? buffer, out nuint size, nuint capacity);

    // ---------------------------------------------------------------------------------------
    // Tmr: frame operations
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static extern bool wse_capi_camera_pixel_format_supports_orientation(int pixelFormat);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static extern bool wse_capi_camera_pixel_format_supports_averaging(int pixelFormat);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static extern bool wse_capi_camera_pixel_format_is_bayer(int pixelFormat);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_pixel_format_bayer_pattern(
        out int pattern, int pixelFormat);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_frame_apply_orientation(
        SafeHandle frame, out IntPtr result, int orientation);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_frame_demosaic(
        SafeHandle frame, out IntPtr result, int outputPixelFormat, int method);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_frame_accumulator_create(
        out IntPtr accumulator);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_camera_frame_accumulator_destroy(IntPtr accumulator);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_frame_accumulator_add(
        SafeHandle accumulator, SafeHandle frame);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_frame_accumulator_count(
        SafeHandle accumulator, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_frame_accumulator_average(
        SafeHandle accumulator, out IntPtr result);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_camera_frame_accumulator_reset(SafeHandle accumulator);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_read_averaged_frame(
        SafeHandle camera, out IntPtr frame, nuint count, uint timeoutMilliseconds);

    // ---------------------------------------------------------------------------------------
    // Tmr: camera owner
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_create(out IntPtr camera);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_camera_destroy(IntPtr camera);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_open(
        SafeHandle camera, SafeHandle device, IntPtr configuration);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_start(SafeHandle camera);

    /// <summary>Native frame callback. The frame handle is owned by the callback.</summary>
    /// <param name="frame">Frame handle, or zero when the capture failed.</param>
    /// <param name="status">Status of the capture the callback is reporting.</param>
    /// <param name="userData">Token handed back unchanged.</param>
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void CameraFrameCallback(IntPtr frame, NativeStatus status, IntPtr userData);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_start_with_callback(
        SafeHandle camera, CameraFrameCallback callback, IntPtr userData);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_stop(SafeHandle camera);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_read_frame(
        SafeHandle camera, out IntPtr frame, uint timeoutMilliseconds);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_current_capabilities(
        SafeHandle camera, out IntPtr capability);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_query_control_capability(
        SafeHandle camera, out NativeCameraControlCapability capability, int control);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_get_control(
        SafeHandle camera, out NativeCameraControlValue value, int control);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_set_control(
        SafeHandle camera, ref NativeCameraControlValue value);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_get_extension_unit(
        SafeHandle camera, byte[]? buffer, out nuint size, SafeHandle selector, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_set_extension_unit(
        SafeHandle camera, SafeHandle selector, byte[] payload, nuint size);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_is_open(SafeHandle camera, out int open);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_is_streaming(
        SafeHandle camera, out int streaming);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_camera_close(SafeHandle camera);

    // ---------------------------------------------------------------------------------------
    // VPJ: model profile
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint wse_capi_model_profile_supported_schema_version();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_from_json(
        out IntPtr profile, byte[] jsonText);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_from_file(
        out IntPtr profile, byte[] filePath);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_model_profile_destroy(IntPtr profile);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_schema_version(
        IntPtr profile, out uint version);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_model_id(
        IntPtr profile, byte[]? buffer, out nuint size, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_display_name(
        IntPtr profile, byte[]? buffer, out nuint size, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_alias_count(
        IntPtr profile, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_alias_at(
        IntPtr profile, byte[]? buffer, out nuint size, nuint index, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_capability_count(
        IntPtr profile, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_capability_at(
        IntPtr profile, byte[]? buffer, out nuint size, nuint index, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_protocol_count(
        IntPtr profile, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_protocol_at(
        IntPtr profile, out int kind, out int transport, out int authentication, nuint index);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_protocol_id(
        IntPtr profile, byte[]? buffer, out nuint size, nuint index, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_protocol_credential_reference(
        IntPtr profile, byte[]? buffer, out nuint size, nuint index, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_command_count(
        IntPtr profile, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_command_read_only(
        IntPtr profile, out int readOnly, nuint index);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_command_id(
        IntPtr profile, byte[]? buffer, out nuint size, nuint index, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_command_protocol_id(
        IntPtr profile, byte[]? buffer, out nuint size, nuint index, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_constraints(
        IntPtr profile, out NativeModelConstraints constraints);

    // ---------------------------------------------------------------------------------------
    // VPJ: registry
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_registry_create(
        out IntPtr registry);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_model_profile_registry_destroy(IntPtr registry);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_registry_register(
        IntPtr registry, IntPtr profile, int replaceExisting);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_registry_load_file(
        IntPtr registry, byte[] filePath, int replaceExisting);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_registry_load_directory(
        IntPtr registry, out uint discovered, out uint registered,
        byte[] directoryPath, int replaceExisting);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_registry_find(
        IntPtr registry, out IntPtr profile, byte[] modelOrAliasId);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_registry_resolve_identity(
        IntPtr registry, out int state, byte[]? buffer, out nuint size,
        byte[] modelOrAliasId, nuint capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_registry_id_count(
        IntPtr registry, out nuint count);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_model_profile_registry_id_at(
        IntPtr registry, byte[]? buffer, out nuint size, nuint index, nuint capacity);

    // ---------------------------------------------------------------------------------------
    // VPJ: profile control session
    // ---------------------------------------------------------------------------------------

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_profile_control_session_create(
        out IntPtr session);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void wse_capi_profile_control_session_destroy(IntPtr session);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_profile_control_session_configure(
        IntPtr session, IntPtr profile, byte[] protocolId,
        ref NativeProfileControlEndpoint endpoint);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_profile_control_session_is_configured(
        IntPtr session, out int configured);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_profile_control_session_connect(
        IntPtr session, ref NativeOperationContext context);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_profile_control_session_disconnect(
        IntPtr session);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_profile_control_session_is_connected(
        IntPtr session, out int connected);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern NativeStatus wse_capi_profile_control_session_execute(
        IntPtr session, out ushort statusCode, out IntPtr body,
        byte[] commandId, byte[] requestBody, nuint requestSize,
        ref NativeOperationContext context);
}

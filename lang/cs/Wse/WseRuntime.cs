//*****************************************************************************************************************
//!
//! @file    WseRuntime.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese C# Bindingの公開入口. 同期Native処理をManagedへ提供する.
//! @brief   \~english  Public entry point of the C# binding, providing synchronous native operations.
//!
//! @details
//!     \~japanese
//!     @n JavaScript、Python、Java Bindingと同じ公開面と意味論を持つ。所有権は`Dispose`で解放する
//!        単一のRAII所有者に集約し、OS Native handleおよびBackend objectは公開しない。
//!     \~english
//!     @n Offers the same public surface and semantics as the JavaScript, Python, and Java bindings.
//!        Ownership stays in one RAII owner released by <c>Dispose</c>; neither OS native handles
//!        nor backend objects are exposed.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Runtime.InteropServices;
using System.Text;

namespace WapitiStew.Wse;

/// <summary>Owns the native runtime handle and releases it exactly once.</summary>
internal sealed class RuntimeHandle : SafeHandle
{
    internal RuntimeHandle()
        : base(IntPtr.Zero, ownsHandle: true)
    {
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Attach(IntPtr value) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        NativeMethods.wse_capi_runtime_destroy(handle);
        return true;
    }
}

/// <summary>
/// Entry point of the WonderStewEngine C# binding. Create one instance, use it from managed code,
/// and dispose it when finished.
/// </summary>
public sealed class WseRuntime : IDisposable
{
    private readonly RuntimeHandle _handle;

    /// <summary>Creates a runtime.</summary>
    /// <exception cref="WseException">The native runtime could not be created.</exception>
    public WseRuntime()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_runtime_destroy);
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_runtime_create(out output.Value));
        _handle = new RuntimeHandle();
        _handle.Attach(output.Take());
    }

    /// <summary>
    /// Names the native library file explicitly. Call this before the first native call when the
    /// library does not sit beside the managed assembly; it is the C# counterpart of the runtime
    /// path arguments accepted by the JavaScript, Python, and Java bindings.
    /// </summary>
    /// <param name="path">Full path to the native library file.</param>
    public static void SetNativeLibraryPath(string path) => NativeMethods.SetLibraryPath(path);

    /// <summary>Gets the version of the flat C ABI implemented by the loaded native library.</summary>
    public static uint NativeAbiVersion => NativeMethods.wse_capi_abi_version();

    /// <summary>Reads the runtime version, binding ABI version, and available components.</summary>
    /// <returns>Portable runtime information.</returns>
    /// <exception cref="ObjectDisposedException">The runtime was already disposed.</exception>
    /// <exception cref="WseException">The native call failed.</exception>
    public RuntimeInfo Info()
    {
        ThrowIfDisposed();
        SafeHandle raw = _handle;

        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_runtime_version(raw, null, out nuint required, 0));

        string version = string.Empty;
        if (required > 1)
        {
            byte[] buffer = new byte[checked((int)required)];
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_runtime_version(raw, buffer, out _, required));
            version = Encoding.UTF8.GetString(buffer, 0, buffer.Length - 1);
        }

        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_runtime_binding_abi_version(raw, out uint abiVersion));
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_runtime_components(raw, out NativeComponents components));

        return new RuntimeInfo(
            version,
            abiVersion,
            new ComponentAvailability(
                components.HasXpt != 0,
                components.HasTmr != 0,
                components.HasOui != 0,
                components.HasGef != 0,
                components.HasIui != 0,
                components.HasVpj != 0));
    }

    /// <summary>
    /// Copies caller bytes into a runtime-owned buffer. The runtime never retains the caller array.
    /// </summary>
    /// <param name="bytes">Bytes to copy.</param>
    /// <returns>A buffer owning the copied bytes. Dispose it when finished.</returns>
    /// <exception cref="ArgumentNullException"><paramref name="bytes"/> is null.</exception>
    /// <exception cref="ObjectDisposedException">The runtime was already disposed.</exception>
    /// <exception cref="WseException">The native call failed.</exception>
    public FrameBuffer CopyFrame(byte[] bytes)
    {
        ArgumentNullException.ThrowIfNull(bytes);
        ThrowIfDisposed();

        using var output = new NativeOutput(NativeMethods.wse_capi_frame_buffer_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_runtime_copy_frame(
                _handle, out output.Value, bytes, (nuint)bytes.Length));
        return new FrameBuffer(output);
    }

    /// <summary>Waits for a finite duration without observing cancellation.</summary>
    /// <param name="duration">Non-negative wait duration.</param>
    public void Wait(TimeSpan duration) => Wait(duration, null);

    /// <summary>
    /// Waits for a finite duration while cooperatively observing a cancellation request.
    /// </summary>
    /// <param name="duration">Non-negative wait duration.</param>
    /// <param name="cancellation">Cancellation source to observe, or null to wait uninterrupted.</param>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="duration"/> is negative.</exception>
    /// <exception cref="ObjectDisposedException">The runtime was already disposed.</exception>
    /// <exception cref="WseException">
    /// The native call failed, including <see cref="ErrorCategory.Cancellation"/> when the wait
    /// observed a cancellation request.
    /// </exception>
    public void Wait(TimeSpan duration, CancellationSource? cancellation)
    {
        if (duration < TimeSpan.Zero)
        {
            throw new ArgumentOutOfRangeException(
                nameof(duration), "The wait duration must not be negative.");
        }
        ThrowIfDisposed();

        using var cancellationLease = cancellation?.Acquire();
        IntPtr token = cancellationLease?.Pointer ?? IntPtr.Zero;
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_runtime_wait(
                _handle, (long)duration.TotalMilliseconds, token));
    }

    /// <summary>Releases the native runtime.</summary>
    public void Dispose() => _handle.Dispose();

    private void ThrowIfDisposed()
    {
        if (_handle.IsClosed || _handle.IsInvalid)
        {
            throw new ObjectDisposedException(nameof(WseRuntime));
        }
    }
}

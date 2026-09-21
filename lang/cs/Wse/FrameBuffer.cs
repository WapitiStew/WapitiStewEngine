//*****************************************************************************************************************
//!
//! @file    FrameBuffer.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Native Handleを漏らさず共有できる不変Byte列.
//! @brief   \~english  Immutable bytes that can be shared without exposing native handles.
//!
//! @details
//!     \~japanese
//!     @n 所有権は単一のRAII所有者(`SafeHandle`)にある。`Dispose`は何度呼んでも安全である。
//!     \~english
//!     @n Ownership lives in one RAII owner (a <c>SafeHandle</c>); <c>Dispose</c> is idempotent.
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

/// <summary>Owns the native frame-buffer handle and releases it exactly once.</summary>
internal sealed class FrameBufferHandle : SafeHandle
{
    internal FrameBufferHandle()
        : base(IntPtr.Zero, ownsHandle: true)
    {
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Attach(IntPtr value) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        NativeMethods.wse_capi_frame_buffer_destroy(handle);
        return true;
    }
}

/// <summary>
/// An immutable block of bytes owned by the native runtime. Dispose the instance to release the
/// native allocation; the copy helpers never expose a native handle.
/// </summary>
public sealed class FrameBuffer : IDisposable
{
    private readonly FrameBufferHandle _handle;

    internal FrameBuffer(NativeOutput output)
    {
        _handle = new FrameBufferHandle();
        _handle.Attach(output.Take());
    }

    /// <summary>Gets the number of bytes held by this buffer.</summary>
    /// <exception cref="ObjectDisposedException">The buffer was already disposed.</exception>
    public int Size
    {
        get
        {
            ThrowIfDisposed();
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_frame_buffer_size(_handle, out nuint size));
            return checked((int)size);
        }
    }

    /// <summary>Gets a value indicating whether this buffer holds no bytes.</summary>
    public bool IsEmpty => Size == 0;

    /// <summary>Copies the buffer contents into a newly allocated managed array.</summary>
    /// <returns>A managed copy of the buffer contents.</returns>
    /// <exception cref="ObjectDisposedException">The buffer was already disposed.</exception>
    public byte[] ToArray()
    {
        ThrowIfDisposed();

        SafeHandle raw = _handle;
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_frame_buffer_copy_to(raw, null, out nuint required, 0));
        if (required == 0)
        {
            return Array.Empty<byte>();
        }

        byte[] destination = new byte[checked((int)required)];
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_frame_buffer_copy_to(raw, destination, out _, required));
        return destination;
    }

    /// <summary>Releases the native buffer.</summary>
    public void Dispose() => _handle.Dispose();

    private void ThrowIfDisposed()
    {
        if (_handle.IsClosed || _handle.IsInvalid)
        {
            throw new ObjectDisposedException(nameof(FrameBuffer));
        }
    }
}

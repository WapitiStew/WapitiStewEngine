//*****************************************************************************************************************
//!
//! @file    CancellationSource.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Binding共通のCooperative Cancellation所有者.
//! @brief   \~english  Owner of the cooperative cancellation shared by every binding.
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

/// <summary>Owns the native cancellation handle and releases it exactly once.</summary>
internal sealed class CancellationHandle : SafeHandle
{
    internal CancellationHandle()
        : base(IntPtr.Zero, ownsHandle: true)
    {
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Attach(IntPtr value) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        NativeMethods.wse_capi_cancellation_destroy(handle);
        return true;
    }
}

/// <summary>
/// Requests cooperative cancellation of a native wait. Cancellation is observed, never forced:
/// the native operation returns with <see cref="ErrorCategory.Cancellation"/> at its next
/// observation point.
/// </summary>
public sealed class CancellationSource : IDisposable
{
    private readonly CancellationHandle _handle;

    /// <summary>Creates a cancellation source that has not yet been cancelled.</summary>
    public CancellationSource()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_cancellation_destroy);
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_cancellation_create(out output.Value));
        _handle = new CancellationHandle();
        _handle.Attach(output.Take());
    }

    /// <summary>Gets a value indicating whether cancellation was requested.</summary>
    /// <exception cref="ObjectDisposedException">The source was already disposed.</exception>
    public bool IsCancellationRequested
    {
        get
        {
            ThrowIfDisposed();
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_cancellation_is_requested(
                    _handle, out int requested));
            return requested != 0;
        }
    }

    /// <summary>Requests cancellation. Calling this more than once is safe.</summary>
    /// <exception cref="ObjectDisposedException">The source was already disposed.</exception>
    public void Cancel()
    {
        ThrowIfDisposed();
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_cancellation_cancel(_handle));
    }

    /// <summary>Releases the native cancellation source.</summary>
    public void Dispose() => _handle.Dispose();

    internal NativeHandleLease Acquire() => new(_handle);

    private void ThrowIfDisposed()
    {
        if (_handle.IsClosed || _handle.IsInvalid)
        {
            throw new ObjectDisposedException(nameof(CancellationSource));
        }
    }
}

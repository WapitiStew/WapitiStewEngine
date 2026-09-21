using System;
using System.Threading;

namespace WapitiStew.Wse;

/// <summary>Contains managed exceptions at the reverse-P/Invoke boundary.</summary>
internal sealed class CameraCallbackRegistration
{
    private readonly CameraHandle _owner;
    private readonly Action<CameraFrame?, WseException?> _callback;
    private Exception? _failure;
    internal Exception? Failure => Volatile.Read(ref _failure);
    internal NativeMethods.CameraFrameCallback Bridge { get; }

    internal CameraCallbackRegistration(CameraHandle owner, Action<CameraFrame?, WseException?> callback)
    {
        _owner = owner;
        _callback = callback;
        Bridge = Invoke;
    }

    private void Invoke(IntPtr frame, NativeStatus status, IntPtr userData)
    {
        // A frame already belongs to us before any managed allocation can succeed.
        // Keep a non-allocating fallback until the temporary owner exists.
        IntPtr unclaimed = frame;
        try
        {
            if (Failure is not null) return;
            if (status.Category == (int)ErrorCategory.None)
            {
                using var pending = new NativeOutput(NativeMethods.wse_capi_camera_frame_destroy);
                pending.Value = unclaimed;
                unclaimed = IntPtr.Zero;
                var received = new CameraFrame(pending);
                // Ownership passes when the callback is entered. It may retain the
                // frame before throwing; do not revoke its ownership in the catch.
                _callback(received, null);
            }
            else
            {
                _callback(null, new WseException((ErrorCategory)status.Category,
                    status.Code, status.NativeCode, NativeMethods.LastErrorMessage()));
            }
        }
        catch (Exception error)
        {
            Interlocked.CompareExchange(ref _failure, error, null);
            // Callback-side stop defers join. A failed stop does not erase the
            // original exception or permit further delivery by this registration.
            try { _ = NativeMethods.wse_capi_camera_stop(_owner); }
            catch (Exception) { }
        }
        finally
        {
            if (unclaimed != IntPtr.Zero) NativeMethods.wse_capi_camera_frame_destroy(unclaimed);
        }
    }
}

using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace WapitiStew.Wse;

/// <summary>Retains an owner while its address is embedded in a native argument.</summary>
internal sealed class NativeHandleLease : IDisposable
{
    private SafeHandle? _owner;

    internal NativeHandleLease(SafeHandle owner)
    {
        bool acquired = false;
        try
        {
            owner.DangerousAddRef(ref acquired);
            Pointer = owner.DangerousGetHandle();
            _owner = owner;
        }
        catch
        {
            if (acquired)
            {
                owner.DangerousRelease();
            }
            throw;
        }
    }

    internal IntPtr Pointer { get; }

    public void Dispose() => Interlocked.Exchange(ref _owner, null)?.DangerousRelease();
}

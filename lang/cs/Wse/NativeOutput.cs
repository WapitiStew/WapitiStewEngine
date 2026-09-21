using System;
using System.Threading;

namespace WapitiStew.Wse;

/// <summary>Owns an output slot until its value is attached to a prepared SafeHandle.</summary>
internal sealed class NativeOutput : IDisposable
{
    private readonly Action<IntPtr> _destroy;
    internal IntPtr Value;

    // Thread-local characterization seam; never set by the public facade.
    [ThreadStatic] internal static Action<string>? Fault = null;
    internal static void Checkpoint(string stage) => Fault?.Invoke(stage);

    internal NativeOutput(Action<IntPtr> destroy)
    {
        _destroy = destroy;
        Checkpoint("prepare");
    }

    // The receiver allocates its SafeHandle first, then immediately calls Attach(Take()).
    // No allocating work is permitted between taking the pointer and attaching it.
    internal IntPtr Take()
    {
        Checkpoint("adopt");
        return Interlocked.Exchange(ref Value, IntPtr.Zero);
    }

    public void Dispose()
    {
        IntPtr value = Interlocked.Exchange(ref Value, IntPtr.Zero);
        if (value != IntPtr.Zero) _destroy(value);
    }

    internal static T[] Collect<T>(int count, Func<int, T> create) where T : class, IDisposable
    {
        var values = new T[count];
        try
        {
            for (int index = 0; index < count; ++index)
            {
                values[index] = create(index);
                Checkpoint("collection-item");
            }
            return values;
        }
        catch
        {
            foreach (T? value in values) value?.Dispose();
            throw;
        }
    }
}

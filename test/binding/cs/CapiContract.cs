using System;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using WapitiStew.Wse;

internal static class CapiContract
{
    internal static void Verify(string layoutProgram)
    {
        VerifyLayouts(layoutProgram);
        VerifyLease();
        VerifyDisposeRaces();
    }

    private static void Check(bool value, string message)
    {
        if (!value) { throw new InvalidOperationException(message); }
    }

    private static void VerifyLayouts(string program)
    {
        using var process = Process.Start(new ProcessStartInfo(program)
        {
            RedirectStandardOutput = true,
            UseShellExecute = false,
            CreateNoWindow = true,
        })!;
        string output = process.StandardOutput.ReadToEnd();
        Check(process.WaitForExit(10000) && process.ExitCode == 0, "C layout fixture failed");
        Assembly assembly = typeof(WseRuntime).Assembly;
        Type? current = null;
        int types = 0, fields = 0, currentFields = 0;
        foreach (string line in output.Split('\n', StringSplitOptions.RemoveEmptyEntries))
        {
            string[] cells = line.Trim().Split(' ');
            if (cells[0] == "TYPE")
            {
                if (current is not null)
                {
                    Check(current.GetFields(BindingFlags.Public | BindingFlags.Instance).Length == currentFields,
                        current.Name + " field inventory");
                }
                currentFields = 0;
                current = assembly.GetType("WapitiStew.Wse." + cells[1], throwOnError: true)!;
                Check(Marshal.SizeOf(current) == int.Parse(cells[2]), current.Name + " size");
                Type probe = typeof(AlignmentProbe<>).MakeGenericType(current);
                Check(Marshal.OffsetOf(probe, "Value").ToInt32() == int.Parse(cells[3]),
                    current.Name + " alignment");
                ++types;
            }
            else
            {
                Check(cells[0] == "FIELD" && current is not null, "invalid native fixture row");
                FieldInfo field = current!.GetField(cells[1])!;
                Check(Marshal.OffsetOf(current, cells[1]).ToInt32() == int.Parse(cells[2]),
                    current.Name + "." + cells[1] + " offset");
                int size = field.FieldType.IsArray
                    ? field.GetCustomAttribute<MarshalAsAttribute>()!.SizeConst *
                        Marshal.SizeOf(field.FieldType.GetElementType()!)
                    : Marshal.SizeOf(field.FieldType);
                Check(size == int.Parse(cells[3]), current.Name + "." + cells[1] + " size");
                ++fields;
                ++currentFields;
            }
        }
        Check(current is not null && current.GetFields(BindingFlags.Public | BindingFlags.Instance).Length == currentFields,
            "final field inventory");
        Check(types == 16 && fields == 87, "complete public C ABI layout inventory");
        Console.WriteLine($"C/C# layouts: {types} structs, {fields} fields");
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct AlignmentProbe<T>
    {
        public byte Prefix;
        public T Value;
    }

    private sealed class CountingHandle : SafeHandle
    {
        internal int Releases;
        internal CountingHandle() : base(IntPtr.Zero, true) { SetHandle(new IntPtr(1)); }
        public override bool IsInvalid => handle == IntPtr.Zero;
        protected override bool ReleaseHandle() { ++Releases; return true; }
    }

    private static IDisposable Acquire(SafeHandle handle)
    {
        Type lease = typeof(WseRuntime).Assembly.GetType("WapitiStew.Wse.NativeHandleLease", true)!;
        return (IDisposable)Activator.CreateInstance(lease, BindingFlags.Instance | BindingFlags.NonPublic,
            binder: null, args: new object[] { handle }, culture: null)!;
    }

    private static void VerifyLease()
    {
        var owner = new CountingHandle();
        IDisposable first = Acquire(owner);
        IDisposable second = Acquire(owner);
        Task.Run(owner.Dispose).GetAwaiter().GetResult();
        Check(owner.Releases == 0, "Dispose freed an owner while leases were active");
        first.Dispose();
        first.Dispose();
        Check(owner.Releases == 0, "one lease released another lease's reference");
        second.Dispose();
        Check(owner.Releases == 1 && owner.IsClosed, "last lease must release exactly once");
        try { using var invalid = Acquire(owner); throw new Exception("closed handle was accepted"); }
        catch (TargetInvocationException error) when (error.InnerException is ObjectDisposedException) { }
        Check(owner.Releases == 1, "failed acquisition released an unowned reference");
    }

    private static void VerifyDisposeRaces()
    {
        for (int iteration = 0; iteration != 64; ++iteration)
        {
            var runtime = new WseRuntime();
            var cancellation = new CancellationSource();
            FrameBuffer buffer = runtime.CopyFrame(new byte[] { 1, 2, 3, 4 });
            using var begin = new ManualResetEventSlim();
            Task operation = Task.Run(() =>
            {
                begin.Wait();
                try
                {
                    runtime.Wait(TimeSpan.FromMilliseconds(2), cancellation);
                    byte[] copy = buffer.ToArray();
                    Check(copy.Length == 4 && copy[3] == 4, "concurrent frame copy corruption");
                }
                catch (ObjectDisposedException) { }
            });
            begin.Set();
            if ((iteration & 1) != 0) { Thread.Yield(); }
            cancellation.Dispose();
            runtime.Dispose();
            buffer.Dispose();
            operation.GetAwaiter().GetResult();
            runtime.Dispose();
            cancellation.Dispose();
            buffer.Dispose();
        }
        Console.WriteLine("Lease release ordering and 64 managed Dispose races passed");
    }
}

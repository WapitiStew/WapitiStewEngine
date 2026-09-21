using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using WapitiStew.Wse;

internal static class OwnershipContract
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int Query();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate void Configure(int mode);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate void Join();
    private static Query Live = null!, Created = null!, Stops = null!;
    private static Configure Mode = null!;
    private static Join JoinWorker = null!;
    private static readonly FieldInfo Fault = typeof(WseRuntime).Assembly
        .GetType("WapitiStew.Wse.NativeOutput", true)!.GetField("Fault", BindingFlags.Static | BindingFlags.NonPublic)!;
    private static int faultCases, callbackCases;
    private static void Check(bool value, string message)
    { if(!value) throw new InvalidOperationException(message); }
    private static void SetFault(Action<string>? value) => Fault.SetValue(null, value);

    private static void Fail(Action operation, string stage = "adopt", int occurrence = 1)
    {
        int before=Live(), created=Created();
        var expected=new OutOfMemoryException("synthetic " + stage);
        int seen=0; bool caught=false;
        SetFault(value => { if(value==stage && ++seen==occurrence) throw expected; });
        try { operation(); }
        catch(OutOfMemoryException error) { Check(ReferenceEquals(error,expected),"primary exception was replaced"); caught=true; }
        finally { SetFault(null); }
        Check(caught,$"fault was not reached: {stage}/{occurrence}");
        Check(Live()==before,$"native leak after {stage}/{occurrence}");
        if(stage=="prepare" && occurrence==1) Check(Created()==created,"native acquisition preceded guard preparation");
        ++faultCases;
    }

    internal static int Verify(string library)
    {
        IntPtr module=NativeLibrary.Load(library);
        T Export<T>(string name) where T:Delegate => Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(module,name));
        Live=Export<Query>("wse_test_live"); Created=Export<Query>("wse_test_created");
        Stops=Export<Query>("wse_test_stops"); Mode=Export<Configure>("wse_test_mode"); JoinWorker=Export<Join>("wse_test_join");
        // Keep the module loaded: SafeHandle finalizers must never target an unloaded fixture.
        VerifyOutputs();
        VerifyCallbacks();
        VerifyPinRollback();
        GC.Collect(); GC.WaitForPendingFinalizers(); GC.Collect();
        Check(Live()==0,"final owned handle balance");
        Console.WriteLine($"Managed ownership: {faultCases} injected rollback cases, {callbackCases} worker callback cases; all handles balanced.");
        return 0;
    }

    private static void VerifyOutputs()
    {
        Mode(0);
        Action[] constructors={
            () => { using var x=new WseRuntime(); },
            () => { using var x=new CancellationSource(); },
            () => { using var x=new Keyboard(); },
            () => { using var x=new TcpClient(); },
            () => { using var x=new UdpClient(); },
            () => { using var x=new SerialPort(); },
            () => { using var x=new WebCamera(); },
            () => { using var x=new CameraFrameAccumulator(); },
            () => { using var x=new HttpRequest(HttpMethod.Get,"http://synthetic/"); }
        };
        foreach(Action create in constructors)
        { Fail(create,"prepare"); Fail(create); create(); Check(Live()==0,"constructor release"); }
        using var runtime=new WseRuntime();
        using var tcp=new TcpClient(); using var udp=new UdpClient(); using var serial=new SerialPort();
        using var camera=new WebCamera(); using var accumulator=new CameraFrameAccumulator();
        using var request=new HttpRequest(HttpMethod.Get,"http://synthetic/");
        var context=new OperationContext(TimeSpan.FromMilliseconds(50));
        var devices=WebCamera.Enumerate();
        try
        {
            using var capability=devices[0].Capabilities();
            using var frame=camera.ReadFrame(TimeSpan.FromMilliseconds(50));
            Action[] outputs={
                () => { using var x=runtime.CopyFrame(new byte[]{1,2,3,4}); },
                () => { _=tcp.Receive(4,context); },
                () => { _=serial.Receive(4,context); },
                () => { using var x=udp.ReceiveFrom(4,context); },
                () => { using var x=HttpClient.Execute(request,4,context); },
                () => { using var x=HttpClient.ExecuteAuthenticated(request,4,"u","s",context); },
                () => { using var x=devices[0].Capabilities(); },
                () => { using var x=capability.Device(); },
                () => { using var x=camera.ReadFrame(TimeSpan.FromMilliseconds(50)); },
                () => { using var x=camera.ReadAveragedFrame(2,TimeSpan.FromMilliseconds(50)); },
                () => { using var x=camera.CurrentCapabilities(); },
                () => { using var x=frame.ApplyOrientation(ImageOrientation.None); },
                () => { using var x=frame.Demosaic(CameraPixelFormat.Rgb8); },
                () => { using var x=accumulator.Average(); },
                () => { using var x=Projection.Render(ProjectionRequest()); }
            };
            foreach(Action produce in outputs) { Fail(produce); int liveBefore=Live(); produce(); Check(Live()==liveBefore,"success transfer balance"); }
            for(int index=1;index<=3;++index)
            {
                Fail(() => DisposeAll(WebCamera.Enumerate()),"adopt",index);
                Fail(() => DisposeAll(WebCamera.Enumerate()),"collection-item",index);
                Fail(() => DisposeAll(capability.ExtensionUnits()),"adopt",index);
                Fail(() => DisposeAll(capability.ExtensionUnits()),"collection-item",index);
                Fail(() => DisposeAll(capability.ExtensionUnits()),"selector-metadata",index);
            }
            Fail(() => { using var x=camera.ReadFrame(TimeSpan.FromMilliseconds(50)); },"frame-metadata");
            Fail(() => { using var x=Projection.Render(ProjectionRequest()); },"projection-result");
            Mode(1);
            Fail(() => { using var x=HttpClient.Execute(request,4,context); },"http-exception");
            Fail(() => { using var x=HttpClient.ExecuteAuthenticated(request,4,"u","s",context); },"http-exception");
            int before=Live();
            try { using var x=HttpClient.Execute(request,4,context); throw new InvalidOperationException("expected HTTP status failure"); }
            catch(HttpStatusException error)
            { Check(Live()==before+1 && error.Response.StatusCode==404,"HTTP error response ownership"); error.Response.Dispose(); }
            Check(Live()==before,"HTTP error response release");
            Mode(2);
            try { using var x=HttpClient.Execute(request,4,context); throw new InvalidOperationException("expected transport error"); }
            catch(WseException) { Check(Live()==before,"untouched output on transport failure"); }
            Mode(3);
            try { using var x=camera.ReadFrame(TimeSpan.FromMilliseconds(50)); throw new InvalidOperationException("expected metadata failure"); }
            catch(WseException) { Check(Live()==before,"post-adoption metadata failure release"); }
            Mode(7);
            try { DisposeAll(capability.ExtensionUnits()); throw new InvalidOperationException("expected metadata overflow"); }
            catch(OverflowException) { Check(Live()==before,"selector conversion failure release"); }
            Mode(0);
        }
        finally { DisposeAll(devices); }
    }

    private static void DisposeAll<T>(IReadOnlyList<T> values) where T:IDisposable
    { foreach(T value in values) value.Dispose(); }

    private static ProjectionRequest ProjectionRequest() => new(1,1,new[]{new ProjectionLayer(1,1,new byte[]{1,2,3,4},
        new[]{new ProjectionVertex(-1,1,0,0),new ProjectionVertex(1,1,1,0),new ProjectionVertex(-1,-1,0,1)},new[]{0,1,2})});

    private static void VerifyCallbacks()
    {
        foreach(string stage in new[]{"normal","application","prepare","adopt","frame-metadata","native-metadata","native-error","failed-stop"})
        {
            Mode(stage=="native-metadata" ? 3 : stage=="native-error" ? 5 : stage=="failed-stop" ? 6 : 0);
            using var camera=new WebCamera(); int before=Live(), calls=0;
            CameraFrame? retained=null;
            var expected=new OutOfMemoryException("synthetic callback failure");
            camera.Start((frame,error) =>
            {
                ++calls;
                if(stage=="application" || stage=="failed-stop") { retained=frame; throw expected; }
                if(stage=="native-error") { Check(frame is null && error is not null,"error callback"); throw expected; }
                Check(frame is not null && error is null,"successful callback");
                frame!.Dispose();
                if(stage=="prepare" || stage=="adopt" || stage=="frame-metadata")
                    SetFault(value => { if(value==stage) throw expected; });
            });
            JoinWorker();
            if(stage=="normal") Check(calls==3 && camera.CallbackException is null && Stops()==0,"normal callbacks");
            else
            {
                Check(calls==(stage=="native-metadata" ? 0 : 1),"failed registration delivered again");
                Check(stage=="native-metadata" ? camera.CallbackException is WseException : ReferenceEquals(camera.CallbackException,expected),"callback exception identity");
                Check(Stops()==1,"callback did not request exactly one stop");
            }
            Check(Live()==before+(retained is null ? 0 : 1),"callback raw/adopted/late-frame balance");
            if(retained is not null) { Check(retained.ToArray()[0]==7,"callback retained frame revoked"); retained.Dispose(); }
            Exception? previous=camera.CallbackException;
            camera.Close(); Check(ReferenceEquals(camera.CallbackException,previous),"Close erased failure diagnostic");
            Mode(4);
            try { camera.Start((f,e)=>f?.Dispose()); throw new InvalidOperationException("expected start failure"); }
            catch(WseException) { Check(ReferenceEquals(camera.CallbackException,previous),"failed Start erased prior failure"); }
            Mode(0); camera.Start(); Check(camera.CallbackException is null,"successful Start did not reset failure");
            ++callbackCases;
        }
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static WeakReference FaultedPin()
    {
        var bytes=new byte[16]; var weak=new WeakReference(bytes);
        var method=typeof(Projection).GetMethod("Pin",BindingFlags.Static|BindingFlags.NonPublic)!;
        SetFault(stage=>{ if(stage=="pin-register") throw new OutOfMemoryException("synthetic pin register"); });
        try { method.Invoke(null,new object[]{bytes,new List<GCHandle>()}); throw new InvalidOperationException("pin fault not reached"); }
        catch(TargetInvocationException error) { Check(error.InnerException is OutOfMemoryException,"pin primary error"); }
        finally { SetFault(null); }
        return weak;
    }
    private static void VerifyPinRollback()
    {
        WeakReference weak=FaultedPin(); GC.Collect(); GC.WaitForPendingFinalizers(); GC.Collect();
        Check(!weak.IsAlive,"failed pin registration leaked a GC root"); ++faultCases;
    }
}

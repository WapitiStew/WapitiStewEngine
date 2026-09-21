using System;
using System.Reflection;
using System.Runtime.InteropServices;
using WapitiStew.Wse;

internal static class TransferContract
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int Query();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate void Configure(int mode);
    private static void Check(bool value,string message)
    { if(!value) throw new InvalidOperationException(message); }
    private static readonly FieldInfo Fault=typeof(WseRuntime).Assembly.GetType("WapitiStew.Wse.NativeOutput",true)!
        .GetField("Fault",BindingFlags.NonPublic|BindingFlags.Static)!;

    internal static int Verify(string library)
    {
        IntPtr module=NativeLibrary.Load(library);
        T Export<T>(string name) where T:Delegate => Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(module,name));
        var mode=Export<Configure>("wse_test_mode");
        var live=Export<Query>("wse_test_live");var calls=Export<Query>("wse_test_transfers");
        int cases=0;
        using(var tcp=new TcpClient())
        using(var serial=new SerialPort())
        using(var udp=new UdpClient())
        {
            var context=new OperationContext(TimeSpan.FromMilliseconds(20));
            byte[] data={0,127,255,4};
            Func<int>[] sends={()=>tcp.Send(data,context),()=>serial.Send(data,context),
                ()=>udp.SendTo(new Endpoint("127.0.0.1",4321),data,context)};
            foreach(var send in sends)
            {
                foreach(int scenario in new[]{8,9,12,13,15})
                {
                    mode(scenario); int baseline=live();
                    try { send(); throw new InvalidOperationException("send should fail"); }
                    catch(WseException failure)
                    {
                        Check(failure is TransferException,"base catch retains transfer subtype");
                        var progress=(TransferException)failure;
                        ulong expected=scenario==8 ? 3UL : scenario==9 ? 1UL : scenario==12 ? 2UL
                            : scenario==15 ? (ulong)nuint.MaxValue : 0UL;
                        Check(progress.BytesTransferred==expected,"send progress was narrowed/lost");
                        Check(progress.ReceivedData is null && progress.SourceEndpoint is null,"send fabricated a receive");
                        Check(progress.NativeCode==78 && progress.Message=="synthetic transfer","diagnostic identity");
                        Check(progress.Code==(int)(scenario==9 ? TransportErrorCode.Cancelled : TransportErrorCode.TimedOut),"stable code");
                    }
                    Check(calls()==1 && live()==baseline,"send retry/leak");++cases;
                }
                mode(14);Check(send()==4 && calls()==1,"successful send changed");++cases;
            }
            mode(10);int before=live();
            TransferException? retained=null;
            try { udp.ReceiveFrom(3,context);throw new InvalidOperationException("truncation should fail"); }
            catch(TransferException failure) { retained=failure; }
            Check(retained is not null && retained.BytesTransferred==3 && retained.ReceivedData![0]==0
                && retained.ReceivedData[1]==127 && retained.ReceivedData[2]==255,"detached binary prefix");
            Check(retained!.SourceEndpoint==new Endpoint("127.0.0.1",4321),"source endpoint");
            Check(retained.Code==(int)TransportErrorCode.DatagramTruncated && retained.NativeCode==77
                && retained.Message=="synthetic transfer","getters erased original diagnostic");
            Check(live()==before && calls()==1,"exception retains native datagram or retries");++cases;
            foreach(string stage in new[]{"prepare","adopt","transfer-payload","transfer-source","transfer-exception"})
            {
                mode(10);var expected=new OutOfMemoryException("synthetic "+stage);bool caught=false;
                Fault.SetValue(null,(Action<string>)(value=>{if(value==stage)throw expected;}));
                try { using var unexpected=udp.ReceiveFrom(3,context); }
                catch(OutOfMemoryException failure) { caught=ReferenceEquals(expected,failure); }
                finally { Fault.SetValue(null,null); }
                Check(caught && live()==before,"partial-result failure leaked/replaced exception: "+stage);
                Check(calls()==(stage=="prepare" ? 0 : 1),"failure caused repeated I/O");++cases;
            }
            mode(11);
            try { using var unexpected=udp.ReceiveFrom(3,context);throw new InvalidOperationException("idle receive should fail"); }
            catch(WseException failure)
            { Check(failure is not TransferException && failure.Code==(int)TransportErrorCode.TimedOut,"no fabricated datagram"); }
            Check(live()==before && calls()==1,"empty failure lifetime");++cases;
        }
        GC.Collect();GC.WaitForPendingFinalizers();GC.Collect();Check(live()==0,"final native balance");
        Console.WriteLine($"{cases} managed transfer cases passed, including 5 allocation-boundary rollbacks.");
        return 0;
    }

    internal static void VerifyLoopback()
    {
        var context=new OperationContext(TimeSpan.FromSeconds(2));
        using(var receiver=new UdpClient())
        using(var sender=new UdpClient())
        {
            receiver.Bind(new Endpoint("127.0.0.1",0),context);
            sender.Bind(new Endpoint("127.0.0.1",0),context);
            byte[] bytes={0,127,255,4,5};sender.SendTo(receiver.LocalEndpoint,bytes,context);
            try { using var unexpected=receiver.ReceiveFrom(3,context);throw new InvalidOperationException("UDP truncation expected"); }
            catch(TransferException failure)
            {
                Check(failure.Code==(int)TransportErrorCode.DatagramTruncated && failure.BytesTransferred==3,"real UDP progress");
                Check(failure.ReceivedData is {Length:3} && failure.ReceivedData[0]==0
                    && failure.ReceivedData[1]==127 && failure.ReceivedData[2]==255,"real UDP bytes");
                Check(failure.SourceEndpoint==sender.LocalEndpoint,"real UDP source");
            }
            try { using var unexpected=receiver.ReceiveFrom(3,new OperationContext(TimeSpan.FromMilliseconds(20)));
                throw new InvalidOperationException("discarded suffix reappeared"); }
            catch(WseException failure) { Check(failure.Code==(int)TransportErrorCode.TimedOut,"whole datagram consumed"); }
            sender.SendTo(receiver.LocalEndpoint,Array.Empty<byte>(),context);
            using var empty=receiver.ReceiveFrom(3,context);Check(empty.Payload().Length==0,"zero-byte datagram remains a success");
        }
        // Actual C ABI calls: successful bytes are not undone by a later cancellation.
        // Positive progress within one failed call is covered by the deterministic native fixture;
        // OS socket-buffer sizes cannot be assumed to force the same short write on every host.
        var listener=new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback,0);
        listener.Start();
        try
        {
            using var client=new TcpClient();
            client.Connect(new Endpoint("127.0.0.1",checked((ushort)((System.Net.IPEndPoint)listener.LocalEndpoint).Port)),context);
            using var peer=listener.AcceptTcpClient();
            byte[] payload={37,38,39};
            Check(client.Send(payload,context)==3,"real TCP completed count");
            using var cancellation=new CancellationSource();cancellation.Cancel();
            try { client.Send(payload,new OperationContext(TimeSpan.FromSeconds(1),cancellation));
                throw new InvalidOperationException("cancelled send unexpectedly succeeded"); }
            catch(TransferException failure)
            {
                Check(failure.Code==(int)TransportErrorCode.Cancelled,"TCP cancellation failure type");
                Check(failure.BytesTransferred==0,"pre-cancelled send fabricated progress");
                Check(client.IsConnected,"cancellation closed established connection");
                peer.ReceiveTimeout=2000;
                Check(peer.GetStream().ReadByte()==37,"no real I/O accompanied progress");
            }
        }
        finally { listener.Stop(); }
        Console.WriteLine("Managed actual-shim loopback: UDP prefix/source/consumption/empty datagram and TCP send/cancellation passed.");
    }
}

// Synthetic C ABI provider for managed ownership tests. No device, network, or GPU access.
// Public declarations validate the signatures; this is never installed or packaged.
#include <wse/capi/stew.h>
#include <wse/capi/wse_capi_xpt.h>
#include <wse/capi/wse_capi_iui.h>
#include <wse/capi/wse_capi_oui.h>
#include <wse/capi/wse_capi_tmr.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace
{
std::mutex mutex;
std::unordered_set<void*> live;
std::atomic<int> created{0}, destroyed{0}, stops{0};
int mode=0;
int transfers=0;
thread_local const char* transfer_diagnostic=nullptr;
std::thread worker;
thread_local bool callback_thread=false;
const wse_capi_status ok{};
const wse_capi_status failure{3,91,0};
template<class T> T create()
{
    auto* value=new unsigned char(1);
    std::lock_guard<std::mutex> lock(mutex);
    live.insert(value); ++created;
    return reinterpret_cast<T>(value);
}
void destroy(void* value_in)
{
    if(!value_in) return;
    std::lock_guard<std::mutex> lock(mutex);
    if(live.erase(value_in)!=1) std::abort();
    delete static_cast<unsigned char*>(value_in); ++destroyed;
}
void join()
{
    if(callback_thread) return;
    if(worker.joinable()) worker.join();
}
}

extern "C" WSE_CAPI int WSE_CAPI_CALL wse_test_live() { std::lock_guard<std::mutex> lock(mutex); return static_cast<int>(live.size()); }
extern "C" WSE_CAPI int WSE_CAPI_CALL wse_test_created() { return created; }
extern "C" WSE_CAPI int WSE_CAPI_CALL wse_test_stops() { return stops; }
extern "C" WSE_CAPI void WSE_CAPI_CALL wse_test_mode(int value_in) { join(); mode=value_in; stops=0; transfers=0; transfer_diagnostic=nullptr; }
extern "C" WSE_CAPI int WSE_CAPI_CALL wse_test_transfers() { return transfers; }
extern "C" WSE_CAPI void WSE_CAPI_CALL wse_test_join() { join(); }

const char* WSE_CAPI_CALL wse_capi_last_error_message() { return transfer_diagnostic ? transfer_diagnostic : "synthetic failure"; }
uint32_t WSE_CAPI_CALL wse_capi_abi_version() { return 1; }

#define CREATE(name,type) wse_capi_status WSE_CAPI_CALL wse_capi_##name(type* out) { *out=create<type>(); return ok; }
#define DESTROY(name,type) void WSE_CAPI_CALL wse_capi_##name(type value) { destroy(value); }
CREATE(runtime_create,wse_capi_runtime)
CREATE(cancellation_create,wse_capi_cancellation)
CREATE(keyboard_create,wse_capi_keyboard)
CREATE(tcp_client_create,wse_capi_tcp_client)
CREATE(udp_client_create,wse_capi_udp_client)
CREATE(serial_port_create,wse_capi_serial_port)
CREATE(camera_create,wse_capi_camera)
CREATE(camera_frame_accumulator_create,wse_capi_camera_frame_accumulator)
DESTROY(runtime_destroy,wse_capi_runtime)
DESTROY(cancellation_destroy,wse_capi_cancellation)
DESTROY(keyboard_destroy,wse_capi_keyboard)
DESTROY(tcp_client_destroy,wse_capi_tcp_client)
DESTROY(udp_client_destroy,wse_capi_udp_client)
DESTROY(udp_datagram_destroy,wse_capi_udp_datagram)
DESTROY(serial_port_destroy,wse_capi_serial_port)
DESTROY(http_request_destroy,wse_capi_http_request)
DESTROY(http_response_destroy,wse_capi_http_response)
DESTROY(frame_buffer_destroy,wse_capi_frame_buffer)
DESTROY(camera_device_list_destroy,wse_capi_camera_device_list)
DESTROY(camera_device_destroy,wse_capi_camera_device)
DESTROY(camera_capability_destroy,wse_capi_camera_capability)
DESTROY(camera_xu_selector_destroy,wse_capi_camera_xu_selector)
DESTROY(camera_frame_destroy,wse_capi_camera_frame)
DESTROY(camera_frame_accumulator_destroy,wse_capi_camera_frame_accumulator)
void WSE_CAPI_CALL wse_capi_camera_destroy(wse_capi_camera value_in) { join(); destroy(value_in); }

wse_capi_status WSE_CAPI_CALL wse_capi_runtime_copy_frame(wse_capi_runtime,wse_capi_frame_buffer* out,const uint8_t*,size_t)
{ *out=create<wse_capi_frame_buffer>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_frame_buffer_size(wse_capi_frame_buffer,size_t* out) { *out=4; return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_frame_buffer_copy_to(wse_capi_frame_buffer,uint8_t* out,size_t* written,size_t capacity)
{ *written=4; if(out && capacity>=4) std::memset(out,7,4); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_receive(wse_capi_tcp_client,wse_capi_frame_buffer* out,size_t,const wse_capi_operation_context*)
{ *out=create<wse_capi_frame_buffer>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_receive(wse_capi_serial_port,wse_capi_frame_buffer* out,size_t,const wse_capi_operation_context*)
{ *out=create<wse_capi_frame_buffer>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_receive_from(wse_capi_udp_client,wse_capi_udp_datagram* out,size_t,const wse_capi_operation_context*)
{ *out=create<wse_capi_udp_datagram>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_receive_from_with_progress(wse_capi_udp_client client,wse_capi_udp_datagram* out,size_t size,const wse_capi_operation_context* context)
{
    ++transfers; *out=nullptr; transfer_diagnostic="synthetic transfer";
    if(mode==11) return {WSE_CAPI_ERROR_TIMEOUT,WSE_CAPI_TRANSPORT_TIMED_OUT,79};
    const auto status=wse_capi_udp_client_receive_from(client,out,size,context);
    return mode==10 ? wse_capi_status{WSE_CAPI_ERROR_PROTOCOL,WSE_CAPI_TRANSPORT_DATAGRAM_TRUNCATED,77} : status;
}
wse_capi_status WSE_CAPI_CALL wse_capi_udp_datagram_payload(wse_capi_udp_datagram,uint8_t* out,size_t* size,size_t capacity)
{
    transfer_diagnostic=""; *size=3;
    if(out && capacity>=3) { out[0]=0;out[1]=127;out[2]=255; }
    return ok;
}
wse_capi_status WSE_CAPI_CALL wse_capi_udp_datagram_source(wse_capi_udp_datagram,char* out,size_t* size,uint16_t* port,size_t capacity)
{
    transfer_diagnostic=""; *size=10;*port=4321;
    if(out && capacity>=10) std::memcpy(out,"127.0.0.1",10);
    return ok;
}
namespace
{
wse_capi_status send(size_t* sent,size_t size)
{
    ++transfers;transfer_diagnostic="synthetic transfer";
    *sent=mode==8 ? 3 : mode==9 ? 1 : mode==12 ? 2 : mode==15 ? static_cast<size_t>(-1) : 0;
    if(mode==14) { *sent=size;return ok; }
    return {mode==9 ? WSE_CAPI_ERROR_CANCELLATION : WSE_CAPI_ERROR_TIMEOUT,
        mode==9 ? WSE_CAPI_TRANSPORT_CANCELLED : WSE_CAPI_TRANSPORT_TIMED_OUT,78};
}
}
wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_send(wse_capi_tcp_client,size_t* sent,const uint8_t*,size_t size,const wse_capi_operation_context*)
{ return send(sent,size); }
wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_send(wse_capi_serial_port,size_t* sent,const uint8_t*,size_t size,const wse_capi_operation_context*)
{ return send(sent,size); }
wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_send_to(wse_capi_udp_client,size_t* sent,const char*,uint16_t,const uint8_t*,size_t size,const wse_capi_operation_context*)
{ return send(sent,size); }
wse_capi_status WSE_CAPI_CALL wse_capi_http_request_create(wse_capi_http_request* out,int32_t,const char*)
{ *out=create<wse_capi_http_request>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_http_execute(wse_capi_http_request,wse_capi_http_response* out,size_t,const wse_capi_operation_context*)
{ if(mode==2) return failure; *out=create<wse_capi_http_response>(); return mode==1 ? failure : ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_http_execute_authenticated(wse_capi_http_request request,wse_capi_http_response* out,size_t count,const char*,const char*,const wse_capi_operation_context* context)
{ return wse_capi_http_execute(request,out,count,context); }
wse_capi_status WSE_CAPI_CALL wse_capi_http_response_status_code(wse_capi_http_response,uint16_t* out)
{ *out=mode==1 ? 404 : 200; return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_render_projection(wse_capi_projection_frame* frame,wse_capi_frame_buffer* out,const wse_capi_projection_request*)
{ *frame={}; frame->width=1; frame->height=1; frame->row_pitch=4; *out=create<wse_capi_frame_buffer>(); return ok; }

wse_capi_status WSE_CAPI_CALL wse_capi_camera_enumerate(wse_capi_camera_device_list* out,int32_t)
{ *out=create<wse_capi_camera_device_list>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_list_count(wse_capi_camera_device_list,size_t* out) { *out=3; return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_list_at(wse_capi_camera_device_list,wse_capi_camera_device* out,size_t)
{ *out=create<wse_capi_camera_device>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_capabilities(wse_capi_camera_device,wse_capi_camera_capability* out)
{ *out=create<wse_capi_camera_capability>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_device(wse_capi_camera_capability,wse_capi_camera_device* out)
{ *out=create<wse_capi_camera_device>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_extension_unit_count(wse_capi_camera_capability,size_t* out) { *out=3; return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_extension_unit_at(wse_capi_camera_capability,wse_capi_camera_xu_selector* out,size_t)
{ *out=create<wse_capi_camera_xu_selector>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_xu_selector_describe(wse_capi_camera_xu_selector,uint8_t* unit,uint8_t* selector,size_t* minimum,size_t* maximum,wse_capi_bool* readable,wse_capi_bool* writable)
{ *unit=1; *selector=1; *minimum=1; *maximum=mode==7 ? static_cast<size_t>(-1) : 4; *readable=1; *writable=1; return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_read_frame(wse_capi_camera,wse_capi_camera_frame* out,uint32_t)
{ *out=create<wse_capi_camera_frame>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_read_averaged_frame(wse_capi_camera,wse_capi_camera_frame* out,size_t,uint32_t)
{ *out=create<wse_capi_camera_frame>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_current_capabilities(wse_capi_camera,wse_capi_camera_capability* out)
{ *out=create<wse_capi_camera_capability>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_apply_orientation(wse_capi_camera_frame,wse_capi_camera_frame* out,int32_t)
{ *out=create<wse_capi_camera_frame>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_demosaic(wse_capi_camera_frame,wse_capi_camera_frame* out,int32_t,int32_t)
{ *out=create<wse_capi_camera_frame>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_accumulator_average(wse_capi_camera_frame_accumulator,wse_capi_camera_frame* out)
{ *out=create<wse_capi_camera_frame>(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_describe(wse_capi_camera_frame,wse_capi_camera_frame_description* out,uint64_t* sequence,int64_t* timestamp)
{ if(mode==3) return failure; *out={1,1,4,4}; *sequence=1; *timestamp=100; return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_data(wse_capi_camera_frame,uint8_t* out,size_t* size,size_t capacity)
{ *size=4; if(out && capacity>=4) std::memset(out,7,4); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_start(wse_capi_camera) { join(); return mode==4 ? failure : ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_stop(wse_capi_camera)
{ ++stops; join(); return mode==6 ? failure : ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_close(wse_capi_camera) { join(); return ok; }
wse_capi_status WSE_CAPI_CALL wse_capi_camera_start_with_callback(wse_capi_camera,wse_capi_camera_frame_callback callback,wse_capi_user_data user)
{
    join(); if(mode==4) return failure;
    worker=std::thread([callback,user]()
    {
        callback_thread=true;
        // Include late delivery after a stop request to verify frame disposal in
        // the registration's suppressed-delivery path. This is a synthetic stress.
        for(int i=0;i<3;++i)
            callback(mode==5 ? nullptr : create<wse_capi_camera_frame>(),mode==5 ? failure : ok,user);
    });
    return ok;
}

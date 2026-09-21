// Actual installed-shape C ABI calls and loopback UDP, without physical hardware.
#include <wse/capi/wse_capi_xpt.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#define CHECK(x) do { if(!(x)) { std::cerr << "line " << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
int main()
{
    wse_capi_operation_context context{1000,nullptr};
    size_t count=123;
    CHECK(wse_capi_tcp_client_send(nullptr,&count,nullptr,0,&context).category!=0 && count==0);
    count=123;
    CHECK(wse_capi_serial_port_send(nullptr,&count,nullptr,0,&context).category!=0 && count==0);
    count=123;
    CHECK(wse_capi_udp_client_send_to(nullptr,&count,nullptr,0,nullptr,0,&context).category!=0 && count==0);
    wse_capi_udp_client sender=nullptr,receiver=nullptr;
    CHECK(wse_capi_udp_client_create(&sender).category==0);
    CHECK(wse_capi_udp_client_create(&receiver).category==0);
    CHECK(wse_capi_udp_client_bind(receiver,"127.0.0.1",0,&context).category==0);
    char host[64]{};size_t required=0;uint16_t port=0;
    CHECK(wse_capi_udp_client_local_endpoint(receiver,host,&required,&port,sizeof(host)).category==0);
    const uint8_t payload[]{0,127,255,4,5};
    for(int mode=0;mode<3;++mode)
    {
        CHECK(wse_capi_udp_client_send_to(sender,&count,host,port,payload,sizeof(payload),&context).category==0);
        CHECK(count==sizeof(payload));
        wse_capi_udp_datagram datagram=nullptr;
        const auto status=mode==0 ? wse_capi_udp_client_receive_from(receiver,&datagram,3,&context)
            : wse_capi_udp_client_receive_from_with_progress(receiver,&datagram,mode==1 ? 3 : 5,&context);
        if(mode<2) CHECK(status.code==WSE_CAPI_TRANSPORT_DATAGRAM_TRUNCATED && std::strlen(wse_capi_last_error_message())>0);
        else CHECK(status.category==0);
        if(mode==0) CHECK(datagram==nullptr);
        else
        {
            CHECK(datagram!=nullptr);uint8_t bytes[5]{};size_t size=0;
            CHECK(wse_capi_udp_datagram_payload(datagram,bytes,&size,sizeof(bytes)).category==0);
            CHECK(size==(mode==1 ? 3U : 5U) && std::memcmp(bytes,payload,size)==0);
            uint16_t source_port=0;
            CHECK(wse_capi_udp_datagram_source(datagram,host,&required,&source_port,sizeof(host)).category==0);
            CHECK(source_port!=0 && std::strcmp(host,"127.0.0.1")==0);
            wse_capi_udp_datagram_destroy(datagram);
        }
        context.timeout_milliseconds=10;
        datagram=reinterpret_cast<wse_capi_udp_datagram>(1);
        const auto empty=wse_capi_udp_client_receive_from_with_progress(receiver,&datagram,5,&context);
        CHECK(empty.code==WSE_CAPI_TRANSPORT_TIMED_OUT && datagram==nullptr);
        context.timeout_milliseconds=1000;
    }
    wse_capi_udp_datagram output=reinterpret_cast<wse_capi_udp_datagram>(1);
    CHECK(wse_capi_udp_client_receive_from_with_progress(nullptr,&output,5,&context).category!=0 && !output);
    CHECK(wse_capi_udp_client_receive_from_with_progress(receiver,nullptr,5,&context).category!=0);
    wse_capi_udp_client_destroy(sender);wse_capi_udp_client_destroy(receiver);
    std::cout << "C ABI UDP: legacy output, truncated prefix/source, full payload, consumed suffix, output initialization passed.\n";
}

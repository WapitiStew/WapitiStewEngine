#include "../../lang/cs/native/transfer_result.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

#define CHECK(x) do { if(!(x)) { std::cerr << "line " << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
using namespace wse::xpt;
namespace
{
int live=0;
bool fail_construct=false;
struct Handle
{
    UdpDatagram value;
    explicit Handle(UdpDatagram value_in) : value(std::move(value_in))
    { if(fail_construct) throw std::bad_alloc(); ++live; }
    ~Handle() { --live; }
};
}
int main()
{
    using namespace wse::capi;
    int cases=0;
    for(const auto count : {std::size_t{0}, std::size_t{1}, std::size_t{7}, std::numeric_limits<std::size_t>::max()})
    {
        for(int mode=0;mode<4;++mode)
        {
            const auto category=mode==1 ? eTransportErrorCategory::Timeout
                : mode==2 ? eTransportErrorCategory::Cancellation : eTransportErrorCategory::InputOutput;
            const auto code=mode==1 ? eTransportErrorCode::TimedOut
                : mode==2 ? eTransportErrorCode::Cancelled : eTransportErrorCode::SendFailed;
            auto result=mode==0 ? TransferResult<std::size_t>(count)
                : TransferResult<std::size_t>(count,TransportError(category,code,"progress diagnostic",127));
            std::size_t output=999;
            const auto status=publishTransferCount(&output,result);
            CHECK(output==count && (status.category==0)==(mode==0));
            if(mode) CHECK(status.code==static_cast<int>(code) && status.native_code==127
                && std::strcmp(lastErrorMessage(),"progress diagnostic")==0);
            else CHECK(lastErrorMessage()[0]=='\0');
            ++cases;
        }
    }
    for(bool partial : {false,true})
    for(int mode=0;mode<4;++mode)
    {
        const auto code=mode==1 ? eTransportErrorCode::DatagramTruncated
            : mode==2 ? eTransportErrorCode::TimedOut : eTransportErrorCode::Cancelled;
        auto result=mode==0 ? TransferResult<UdpDatagram>(UdpDatagram(Endpoint("127.0.0.1",42),{0,127,255}))
            : TransferResult<UdpDatagram>(UdpDatagram(Endpoint("127.0.0.1",42),{0,127,255}),
                TransportError(eTransportErrorCategory::InputOutput,code,"datagram diagnostic",128));
        Handle* output=nullptr;
        const auto status=publishDatagram(&output,result,partial);
        const bool owns=mode==0 || (mode==1 && partial);
        CHECK((output!=nullptr)==owns && live==(owns ? 1 : 0));
        CHECK((status.category==0)==(mode==0));
        if(mode) CHECK(status.code==static_cast<int>(code) && status.native_code==128
            && std::strcmp(lastErrorMessage(),"datagram diagnostic")==0);
        if(output) CHECK(output->value.payload()==std::vector<std::uint8_t>({0,127,255})
            && output->value.source().port()==42);
        delete output; CHECK(live==0); ++cases;
    }
    // Failure after native consumption but before publication must leave no native owner.
    for(int mode=0;mode<3;++mode)
    {
        auto result=mode==2 ? TransferResult<UdpDatagram>(UdpDatagram(Endpoint("127.0.0.1",42),{1}),
            TransportError(eTransportErrorCategory::InputOutput,eTransportErrorCode::DatagramTruncated,"truncated"))
            : TransferResult<UdpDatagram>(UdpDatagram(Endpoint("127.0.0.1",42),{1}));
        Handle* output=nullptr;fail_construct=true;
        const auto status=guard([&] {return publishDatagram(&output,result,mode!=0);});
        fail_construct=false;
        CHECK(status.category==WSE_CAPI_ERROR_RESOURCE_EXHAUSTED && !output && live==0);++cases;
    }
    std::cout << cases << " native transfer-result cases passed.\n";
}

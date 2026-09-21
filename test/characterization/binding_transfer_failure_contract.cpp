#include "../../lang/common/transfer_failure.h"
#include <wse/binding/XptErrorAdapter.h>
#include <cstdlib>
#include <iostream>
#include <limits>

#define CHECK(x) do { if(!(x)) { std::cerr << "line " << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
int main()
{
    using namespace wse::xpt;
    using wse::binding::detail::transferFailure;
    unsigned cases = 0;
    for(auto count : {std::size_t{0}, std::size_t{7}, (std::numeric_limits<std::size_t>::max)()})
        for(auto code : {eTransportErrorCode::TimedOut, eTransportErrorCode::Cancelled, eTransportErrorCode::SendFailed})
        {
            const auto category = code == eTransportErrorCode::TimedOut ? eTransportErrorCategory::Timeout
                : code == eTransportErrorCode::Cancelled ? eTransportErrorCategory::Cancellation : eTransportErrorCategory::InputOutput;
            const TransferResult<std::size_t> result(count, TransportError(category, code, "send diagnostic", -127));
            const auto view = transferFailure(result);
            const auto error = wse::binding::fromXptError(view.error);
            CHECK(view.bytes_transferred == count && view.datagram == nullptr);
            CHECK(&view.error == &result.error() && error.code() == static_cast<int>(code)
                && error.nativeCode() == -127 && error.message() == "send diagnostic");
            ++cases;
        }
    for(auto code : {eTransportErrorCode::DatagramTruncated, eTransportErrorCode::TimedOut, eTransportErrorCode::Cancelled})
        for(bool empty : {false, true})
        {
            const TransferResult<UdpDatagram> result(UdpDatagram(Endpoint("127.0.0.1", 42),
                empty ? std::vector<std::uint8_t>{} : std::vector<std::uint8_t>{0, 127, 255}),
                TransportError(eTransportErrorCategory::InputOutput, code, "receive diagnostic", 128));
            const auto view = transferFailure(result);
            const bool truncated = code == eTransportErrorCode::DatagramTruncated;
            CHECK(view.datagram == (truncated ? &result.value() : nullptr));
            CHECK(view.bytes_transferred == (truncated && !empty ? 3U : 0U));
            CHECK(view.error.nativeCode() == 128 && view.error.message() == "receive diagnostic");
            if(truncated) CHECK(view.datagram->source().port() == 42);
            ++cases;
        }
    std::cout << cases << " synthetic binding transfer cases; no socket or serial device.\n";
}

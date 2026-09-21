#ifndef WSE_INTERNAL_BINDING_TRANSFER_FAILURE_H
#define WSE_INTERNAL_BINDING_TRANSFER_FAILURE_H

#include <xpt/stew.h>

namespace wse::binding::detail
{
// Borrow only during exception construction. The native Result remains the owner;
// each runtime copies bytes/endpoint before this view goes out of scope.
struct TransferFailureView final
{
    const xpt::TransportError& error;
    std::size_t bytes_transferred;
    const xpt::UdpDatagram* datagram;
};

inline TransferFailureView transferFailure(
    const xpt::TransferResult< std::size_t >& result_in ) noexcept
{
    return { result_in.error(), result_in.value(), nullptr };
}

inline TransferFailureView transferFailure(
    const xpt::TransferResult< xpt::UdpDatagram >& result_in ) noexcept
{
    const bool truncated = result_in.error().code() == xpt::eTransportErrorCode::DatagramTruncated;
    return { result_in.error(), truncated ? result_in.value().payload().size() : 0U,
        truncated ? &result_in.value() : nullptr };
}
} // namespace wse::binding::detail
#endif

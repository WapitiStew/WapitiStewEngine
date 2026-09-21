#ifndef WSE_CAPI_INTERNAL_TRANSFER_RESULT_H
#define WSE_CAPI_INTERNAL_TRANSFER_RESULT_H

#include "capi_internal.h"
#include <wse/binding/XptErrorAdapter.h>
#include <xpt/udp/UdpClient.h>
#include <memory>
#include <utility>

namespace wse::capi
{
inline wse_capi_status publishTransferCount(
    std::size_t* const p_sent_out, const wse::xpt::TransferResult<std::size_t>& result_in )
{
    auto& sent_out = *p_sent_out;
    // Publish progress before diagnostic conversion, which can itself allocate.
    sent_out = result_in.value();
    return result_in.succeeded() ? makeSuccess()
        : fromBindingError(wse::binding::fromXptError(result_in.error()));
}

template<class Handle>
wse_capi_status publishDatagram(
    Handle** const p_datagram_out, wse::xpt::TransferResult<wse::xpt::UdpDatagram>& result_inout,
    const bool preserve_partial_in )
{
    auto& datagram_out = *p_datagram_out;
    const bool failed = !result_inout.succeeded();
    if(failed && (!preserve_partial_in
        || result_inout.error().code() != wse::xpt::eTransportErrorCode::DatagramTruncated))
        return fromBindingError(wse::binding::fromXptError(result_inout.error()));

    auto owned = std::make_unique<Handle>(std::move(result_inout.value()));
    const auto status = failed
        ? fromBindingError(wse::binding::fromXptError(result_inout.error())) : makeSuccess();
    // Neither a failed allocation nor a failed diagnostic conversion publishes an owner.
    datagram_out = owned.release();
    return status;
}
}
#endif

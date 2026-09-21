//*****************************************************************************************************************
//!
//! @file    XptErrorAdapter.cpp
//! @brief   \~japanese XPT ErrorからBinding Errorへの変換の実装.
//! @brief   \~english  Implements conversion from XPT errors to binding errors.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include <wse/binding/XptErrorAdapter.h>
#include <xpt/error/TransportError.h>

namespace wse
{
namespace binding
{

// Category-first mapping, then two code-level overrides: Unsupported and ResourceExhausted are
// codes on the XPT side but categories on the binding side, and a binding branches on the
// category. The XPT code itself rides along unchanged as the binding error's code.
Error fromXptError( const xpt::TransportError& error_in )
{
    if ( error_in.ok() )
    {
        return Error();
    }

    eErrorCategory category = eErrorCategory::Internal;
    switch ( error_in.category() )
    {
        case xpt::eTransportErrorCategory::Validation:
            category = eErrorCategory::InvalidArgument;
            break;
        case xpt::eTransportErrorCategory::Resolution:
            category = eErrorCategory::NotFound;
            break;
        case xpt::eTransportErrorCategory::Connection:
        case xpt::eTransportErrorCategory::InputOutput:
            category = eErrorCategory::InputOutput;
            break;
        case xpt::eTransportErrorCategory::Timeout:
            category = eErrorCategory::Timeout;
            break;
        case xpt::eTransportErrorCategory::Cancellation:
            category = eErrorCategory::Cancellation;
            break;
        case xpt::eTransportErrorCategory::Protocol:
        case xpt::eTransportErrorCategory::Http:
            category = eErrorCategory::Protocol;
            break;
        case xpt::eTransportErrorCategory::Security:
            category = eErrorCategory::Security;
            break;
        case xpt::eTransportErrorCategory::None:
            category = eErrorCategory::None;
            break;
    }

    if ( error_in.code() == xpt::eTransportErrorCode::Unsupported )
    {
        category = eErrorCategory::Unsupported;
    }
    else if ( error_in.code() == xpt::eTransportErrorCode::ResourceExhausted )
    {
        category = eErrorCategory::ResourceExhausted;
    }

    return Error(
          category
        , static_cast<std::int32_t>( error_in.code() )
        , error_in.message()
        , error_in.nativeCode()
    );
}

} // namespace binding
} // namespace wse

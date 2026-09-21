//*****************************************************************************************************************
//!
//! @file    XptErrorAdapter.h
//! @brief   \~japanese XPT ErrorをBinding共通Errorへ変換する.
//! @brief   \~english  Converts XPT errors to the common binding error contract.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_BINDING_XPTERRORADAPTER_H
#define WONDERSTEWENGINE_WSE_BINDING_XPTERRORADAPTER_H

#include "Error.h"
#include "../../dynamic.h"

namespace wse
{
namespace xpt
{
class TransportError;
}

namespace binding
{

//! \~japanese XPTの安定Codeを保持したまま共通Categoryへ正規化する.
//! \~english  Normalizes XPT into a common category while preserving its stable code.
WSE_API Error fromXptError( const xpt::TransportError& error_in );

} // namespace binding
} // namespace wse

#endif // WONDERSTEWENGINE_WSE_BINDING_XPTERRORADAPTER_H

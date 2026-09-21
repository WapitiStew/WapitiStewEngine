//*****************************************************************************************************************
//! @file    TmrErrorAdapter.h
//! @brief   \~japanese Tmr CameraのErrorを共通Binding Error契約へ変換する.
//! @brief   \~english  Converts Tmr camera errors to the common binding error contract.
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_BINDING_TMRERRORADAPTER_H
#define WONDERSTEWENGINE_WSE_BINDING_TMRERRORADAPTER_H

#include "Error.h"
#include "../../dynamic.h"

namespace wse
{
namespace tmr
{
class CameraError;
}

namespace binding
{

//! \~japanese CameraErrorをPortableなError（Category、Code、Message、Native code）へ写す.
//!            CameraErrorは前方宣言だけで受け、公開HeaderへTmrの型を広げない.
//! \~english  Maps a CameraError onto the portable Error (category, code, message, native
//!            code). CameraError arrives as a forward declaration only, so this header does
//!            not spread Tmr types further.
WSE_API Error fromTmrError( const tmr::CameraError& error_in );

} // namespace binding
} // namespace wse

#endif // WONDERSTEWENGINE_WSE_BINDING_TMRERRORADAPTER_H

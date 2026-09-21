//*****************************************************************************************************************
//! @file    OuiErrorAdapter.h
//! @brief   \~japanese OUI RendererのErrorを共通Binding Error契約へ変換する.
//! @brief   \~english  Converts OUI renderer errors to the common binding error contract.
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_BINDING_OUIERRORADAPTER_H
#define WONDERSTEWENGINE_WSE_BINDING_OUIERRORADAPTER_H

#include "Error.h"
#include "../../dynamic.h"

namespace wse
{
namespace oui
{
class RendererError;
}

namespace binding
{

//! \~japanese RendererErrorをPortableなError（Category、Code、Message、Native code）へ写す.
//!            RendererErrorは前方宣言だけで受け、公開HeaderへOUIの型を広げない.
//! \~english  Maps a RendererError onto the portable Error (category, code, message, native
//!            code). RendererError arrives as a forward declaration only, so this header does
//!            not spread OUI types further.
WSE_API Error fromOuiError( const oui::RendererError& error_in );

} // namespace binding
} // namespace wse

#endif // WONDERSTEWENGINE_WSE_BINDING_OUIERRORADAPTER_H

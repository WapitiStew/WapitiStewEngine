//*****************************************************************************************************************
//! @file    OuiErrorAdapter.cpp
//! @brief   \~japanese OUI ErrorからBinding Errorへの正準変換の実装.
//! @brief   \~english  Implements the canonical OUI-to-binding error conversion.
//*****************************************************************************************************************

#include <wse/binding/OuiErrorAdapter.h>
#include <oui/renderer/RendererError.h>

namespace wse
{
namespace binding
{

Error fromOuiError( const oui::RendererError& error_in )
{
    using RendererCategory = oui::eRendererErrorCategory;
    eErrorCategory category = eErrorCategory::Internal;
    switch( error_in.category() )
    {
        case RendererCategory::None: category = eErrorCategory::None; break;
        case RendererCategory::Validation: category = eErrorCategory::InvalidArgument; break;
        case RendererCategory::Lifecycle: category = eErrorCategory::InvalidState; break;
        case RendererCategory::Resource: category = eErrorCategory::ResourceExhausted; break;
        case RendererCategory::Execution: category = eErrorCategory::Internal; break;
        case RendererCategory::Timeout: category = eErrorCategory::Timeout; break;
        case RendererCategory::Unsupported: category = eErrorCategory::Unsupported; break;
        case RendererCategory::Backend: category = eErrorCategory::Internal; break;
    }
    return Error( category, static_cast< std::int32_t >( error_in.code() ),
        error_in.message(), error_in.nativeCode() );
}

} // namespace binding
} // namespace wse

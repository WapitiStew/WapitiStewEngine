//*****************************************************************************************************************
//! @file    TmrErrorAdapter.cpp
//! @brief   \~japanese Tmr ErrorからBinding Errorへの正準変換の実装.
//! @brief   \~english  Implements the canonical Tmr-to-binding error conversion.
//*****************************************************************************************************************

#include <wse/binding/TmrErrorAdapter.h>
#include <tmr/camera/CameraError.h>

namespace wse
{
namespace binding
{

Error fromTmrError( const tmr::CameraError& error_in )
{
    using CameraCategory = tmr::eCameraErrorCategory;
    eErrorCategory category = eErrorCategory::Internal;
    switch( error_in.category() )
    {
        case CameraCategory::None: category = eErrorCategory::None; break;
        case CameraCategory::Validation: category = eErrorCategory::InvalidArgument; break;
        case CameraCategory::Lifecycle: category = eErrorCategory::InvalidState; break;
        case CameraCategory::Device: category = eErrorCategory::NotFound; break;
        case CameraCategory::InputOutput: category = eErrorCategory::InputOutput; break;
        case CameraCategory::Timeout: category = eErrorCategory::Timeout; break;
        case CameraCategory::Unsupported: category = eErrorCategory::Unsupported; break;
        case CameraCategory::Backend: category = eErrorCategory::Internal; break;
    }
    if( error_in.code() == tmr::eCameraErrorCode::ResourceExhausted )
        category = eErrorCategory::ResourceExhausted;
    return Error( category, static_cast< std::int32_t >( error_in.code() ),
        error_in.message(), error_in.nativeCode() );
}

} // namespace binding
} // namespace wse

#include "../../platform/tmr/win/camera/MediaFoundationReadError.h"
#include <iostream>

int main()
{
    using namespace wse::tmr;
    for( const HRESULT value : { E_FAIL, HRESULT_FROM_WIN32( ERROR_NO_MATCH ), S_OK } )
    {
        const auto error = detail::mediaFoundationReadError( value );
        if( error.category() != eCameraErrorCategory::Backend || error.code() != eCameraErrorCode::ReadFailed
            || error.nativeCode() != value )
        { std::cerr << "Generic sample failure/EOS must not assert device removal.\n"; return 1; }
    }
    for( const HRESULT value : { MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED,
        HRESULT_FROM_WIN32( ERROR_DEVICE_NOT_CONNECTED ) } )
    {
        const auto error = detail::mediaFoundationReadError( value );
        if( error.category() != eCameraErrorCategory::Device || error.code() != eCameraErrorCode::DeviceDisconnected
            || error.nativeCode() != value )
        { std::cerr << "Explicit invalidation must retain device category and native status.\n"; return 1; }
    }
    return 0;
}

//*****************************************************************************************************************
//! @file    CameraCalibration.cpp
//! @brief   \~japanese Deviceに依存しないTmr Camera校正の検証.
//! @brief   \~english  Device-independent Tmr camera-calibration validation.
//! @author  WapitiStew.
//! @date    Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include <tmr/calibration/CameraCalibration.h>

#include <cmath>
#include <limits>

namespace wse
{
namespace tmr
{
namespace
{

template< typename T, std::size_t Size >
bool allFinite( const std::array< T, Size >& values_in ) noexcept
{
    for( const T value : values_in )
    {
        if( !std::isfinite( static_cast< double >( value ) ) )
            return false;
    }
    return true;
}

} // namespace

bool sCameraIntrinsics::valid() const noexcept
{
    return this->image_width > 0U && this->image_height > 0U
        && std::isfinite( this->focal_length_x ) && this->focal_length_x > 0.0
        && std::isfinite( this->focal_length_y ) && this->focal_length_y > 0.0
        && std::isfinite( this->principal_point_x )
        && std::isfinite( this->principal_point_y )
        && std::isfinite( this->skew );
}

bool sCameraDistortion::valid() const noexcept
{
    return ( this->model == eCameraDistortionModel::None
            || this->model == eCameraDistortionModel::BrownConrady )
        && allFinite( this->radial ) && allFinite( this->tangential );
}

bool sCameraColorCalibration::valid() const noexcept
{
    return allFinite( this->rgb_to_xyz );
}

bool sCameraLensShadingMap::valid() const noexcept
{
    if( this->grid_width == 0U || this->grid_height == 0U
        || ( this->channels != 1U && this->channels != 3U && this->channels != 4U ) )
        return false;
    const std::size_t width = this->grid_width;
    const std::size_t height = this->grid_height;
    if( width > std::numeric_limits< std::size_t >::max() / height )
        return false;
    const std::size_t cells = width * height;
    if( cells > std::numeric_limits< std::size_t >::max() / this->channels
        || this->gains.size() != cells * this->channels )
        return false;
    for( const float gain : this->gains )
    {
        if( !std::isfinite( gain ) || gain <= 0.0F )
            return false;
    }
    return true;
}

bool sCameraCalibration::valid() const noexcept
{
    if( this->camera_id.empty() )
        return false;
    if( this->intrinsics && !this->intrinsics->valid() )
        return false;
    if( this->distortion && !this->distortion->valid() )
        return false;
    if( this->color && !this->color->valid() )
        return false;
    if( this->lens_shading && !this->lens_shading->valid() )
        return false;
    return this->intrinsics.has_value() || this->distortion.has_value()
        || this->color.has_value() || this->lens_shading.has_value();
}

} // namespace tmr
} // namespace wse

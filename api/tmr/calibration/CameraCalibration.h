//*****************************************************************************************************************
//!
//! @file    CameraCalibration.h
//! @brief   \~japanese Device I/Oに依存しないCamera Calibration値を定義する.
//! @brief   \~english  Defines camera-calibration values independent of device I/O.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-28, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_TMR_CALIBRATION_CAMERACALIBRATION_H
#define WONDERSTEWENGINE_TMR_CALIBRATION_CAMERACALIBRATION_H

#include <utility>
#include "../../dynamic.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace wse
{
namespace tmr
{

enum class eCameraDistortionModel : std::uint8_t
{
      None = 0U
    , BrownConrady
};

struct sCameraIntrinsics
{
    std::uint32_t image_width;
    std::uint32_t image_height;
    double        focal_length_x;
    double        focal_length_y;
    double        principal_point_x;
    double        principal_point_y;
    double        skew;

    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraIntrinsics(
          std::uint32_t image_width_in = 0U
        , std::uint32_t image_height_in = 0U
        , double focal_length_x_in = 0.0
        , double focal_length_y_in = 0.0
        , double principal_point_x_in = 0.0
        , double principal_point_y_in = 0.0
        , double skew_in = 0.0
    )
        : image_width       ( image_width_in )
        , image_height      ( image_height_in )
        , focal_length_x    ( focal_length_x_in )
        , focal_length_y    ( focal_length_y_in )
        , principal_point_x ( principal_point_x_in )
        , principal_point_y ( principal_point_y_in )
        , skew              ( skew_in )
    {
    }
};

struct sCameraDistortion
{
    eCameraDistortionModel model;
    std::array< double, 6U > radial;
    std::array< double, 2U > tangential;

    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraDistortion(
          eCameraDistortionModel model_in = eCameraDistortionModel::None
        , const std::array< double, 6U >& radial_in = {}
        , const std::array< double, 2U >& tangential_in = {}
    )
        : model      ( model_in )
        , radial     ( radial_in )
        , tangential ( tangential_in )
    {
    }
};

struct sCameraColorCalibration
{
    std::array< double, 9U > rgb_to_xyz;

    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraColorCalibration(
          const std::array< double, 9U >& rgb_to_xyz_in = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0
    }
    )
        : rgb_to_xyz ( rgb_to_xyz_in )
    {
    }
};

struct sCameraLensShadingMap
{
    std::uint32_t       grid_width;
    std::uint32_t       grid_height;
    std::uint8_t        channels;
    std::vector< float > gains;

    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraLensShadingMap(
          std::uint32_t grid_width_in = 0U
        , std::uint32_t grid_height_in = 0U
        , std::uint8_t channels_in = 0U
        , const std::vector< float >& gains_in = {}
    )
        : grid_width  ( grid_width_in )
        , grid_height ( grid_height_in )
        , channels    ( channels_in )
        , gains       ( gains_in )
    {
    }
};

//! \~japanese CalibrationをDevice read/write操作から分離した所有値.
//! \~english  Owned calibration value separated from device read/write operations.
struct sCameraCalibration
{
    std::string                                camera_id;
    std::optional< sCameraIntrinsics >          intrinsics;
    std::optional< sCameraDistortion >          distortion;
    std::optional< sCameraColorCalibration >    color;
    std::optional< sCameraLensShadingMap >      lens_shading;

    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraCalibration(
          const std::string& camera_id_in = {}
        , const std::optional< sCameraIntrinsics >& intrinsics_in = {}
        , const std::optional< sCameraDistortion >& distortion_in = {}
        , const std::optional< sCameraColorCalibration >& color_in = {}
        , const std::optional< sCameraLensShadingMap >& lens_shading_in = {}
    )
        : camera_id    ( camera_id_in )
        , intrinsics   ( intrinsics_in )
        , distortion   ( distortion_in )
        , color        ( color_in )
        , lens_shading ( lens_shading_in )
    {
    }
};

} // namespace tmr
} // namespace wse

#endif // WONDERSTEWENGINE_TMR_CALIBRATION_CAMERACALIBRATION_H

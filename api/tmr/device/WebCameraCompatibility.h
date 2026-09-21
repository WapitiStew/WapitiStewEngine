//*****************************************************************************************************************
//!
//! @file    WebCameraCompatibility.h
//! @brief   \~japanese 旧WebCamera APIのPortable互換型を定義する.
//! @brief   \~english  Defines portable compatibility types for the historical WebCamera API.
//! @author  WapitiStew.
//! @date    Aug-29, 2026   Create New.
//!
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_TMR_DEVICE_WEBCAMERA_COMPATIBILITY_H
#define WONDERSTEWENGINE_TMR_DEVICE_WEBCAMERA_COMPATIBILITY_H
#define WSE_TMR_WEBCAMERA_COMPATIBILITY_TYPES_PROVIDED 1

#include <cstdint>
#include <string>

namespace wse
{
namespace tmr
{

enum class eEvent : std::int32_t
{
      TASK_IDLE               =  0
    , TASK_START              =  1
    , TASK_SUCCESS            =  5
    , TASK_FAILED             = -1
    , TASK_DEVICE_UNSUPPORTED = -2
    , TASK_UNKNOWN_ERROR      = -3
    , TASK_TIMEOUT            = -4
};

enum class eControlMode : std::uint8_t
{
      MANUAL      = ( 1U << 1U )
    , AUTO        = ( 1U << 2U )
    , AUTO_ADJUST = ( 1U << 3U )
};

inline std::string toString( const eControlMode mode_in )
{
    switch( mode_in )
    {
        case eControlMode::MANUAL     : return "Manual";
        case eControlMode::AUTO       : return "Auto";
        case eControlMode::AUTO_ADJUST: return "AutoAdjust";
        default                       : return "";
    }
}

enum class eAutoState : std::int32_t
{
      UNLOCKED    = 0x0121
    , LOCKED      = 0x0122
    , CONTORL_OFF = 0x0123
};

enum class eAutoStep : std::int32_t
{
      IDLE    = 0x0121
    , RUNNING = 0x0122
    , SUCCESS = 0x0123
    , FAILED  = 0x0124
};

enum class eParam : std::uint8_t
{
      Zoom                  = 0
    , Exposure              = 1
    , Iris                  = 2
    , Focus                 = 3
    , Brightness            = 4
    , Contrast              = 5
    , Hue                   = 6
    , Saturation            = 7
    , Sharpness             = 8
    , Gamma                 = 9
    , ColorEnable           = 10
    , WhiteBalance          = 11
    , BacklightCompensation = 12
    , Gain                  = 13
};

enum class eDOR : std::uint8_t
{
      Raw           = 0
    , Roll180       = 1
    , FlipLeftRight = 2
    , FlipTopBottom = 3
};

} // namespace tmr
} // namespace wse

#endif // WONDERSTEWENGINE_TMR_DEVICE_WEBCAMERA_COMPATIBILITY_H

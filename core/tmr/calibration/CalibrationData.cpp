//*****************************************************************************************************************
//! @file    CalibrationData.cpp
//! @brief   \~japanese Device I/Oと独立に実体化されるPortable Tmr校正値型.
//! @brief   \~english  Portable Tmr calibration value types are emitted independently of device I/O.
//! @author  WapitiStew.
//! @date    Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include <tmr/data/ColorMatrix.h>

// The legacy calibration value types are header-defined exported classes, so some translation
// unit has to name them for the shared library to export them. This one is device-independent,
// which keeps that export away from WebCamera. The types belonging to an access-controlled
// device moved out with it and are emitted by the extension that owns them.

//*****************************************************************************************************************
//!
//! @file    stew.h
//! @brief   \~japanese WSE Binding Native facadeの公開入口.
//! @brief   \~english  Public entry point for the WSE binding native facade.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_BINDING_STEW_H
#define WONDERSTEWENGINE_WSE_BINDING_STEW_H

#include "Cancellation.h"
#include "Error.h"
#include "FrameBuffer.h"
#include "Runtime.h"

#if defined( WSE_HAS_XPT )
#include "XptErrorAdapter.h"
#endif
#if defined( WSE_HAS_OUI )
#include "OuiErrorAdapter.h"
#endif
#if defined( WSE_HAS_TMR )
#include "TmrErrorAdapter.h"
#endif
#if defined( WSE_HAS_IUI )
#include "IuiErrorAdapter.h"
#endif

#endif // WONDERSTEWENGINE_WSE_BINDING_STEW_H

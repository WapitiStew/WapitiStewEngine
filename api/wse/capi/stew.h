//*****************************************************************************************************************
//!
//! @file    stew.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese 平坦C ABIの公開入口.
//! @brief   \~english  Public entry point for the flat C ABI.
//!
//! @details
//!     \~japanese
//!     @n C++ Classを直接呼び出せないLanguage Runtime向けの公開入口である。
//!        個別Headerではなく本Headerを入口として使用する。
//!     \~english
//!     @n Public entry point for language runtimes that cannot call C++ classes directly.
//!        Consumers include this header instead of the individual component headers.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_CAPI_STEW_H
#define WONDERSTEWENGINE_WSE_CAPI_STEW_H

#include "wse_capi.h"
#include "wse_capi_core.h"

#if defined( WSE_HAS_XPT )
#include "wse_capi_xpt.h"
#endif
#if defined( WSE_HAS_IUI )
#include "wse_capi_iui.h"
#endif
#if defined( WSE_HAS_OUI )
#include "wse_capi_oui.h"
#endif
#if defined( WSE_HAS_TMR )
#include "wse_capi_tmr.h"
#endif
#if defined( WSE_HAS_VPJ )
#include "wse_capi_vpj.h"
#endif

#endif // WONDERSTEWENGINE_WSE_CAPI_STEW_H

//*****************************************************************************************************************
//!
//! @file    component_addons.h
//! @brief   \~japanese 各Component addonの登録関数を宣言する. addon.cppがModule初期化時に順に呼ぶ.
//! @brief   \~english  Declares the per-component addon registration functions addon.cpp calls in turn at
//!                     module initialization.
//!
//! @date
//!   Sep-01, 2026   Create New.
//*****************************************************************************************************************

#pragma once

#include <node_api.h>

napi_status registerTmrAddon( napi_env env_in, napi_value exports_in );
napi_status registerOuiAddon( napi_env env_in, napi_value exports_in );
napi_status registerIuiAddon( napi_env env_in, napi_value exports_in );
napi_status registerXptAddon( napi_env env_in, napi_value exports_in );
#ifdef WSE_HAS_VPJ
// Supplied by the extension overlay that provides the component.
napi_status registerVpjAddon( napi_env env_in, napi_value exports_in );
#endif

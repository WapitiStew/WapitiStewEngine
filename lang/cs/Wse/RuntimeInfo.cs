//*****************************************************************************************************************
//!
//! @file    RuntimeInfo.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese RuntimeとBinding ABIのPortable情報.
//! @brief   \~english  Portable runtime and binding-ABI information.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

namespace WapitiStew.Wse;

/// <summary>Reports which optional components are compiled into the loaded native build.</summary>
/// <param name="HasXpt">True when the XPT transport component is available.</param>
/// <param name="HasTmr">True when the Tmr camera component is available.</param>
/// <param name="HasOui">True when the OUI output and rendering component is available.</param>
/// <param name="HasGef">True when the GEF generic file component is available.</param>
/// <param name="HasIui">True when the IUI input component is available.</param>
/// <param name="HasVpj">True when the VPJ projector component is available.</param>
public readonly record struct ComponentAvailability(
    bool HasXpt,
    bool HasTmr,
    bool HasOui,
    bool HasGef,
    bool HasIui,
    bool HasVpj);

/// <summary>
/// Describes the loaded native runtime. The shape matches <c>runtimeInfo()</c> in the JavaScript
/// binding, <c>runtime_info()</c> in Python, and <c>WseRuntime.info()</c> in Java.
/// </summary>
/// <param name="Version">Semantic version of the native library.</param>
/// <param name="BindingAbiVersion">Binding ABI version shared by every language binding.</param>
/// <param name="Components">Optional components compiled into this build.</param>
public readonly record struct RuntimeInfo(
    string Version,
    uint BindingAbiVersion,
    ComponentAvailability Components);

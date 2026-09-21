//*****************************************************************************************************************
//!
//! @file    ErrorCategory.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese 全Bindingで同じ値を使うPortable Error分類.
//! @brief   \~english  Portable error categories shared by every binding.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

namespace WapitiStew.Wse;

/// <summary>
/// Portable error categories. The values match <c>wse::binding::eErrorCategory</c> and the
/// JavaScript, Python, and Java bindings.
/// </summary>
public enum ErrorCategory
{
    /// <summary>No failure occurred.</summary>
    None = 0,

    /// <summary>An argument was outside its documented contract.</summary>
    InvalidArgument = 1,

    /// <summary>The requested device, resource, or entry does not exist.</summary>
    NotFound = 2,

    /// <summary>The operation is not valid for the current object state.</summary>
    InvalidState = 3,

    /// <summary>An input or output operation failed.</summary>
    InputOutput = 4,

    /// <summary>The operation did not complete within its deadline.</summary>
    Timeout = 5,

    /// <summary>The operation observed a cooperative cancellation request.</summary>
    Cancellation = 6,

    /// <summary>A protocol or wire-format contract was violated.</summary>
    Protocol = 7,

    /// <summary>The operation was refused for a security or permission reason.</summary>
    Security = 8,

    /// <summary>The operation is not supported by this build or platform.</summary>
    Unsupported = 9,

    /// <summary>A required resource, such as memory or handles, was exhausted.</summary>
    ResourceExhausted = 10,

    /// <summary>An unexpected internal failure occurred.</summary>
    Internal = 11,
}

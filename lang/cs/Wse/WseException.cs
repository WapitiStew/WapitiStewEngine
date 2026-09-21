//*****************************************************************************************************************
//!
//! @file    WseException.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Native失敗をManagedへ伝える例外型.
//! @brief   \~english  Exception type that carries a native failure into managed code.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;

namespace WapitiStew.Wse;

/// <summary>
/// Reports a WonderStewEngine native failure. The category, code, and native code carry the same
/// values as the <c>WseError</c> surfaces of the JavaScript, Python, and Java bindings.
/// </summary>
public class WseException : Exception
{
    /// <summary>Initializes a new instance from a native error contract.</summary>
    /// <param name="category">Portable error category.</param>
    /// <param name="code">Component-specific code.</param>
    /// <param name="nativeCode">Native operating-system or third-party code.</param>
    /// <param name="message">Human-readable failure description.</param>
    public WseException(ErrorCategory category, int code, long nativeCode, string message)
        : base(message)
    {
        Category = category;
        Code = code;
        NativeCode = nativeCode;
    }

    /// <summary>Gets the portable error category.</summary>
    public ErrorCategory Category { get; }

    /// <summary>Gets the component-specific error code.</summary>
    public int Code { get; }

    /// <summary>Gets the native operating-system or third-party error code.</summary>
    public long NativeCode { get; }
}

/// <summary>
/// Reports a 4xx or 5xx answer. The exchange finished and the server replied, so unlike every
/// other failure this one still carries the response Core produced. Dispose it with the exception.
/// </summary>
public sealed class HttpStatusException : WseException
{
    /// <summary>Initializes a new instance from a native error contract and the received response.</summary>
    /// <param name="category">Portable error category.</param>
    /// <param name="code">Component-specific code.</param>
    /// <param name="nativeCode">Native operating-system or third-party code.</param>
    /// <param name="message">Human-readable failure description.</param>
    /// <param name="response">Response the server returned. The exception owns it.</param>
    public HttpStatusException(
        ErrorCategory category, int code, long nativeCode, string message, HttpResponse response)
        : base(category, code, nativeCode, message)
    {
        Response = response;
    }

    /// <summary>Gets the response the server returned. Never null.</summary>
    public HttpResponse Response { get; }
}

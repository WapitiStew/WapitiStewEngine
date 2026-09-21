//*****************************************************************************************************************
//!
//! @file    HttpClient.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese XPT HTTP Client、RequestおよびResponse.
//! @brief   \~english  XPT HTTP client, request, and response.
//!
//! @details
//!     \~japanese
//!     @n BindingはRequestを非idempotentとして扱い、一時失敗のRetryを行わない。再実行の安全性は
//!        呼出元が判断する。
//!     \~english
//!     @n The binding treats a request as non-idempotent and never retries transient failures; the
//!        caller decides when a repeat is safe.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace WapitiStew.Wse;

/// <summary>HTTP request method.</summary>
public enum HttpMethod
{
    /// <summary>GET.</summary>
    Get = 0,
    /// <summary>HEAD.</summary>
    Head = 1,
    /// <summary>POST.</summary>
    Post = 2,
    /// <summary>PUT.</summary>
    Put = 3,
    /// <summary>PATCH.</summary>
    Patch = 4,
    /// <summary>DELETE.</summary>
    Delete = 5,
}

/// <summary>One HTTP header field.</summary>
/// <param name="Name">Field name.</param>
/// <param name="Value">Field value.</param>
public readonly record struct HttpHeader(string Name, string Value);

/// <summary>Owns the native HTTP request handle and releases it exactly once.</summary>
internal sealed class HttpRequestHandle : SafeHandle
{
    internal HttpRequestHandle()
        : base(IntPtr.Zero, ownsHandle: true)
    {
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Attach(IntPtr value) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        NativeMethods.wse_capi_http_request_destroy(handle);
        return true;
    }
}

/// <summary>Owns the native HTTP response handle and releases it exactly once.</summary>
internal sealed class HttpResponseHandle : SafeHandle
{
    internal HttpResponseHandle()
        : base(IntPtr.Zero, ownsHandle: true)
    {
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Attach(IntPtr value) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        NativeMethods.wse_capi_http_response_destroy(handle);
        return true;
    }
}

/// <summary>An HTTP request under construction. Dispose it when finished.</summary>
public sealed class HttpRequest : IDisposable
{
    private readonly HttpRequestHandle _handle;

    /// <summary>Creates a request.</summary>
    /// <param name="method">Request method.</param>
    /// <param name="url">Absolute request URL.</param>
    /// <exception cref="ArgumentNullException"><paramref name="url"/> is null.</exception>
    /// <exception cref="WseException">The native request could not be created.</exception>
    public HttpRequest(HttpMethod method, string url)
    {
        ArgumentNullException.ThrowIfNull(url);
        using var output = new NativeOutput(NativeMethods.wse_capi_http_request_destroy);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_http_request_create(
                out output.Value, (int)method, TransportMarshal.Utf8(url)));
        _handle = new HttpRequestHandle();
        _handle.Attach(output.Take());
    }

    /// <summary>Adds one header field.</summary>
    /// <param name="name">Field name.</param>
    /// <param name="value">Field value.</param>
    /// <exception cref="ArgumentNullException">A parameter is null.</exception>
    public void AddHeader(string name, string value)
    {
        ArgumentNullException.ThrowIfNull(name);
        ArgumentNullException.ThrowIfNull(value);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_http_request_add_header(
                Handle, TransportMarshal.Utf8(name), TransportMarshal.Utf8(value)));
    }

    /// <summary>Sets the request body. The native side copies the bytes and retains nothing.</summary>
    /// <param name="body">Body bytes.</param>
    /// <exception cref="ArgumentNullException"><paramref name="body"/> is null.</exception>
    public void SetBody(byte[] body)
    {
        ArgumentNullException.ThrowIfNull(body);
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_http_request_set_body(Handle, body, (nuint)body.Length));
    }

    /// <summary>Releases the native request.</summary>
    public void Dispose() => _handle.Dispose();

    internal SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(HttpRequest));
            }
            return _handle;
        }
    }
}

/// <summary>A received HTTP response. Dispose it when finished.</summary>
public sealed class HttpResponse : IDisposable
{
    private readonly HttpResponseHandle _handle;

    internal HttpResponse(NativeOutput output)
    {
        _handle = new HttpResponseHandle();
        _handle.Attach(output.Take());
    }

    /// <summary>Gets the HTTP status code.</summary>
    public int StatusCode
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_http_response_status_code(Handle, out ushort code));
            return code;
        }
    }

    /// <summary>Gets the number of attempts, including retries performed by the transport.</summary>
    public int AttemptCount
    {
        get
        {
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_http_response_attempt_count(Handle, out uint count));
            return checked((int)count);
        }
    }

    /// <summary>Reads every response header field.</summary>
    /// <returns>The header fields in the order the server sent them.</returns>
    public IReadOnlyList<HttpHeader> Headers()
    {
        SafeHandle raw = Handle;
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_http_response_header_count(raw, out nuint count));

        var headers = new List<HttpHeader>(checked((int)count));
        for (nuint index = 0; index < count; ++index)
        {
            nuint current = index;
            string name = TransportMarshal.ReadString(
                (byte[]? buffer, nuint capacity, out nuint size) =>
                    NativeMethods.wse_capi_http_response_header_name(
                        raw, buffer, out size, current, capacity));
            string value = TransportMarshal.ReadString(
                (byte[]? buffer, nuint capacity, out nuint size) =>
                    NativeMethods.wse_capi_http_response_header_value(
                        raw, buffer, out size, current, capacity));
            headers.Add(new HttpHeader(name, value));
        }
        return headers;
    }

    /// <summary>Copies the response body into a managed array.</summary>
    /// <returns>The body bytes.</returns>
    public byte[] Body()
    {
        SafeHandle raw = Handle;
        return TransportMarshal.ReadBytes((byte[]? buffer, nuint capacity, out nuint size) =>
            NativeMethods.wse_capi_http_response_body(raw, buffer, out size, capacity));
    }

    /// <summary>Releases the native response.</summary>
    public void Dispose() => _handle.Dispose();

    private SafeHandle Handle
    {
        get
        {
            if (_handle.IsClosed || _handle.IsInvalid)
            {
                throw new ObjectDisposedException(nameof(HttpResponse));
            }
            return _handle;
        }
    }
}

/// <summary>Executes HTTP requests with an explicit deadline and response-size ceiling.</summary>
public static class HttpClient
{
    /// <summary>Executes a request.</summary>
    /// <param name="request">Request to execute.</param>
    /// <param name="maximumResponseBodySize">Accepted response-body ceiling in bytes.</param>
    /// <param name="context">Explicit deadline and optional cancellation.</param>
    /// <returns>The response. Dispose it when finished.</returns>
    /// <exception cref="ArgumentNullException"><paramref name="request"/> is null.</exception>
    /// <exception cref="WseException">
    /// The request failed. An oversized body reports
    /// <see cref="TransportErrorCode.ResponseTooLarge"/>.
    /// </exception>
    /// <exception cref="HttpStatusException">
    /// The server answered with a 4xx or 5xx status. The exception carries that response, so the
    /// status code stays readable after the failure.
    /// </exception>
    public static HttpResponse Execute(
        HttpRequest request, int maximumResponseBodySize, OperationContext context)
    {
        ArgumentNullException.ThrowIfNull(request);
        using var cancellationLease = context.Cancellation?.Acquire();
        NativeOperationContext native = context.ToNative(cancellationLease);
        using var output = new NativeOutput(NativeMethods.wse_capi_http_response_destroy);
        NativeStatus status = NativeMethods.wse_capi_http_execute(
            request.Handle, out output.Value, (nuint)maximumResponseBodySize, ref native);
        return CompleteExecution( status, output );
    }

    /// <summary>Executes a request with server-negotiated authentication.</summary>
    /// <param name="request">Request to execute.</param>
    /// <param name="maximumResponseBodySize">Accepted response-body ceiling in bytes.</param>
    /// <param name="username">Credential user name.</param>
    /// <param name="secret">Credential secret.</param>
    /// <param name="context">Explicit deadline and optional cancellation.</param>
    /// <returns>The response. Dispose it when finished.</returns>
    /// <exception cref="ArgumentNullException">A required parameter is null.</exception>
    /// <exception cref="WseException">The request failed.</exception>
    /// <exception cref="HttpStatusException">
    /// The server answered with a 4xx or 5xx status, and the exception carries that response.
    /// </exception>
    public static HttpResponse ExecuteAuthenticated(
        HttpRequest request,
        int maximumResponseBodySize,
        string username,
        string secret,
        OperationContext context)
    {
        ArgumentNullException.ThrowIfNull(request);
        ArgumentNullException.ThrowIfNull(username);
        ArgumentNullException.ThrowIfNull(secret);
        using var cancellationLease = context.Cancellation?.Acquire();
        NativeOperationContext native = context.ToNative(cancellationLease);
        using var output = new NativeOutput(NativeMethods.wse_capi_http_response_destroy);
        NativeStatus status = NativeMethods.wse_capi_http_execute_authenticated(
            request.Handle,
            out output.Value,
            (nuint)maximumResponseBodySize,
            TransportMarshal.Utf8(username),
            TransportMarshal.Utf8(secret),
            ref native);
        return CompleteExecution( status, output );
    }

    /// <summary>
    /// Turns one native execution result into a response or an exception. A 4xx or 5xx answer is a
    /// failure that still produced a response, so it becomes an <see cref="HttpStatusException"/>
    /// carrying it rather than an error that throws the answer away.
    /// </summary>
    /// <param name="status">Status the native call returned.</param>
    /// <param name="response">Response handle the native call produced, or zero.</param>
    /// <returns>The response on success.</returns>
    private static HttpResponse CompleteExecution( NativeStatus status, NativeOutput response )
    {
        if ( status.Category == (int)ErrorCategory.None )
        {
            return new HttpResponse( response );
        }

        string message = NativeMethods.LastErrorMessage();
        if ( message.Length == 0 )
        {
            message = "The WonderStewEngine native call failed.";
        }
        if ( response.Value == IntPtr.Zero )
        {
            throw new WseException(
                (ErrorCategory)status.Category, status.Code, status.NativeCode, message );
        }
        var received = new HttpResponse(response);
        HttpStatusException failure;
        try
        {
            NativeOutput.Checkpoint("http-exception");
            failure = new HttpStatusException((ErrorCategory)status.Category,
                status.Code, status.NativeCode, message, received);
        }
        catch
        {
            received.Dispose();
            throw;
        }
        throw failure;
    }
}

//*****************************************************************************************************************
//!
//! @file    HttpGetExample.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese XPT HTTP ClientによるGET 1回と、Status、試行回数、Header数、Body Sizeの報告.
//! @brief   \~english  One GET through the XPT HTTP client, reporting status, attempts, header count, and body size.
//!
//! @details
//!     \~japanese
//!     @n 引数順序は arguments[0] がNative Library Path、arguments[1] がURLである。URLが無い場合は、
//!        その旨を出力して0を返す。
//!     @n 到達不能なURLも失敗ではない。構造化Errorを出力して0を返す。分岐はCategoryとCodeで行い、
//!        Message文字列では行わない。
//!     @n C++版のHttpExecutionOptionsはBindingに無い。Execute はBody上限のみを取り、Retryは行わない。
//!     @n C++版は4xx／5xxでもresult.value()にResponseを保持するが、Bindingは
//!        HttpStatusExceptionを送出する。この例外はResponseを運ぶため、失敗時もStatus Codeを
//!        読める。Transport自体の失敗はWseExceptionであり、その場合Responseは存在しない。
//!     \~english
//!     @n Argument order: arguments[0] is the native library path and arguments[1] is the URL.
//!        Without a URL the sample explains that and returns zero.
//!     @n An unreachable URL is not a failure either: the structured error is printed and the sample
//!        still returns zero. Branch on the category and the code, never on the message text.
//!     @n The C++ HttpExecutionOptions type is not bound. Execute takes only the response-body
//!        ceiling and performs no retry.
//!     @n On a 4xx or 5xx answer the C++ counterpart still hands back the received response in
//!        result.value(). The binding raises HttpStatusException, which carries that response, so
//!        the status code stays readable. A transport failure raises WseException and has no
//!        response to hand back.
//!     @n The C++ counterpart reports through the WSE log and also aims a second request at a
//!        closed port to show a transport failure. No log facility is bound, so this sample prints
//!        to the console, and it reaches the same failure path by way of whatever URL the caller
//!        passes.
//!
//! @date
//!   Sep-06, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Collections.Generic;
using WapitiStew.Wse;

/// <summary>One HTTP GET through the XPT HTTP client.</summary>
public static class HttpGetExample
{
    /// <summary>Response-body ceiling in bytes, matching the C++ default of 8 MiB.</summary>
    // The ceiling is a required argument because nothing else bounds how much a server may send.
    // A body over it is refused as ResponseTooLarge before it is buffered, so a smaller value is
    // the way to protect a memory-constrained caller from a hostile or runaway server; a larger
    // one only raises the amount that may be held at once.
    private const int MaximumResponseBodySize = 8 * 1024 * 1024;

    /// <summary>How many header fields are printed before the listing is cut short.</summary>
    // Purely a display bound. Headers() has already read every field, so raising this shows more
    // of them without another exchange.
    private const int HeaderPreviewCount = 5;

    /// <summary>Performs one GET and reports the status, attempts, headers, and body size.</summary>
    /// <param name="arguments">
    /// arguments[0] is the optional path to the native library file and arguments[1] is the URL.
    /// </param>
    /// <returns>Zero on success and when no URL was given or the server was unreachable.</returns>
    public static int Main(string[] arguments)
    {
        if (arguments.Length > 0)
        {
            WseRuntime.SetNativeLibraryPath(arguments[0]);
        }

        using var runtime = new WseRuntime();
        if (!runtime.Info().Components.HasXpt)
        {
            Console.Error.WriteLine("This WSE build does not include the XPT component.");
            return 1;
        }

        // Naming no URL is not an error. The sample has to run on a build machine with no network,
        // so it explains what it wanted and returns success.
        if (arguments.Length < 2)
        {
            Console.WriteLine(
                "No URL was given. Pass the native library path, then an absolute URL, for example:"
                + " HttpGetExample <library> http://example.com/");
            return 0;
        }

        // XPT has no default timeout, so the request carries its own deadline.
        // Five seconds has to cover name resolution, the connection, and the whole body, which is
        // why it is longer than the one-second deadlines the socket samples use. It bounds the
        // exchange as a whole, not each byte, so a large download over a slow link wants more.
        var context = new OperationContext(TimeSpan.FromSeconds(5));
        string url = arguments[1];

        // The request owns a native handle and the using releases it. The native side copies any
        // header or body given to it, so nothing here has to outlive the Execute call.
        using var request = new HttpRequest(HttpMethod.Get, url);
        try
        {
            // The response is a second native object, owned by the caller from here on; the using
            // releases it once the fields below have been copied out.
            using HttpResponse response =
                HttpClient.Execute(request, MaximumResponseBodySize, context);

            // Headers and Body each copy out of native memory, so both survive the response. The
            // attempt count printed below is always one: the binding treats a request as
            // non-idempotent and never retries a transient failure on the caller's behalf.
            IReadOnlyList<HttpHeader> headers = response.Headers();
            byte[] body = response.Body();
            Console.WriteLine(
                $"GET {url}: status={response.StatusCode} attempts={response.AttemptCount}"
                + $" headers={headers.Count} body_bytes={body.Length}");

            int shown = Math.Min(headers.Count, HeaderPreviewCount);
            for (int index = 0; index < shown; ++index)
            {
                Console.WriteLine($"  header: {headers[index].Name}: {headers[index].Value}");
            }
            if (headers.Count > shown)
            {
                Console.WriteLine($"  ... {headers.Count - shown} further header fields");
            }
            return 0;
        }
        catch (HttpStatusException failure)
        {
            // The server answered with a 4xx or 5xx. The exchange finished, so the response came
            // back with the exception and the status code is still readable.
            // The exception owns that response, and nothing else will release it, so the using is
            // what frees the native memory once the fields below have been read. This is the only
            // failure in the binding that hands anything back besides the error itself.
            using HttpResponse answered = failure.Response;
            Console.WriteLine(
                $"GET {url} answered an error status: status={answered.StatusCode}"
                + $" attempts={answered.AttemptCount} headers={answered.Headers().Count}"
                + $" body_bytes={answered.Body().Length}");
            Console.WriteLine(
                $"  reported as category={failure.Category}"
                + $" code={(TransportErrorCode)failure.Code} native={failure.NativeCode}");
            return 0;
        }
        catch (WseException failure)
        {
            // A transport problem: the exchange never produced a response, so there is nothing to
            // read beyond the structured error itself.
            // HttpStatusException derives from WseException, so its catch has to come first; this
            // one collects name resolution, connection, TLS, timeout, and an oversized body. All
            // of them return zero, which is what lets the sample run without a network.
            Console.WriteLine(
                $"GET {url} failed: category={failure.Category}"
                + $" code={(TransportErrorCode)failure.Code} native={failure.NativeCode}"
                + $" message={failure.Message}");
            Console.WriteLine("branch on the category and the code, not on the message text");
            return 0;
        }
    }
}

//*****************************************************************************************************************
//!
//! @file    QuickStart.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese 実機不要のWSE C# Quick start.
//! @brief   \~english  Hardware-free WSE C# quick start.
//!
//! @details
//!     \~japanese
//!     @n 引数順序は arguments[0] がNative Library Pathである。省略時はManaged Assembly隣を探索する。
//!     @n Native Handleを持つのは WseRuntime だけである。`using`がDisposeを呼び、Handleを解放する。
//!     \~english
//!     @n Argument order: arguments[0] is the native library path. Without it the binding looks for
//!        the library beside the managed assembly, so the sample runs with no argument at all.
//!     @n WseRuntime is the only native owner here. The `using` releases its handle; everything
//!        else on this page is managed data copied out of the runtime.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Text.RegularExpressions;
using WapitiStew.Wse;

/// <summary>Hardware-free WSE C# quick start.</summary>
public static class QuickStart
{
    /// <summary>Prints the runtime information reported by the native library.</summary>
    /// <param name="arguments">Optional path to the native library file.</param>
    /// <returns>Zero on success.</returns>
    public static int Main(string[] arguments)
    {
        // The path has to be named before the first native call, because the resolved library
        // handle is cached for the life of the process. Naming nothing is not an error: the
        // resolver then falls back to the assembly directory and the usual platform search.
        if (arguments.Length > 0)
        {
            WseRuntime.SetNativeLibraryPath(arguments[0]);
        }

        // Constructing the runtime creates the native object; the using releases it exactly once
        // through its SafeHandle, whichever way this method leaves.
        using var runtime = new WseRuntime();
        // Info() copies the version string and the component flags out of native memory, so the
        // result stays valid after the runtime is disposed.
        RuntimeInfo info = runtime.Info();
        // The binding promises a MAJOR.MINOR.PATCH version. Checking it here turns a broken or
        // mismatched native library into a loud failure instead of a confusing print.
        if (!Regex.IsMatch(info.Version, @"^\d+\.\d+\.\d+$"))
        {
            throw new InvalidOperationException("WSE returned an invalid semantic version");
        }

        Console.WriteLine(info);
        return 0;
    }
}

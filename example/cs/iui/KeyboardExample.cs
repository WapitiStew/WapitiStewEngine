//*****************************************************************************************************************
//!
//! @file    KeyboardExample.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese IUI Keyboardの読取可否確認と一貫したKey Snapshot取得.
//! @brief   \~english  IUI keyboard readiness check and one consistent key snapshot.
//!
//! @details
//!     \~japanese
//!     @n 実機不要である。読取可能なKeyboardが無い環境では、失敗ではなく読取可否Stateとして報告する。
//!     @n 本BindingはPollingのみを公開する。Key EventのCallback面は無い。
//!     @n 引数順序は arguments[0] がNative Library Pathである。追加引数は取らない。
//!     \~english
//!     @n Hardware-free: a machine without a readable keyboard reports that through the readiness
//!        state instead of failing.
//!     @n The binding exposes polling only; there is no key-event callback surface to subscribe
//!        to. Ask for a snapshot whenever the caller's own loop wants one.
//!     @n Argument order: arguments[0] is the native library path. The sample takes no further
//!        arguments.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Threading;
using WapitiStew.Wse;

/// <summary>IUI keyboard readiness check and one consistent key snapshot.</summary>
public static class KeyboardExample
{
    /// <summary>Prints the keyboard readiness state and one snapshot.</summary>
    /// <param name="arguments">Optional path to the native library file.</param>
    /// <returns>Zero on success.</returns>
    public static int Main(string[] arguments)
    {
        if (arguments.Length > 0)
        {
            WseRuntime.SetNativeLibraryPath(arguments[0]);
        }

        using var runtime = new WseRuntime();
        // A native library built without IUI cannot answer at all, so this is a build mismatch and
        // returns non-zero. Missing keyboard hardware, below, is a different thing and returns 0.
        if (!runtime.Info().Components.HasIui)
        {
            Console.Error.WriteLine("This WSE build does not include the IUI component.");
            return 1;
        }

        // The keyboard is the single RAII owner; disposing it stops the monitoring thread.
        // Construction alone starts that thread, so the instance is created as late as possible.
        using var keyboard = new Keyboard();

        // The backend needs a moment before it reports its first reading.
        // Twenty attempts of 25 ms cap the wait at half a second, which is long enough for the
        // monitoring thread to leave Starting on a slow machine and short enough that a machine
        // with no keyboard at all does not stall the sample. A larger product only lengthens the
        // wait on a machine that will never become ready.
        for (int attempt = 0; attempt < 20 && !keyboard.IsAvailable; ++attempt)
        {
            Thread.Sleep(25);
        }

        Console.WriteLine($"keyboard state: {keyboard.AccessState}");

        if (!keyboard.IsAvailable)
        {
            // Not a failure of this sample: Snapshot() explains why the keyboard is unreadable.
            // AccessState always reads, even when unreadable; Snapshot() is the call that carries
            // the reason as a structured category, which is why it is provoked deliberately here.
            // The sample still returns 0 so it can run on a headless or unprivileged machine.
            try
            {
                keyboard.Snapshot();
            }
            catch (WseException failure)
            {
                Console.WriteLine($"keyboard is not readable: {failure.Message}");
            }
            return 0;
        }

        // One snapshot is one consistent point in time; do not combine several separate reads.
        // The five groups below all come from this one snapshot, so a chord such as Shift+F1 can
        // never be seen half-pressed. The arrays are managed copies and hold no native memory.
        KeyboardState snapshot = keyboard.Snapshot();
        Console.WriteLine($"pressed ascii keys: {CountPressed(snapshot.Ascii)}");
        Console.WriteLine($"pressed function keys: {CountPressed(snapshot.Function)}");
        Console.WriteLine($"pressed arrow keys: {CountPressed(snapshot.Arrow)}");
        Console.WriteLine($"pressed lock keys: {CountPressed(snapshot.Lock)}");
        Console.WriteLine($"pressed command keys: {CountPressed(snapshot.Command)}");
        // A separate read, and deliberately so: PressedAscii is a convenience that reports zero for
        // "nothing pressed" and, with several ASCII keys down, resolves to the smaller code.
        Console.WriteLine($"pressed ascii code: {keyboard.PressedAscii()}");
        return 0;
    }

    private static int CountPressed(bool[] group)
    {
        int pressed = 0;
        foreach (bool state in group)
        {
            if (state) ++pressed;
        }
        return pressed;
    }
}

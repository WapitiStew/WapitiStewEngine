//*****************************************************************************************************************
//!
//! @file    Keyboard.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese IUI Keyboardの公開Owner.
//! @brief   \~english  Public owner of the IUI keyboard.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Runtime.InteropServices;

namespace WapitiStew.Wse;

/// <summary>Readiness state of the keyboard backend.</summary>
public enum KeyboardAccessState
{
    /// <summary>The backend is still starting and has produced no reading yet.</summary>
    Starting = 0,

    /// <summary>Readable sources on Linux; enabled polling on Windows, without a physical-device probe.</summary>
    Ready = 1,

    /// <summary>No readable keyboard source exists on this machine.</summary>
    Unavailable = 2,

    /// <summary>The operating system refused to let this process read the keyboard.</summary>
    PermissionDenied = 3,

    /// <summary>The keyboard source was disconnected after it had been readable.</summary>
    Disconnected = 4,
}

/// <summary>Native layout of one keyboard snapshot. Sizes match the IUI metadata.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeKeyboardState
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = KeyboardState.AsciiCount)]
    public int[] Ascii;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = KeyboardState.FunctionCount)]
    public int[] Function;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = KeyboardState.ArrowCount)]
    public int[] Arrow;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = KeyboardState.LockCount)]
    public int[] Lock;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = KeyboardState.CommandCount)]
    public int[] Command;
}

/// <summary>
/// A consistent keyboard-input snapshot taken at one update point. True means pressed.
/// Index meanings follow <c>iui/depend/Metadata.h</c>.
/// </summary>
public sealed class KeyboardState
{
    /// <summary>Number of ASCII key slots.</summary>
    public const int AsciiCount = 128;

    /// <summary>Number of function key slots, F1 through F24.</summary>
    public const int FunctionCount = 24;

    /// <summary>Number of arrow key slots: left, up, down, right.</summary>
    public const int ArrowCount = 4;

    /// <summary>Number of lock key slots: CapsLock, NumLock, ScrollLock.</summary>
    public const int LockCount = 3;

    /// <summary>Number of command key slots, such as Space, Enter, and Tab.</summary>
    public const int CommandCount = 9;

    internal KeyboardState(NativeKeyboardState native)
    {
        Ascii = ToBooleans(native.Ascii, AsciiCount);
        Function = ToBooleans(native.Function, FunctionCount);
        Arrow = ToBooleans(native.Arrow, ArrowCount);
        Lock = ToBooleans(native.Lock, LockCount);
        Command = ToBooleans(native.Command, CommandCount);
    }

    /// <summary>Gets the ASCII key states, indexed by ASCII code.</summary>
    public bool[] Ascii { get; }

    /// <summary>Gets the function key states, F1 first.</summary>
    public bool[] Function { get; }

    /// <summary>Gets the arrow key states in left, up, down, right order.</summary>
    public bool[] Arrow { get; }

    /// <summary>Gets the lock key states in CapsLock, NumLock, ScrollLock order.</summary>
    public bool[] Lock { get; }

    /// <summary>Gets the command key states.</summary>
    public bool[] Command { get; }

    /// <summary>Gets a value indicating whether no key in this snapshot is pressed.</summary>
    public bool IsIdle =>
        !HasAny(Ascii) && !HasAny(Function) && !HasAny(Arrow) && !HasAny(Lock) && !HasAny(Command);

    private static bool HasAny(bool[] states)
    {
        foreach (bool state in states)
        {
            if (state)
            {
                return true;
            }
        }
        return false;
    }

    private static bool[] ToBooleans(int[]? source, int count)
    {
        bool[] result = new bool[count];
        if (source is null)
        {
            return result;
        }
        for (int index = 0; index < count && index < source.Length; ++index)
        {
            result[index] = source[index] != 0;
        }
        return result;
    }
}

/// <summary>Owns the native keyboard handle and releases it exactly once.</summary>
internal sealed class KeyboardHandle : SafeHandle
{
    internal KeyboardHandle()
        : base(IntPtr.Zero, ownsHandle: true)
    {
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    internal void Attach(IntPtr value) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        NativeMethods.wse_capi_keyboard_destroy(handle);
        return true;
    }
}

/// <summary>
/// Reads physical keyboard input through IUI. Construction starts a monitoring thread, so create
/// one instance and dispose it when finished.
/// </summary>
/// <remarks>
/// The C# binding exposes polling only. Read <see cref="Snapshot"/> for one consistent point in
/// time rather than combining several independent reads.
/// </remarks>
public sealed class Keyboard : IDisposable
{
    private readonly KeyboardHandle _handle;

    /// <summary>Creates a keyboard and starts monitoring.</summary>
    /// <exception cref="WseException">The native keyboard could not be created.</exception>
    public Keyboard()
    {
        using var output = new NativeOutput(NativeMethods.wse_capi_keyboard_destroy);
        NativeMethods.ThrowIfFailed(NativeMethods.wse_capi_keyboard_create(out output.Value));
        _handle = new KeyboardHandle();
        _handle.Attach(output.Take());
    }

    /// <summary>
    /// Gets the current backend readiness state. Reading the state succeeds even when the keyboard
    /// is not readable.
    /// </summary>
    /// <exception cref="ObjectDisposedException">The keyboard was already disposed.</exception>
    public KeyboardAccessState AccessState
    {
        get
        {
            ThrowIfDisposed();
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_keyboard_access_state(
                    _handle, out int state));
            return (KeyboardAccessState)state;
        }
    }

    /// <summary>Gets whether the backend is Ready. Windows polling readiness does not prove physical presence.</summary>
    /// <exception cref="ObjectDisposedException">The keyboard was already disposed.</exception>
    public bool IsAvailable
    {
        get
        {
            ThrowIfDisposed();
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_keyboard_is_available(
                    _handle, out int available));
            return available != 0;
        }
    }

    /// <summary>Reads every key group from one update point.</summary>
    /// <returns>A consistent snapshot of the current key states.</returns>
    /// <exception cref="ObjectDisposedException">The keyboard was already disposed.</exception>
    /// <exception cref="WseException">
    /// The keyboard is not readable. <see cref="WseException.Category"/> distinguishes a backend
    /// that is still starting, unavailable, refused, or disconnected.
    /// </exception>
    public KeyboardState Snapshot()
    {
        ThrowIfDisposed();
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_keyboard_snapshot(
                _handle, out NativeKeyboardState native));
        return new KeyboardState(native);
    }

    /// <summary>
    /// Reads one pressed ASCII code. Multiple simultaneous input is unsupported and the smaller
    /// code wins.
    /// </summary>
    /// <returns>The pressed ASCII code, or zero when no ASCII key is pressed.</returns>
    /// <exception cref="ObjectDisposedException">The keyboard was already disposed.</exception>
    /// <exception cref="WseException">The keyboard is not readable.</exception>
    public sbyte PressedAscii()
    {
        ThrowIfDisposed();
        NativeMethods.ThrowIfFailed(
            NativeMethods.wse_capi_keyboard_pressed_ascii(
                _handle, out sbyte ascii));
        return ascii;
    }

    /// <summary>Releases the native keyboard and stops monitoring.</summary>
    public void Dispose() => _handle.Dispose();

    private void ThrowIfDisposed()
    {
        if (_handle.IsClosed || _handle.IsInvalid)
        {
            throw new ObjectDisposedException(nameof(Keyboard));
        }
    }
}

# WSE Coding Rule

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Scope

This rule governs the C++ and C source WSE owns: `api/`, `core/`, `platform/`, `lang/*/native/`,
`test/`, and `example/cpp/`. Vendored code under `vendor/`, third-party headers, and generated code
are outside it and are not edited to conform.

Each binding's managed source follows its own language's conventions: `lang/cs/` is C#, `lang/java/`
is Java, `lang/js/` is JavaScript, and `lang/python/wse/` is Python. The rules below that describe
direction, ownership, and documentation still apply to those layers in spirit; the spelling rules do
not.

## Layout

- Files are UTF-8 without a byte-order mark. Lines end with LF.
- Indentation is four spaces. Tabs never indent.
- Braces are Allman: the opening brace sits on its own line.
- A pointer or reference token binds to the type: `const char* text`, `sCameraFrame& frame`.
- A call puts a space inside its parentheses: `readFrame( timeout_ms_in )`.
- A header carries both `#pragma once` and a macro guard. The guard is `UPPER_SNAKE_CASE` and
  includes enough of the project and path to be unique.
- A header never contains `using namespace`.

```cpp
if ( frame_in.description.width == 0U )
{
    return CameraStatus::failure( error );
}
```

## Files and namespaces

- A file whose main responsibility is one class or struct holds that one type and takes its name.
- A C++ header is `.h` and its source is `.cpp`.
- Public declarations live in `wse` or `wse::<component>`. A file does not invent an unrelated
  namespace of its own.

## Naming

| Subject | Rule | Example |
|---|---|---|
| Class | `PascalCase` | `WebCamera` |
| Struct | `s` + `PascalCase` | `sCameraFrame` |
| Enumeration type | `e` + `PascalCase` | `eCameraPixelFormat` |
| Enumerator | `PascalCase` | `Rgb8` |
| Function and method | `lowerCamelCase`, opening with a verb | `readAveragedFrame()` |
| Simple getter on a data class | `snake_case` without the member prefix | `native_format()` |
| Local variable | `snake_case` | `frame_index` |
| Parameter | `snake_case` plus a direction suffix | `timeout_ms_in` |
| Out or in-out pointer parameter | `p_` + `snake_case` + suffix | `p_frame_out` |
| Global variable | `g_` + `snake_case` | `g_state_mutex` |
| Pointer variable | `p_` + `snake_case` | `p_buffer` |
| Class member | `m_` + `snake_case` | `m_impl` |
| Pointer class member | `m_ptr_` + `snake_case` | `m_ptr_data` |
| Constant | `UPPER_SNAKE_CASE` | `READ_TIMEOUT_MS` |
| Macro | `UPPER_SNAKE_CASE` with a `WSE_` prefix | `WSE_CAPI_ABI_VERSION` |
| Namespace | lower case | `wse::tmr` |

A function that does something opens with a verb that says what: `updateState()`, not
`stateUpdate()`, `stateChecker()`, or `doWork()`.

The one exception is a simple getter on a data class, meaning a class whose responsibility is to
hold and carry values rather than to perform I/O, drive a state machine, own a resource, or run
domain logic. Its getter drops the `m_` and reads as the member: `device_id()`. Any other class uses
the verb form, `getDeviceId()` or `isConnected()`. A setter is always a verb, on every kind of class.

## Function parameters

A parameter's name says what it is for, its position says how urgent it is to the reader, and its
type says who may write to it. The three agree, so a call site can be read without opening the
declaration.

- Every parameter carries a direction suffix: `_in` for input, `_out` for output, `_inout` for a
  parameter that is read and then written. A pointer parameter also carries a `p_` prefix, so the
  name states the passing kind as well as the direction.
- Parameters are ordered `[in,out]`, then `[out]`, then `[in]`. A member function's `this` is the
  implicit in-out, so its explicit parameters run `[out]` then `[in]`. The Doxygen `@param` lines
  follow the same order.
- An `_out` or `_inout` parameter is a pointer. The implementation aliases it to a reference on its
  first line, so the body reads as a reference while the call site still shows, at the call, that
  the argument is written.

  ```cpp
  bool coreFormatOf( wse::ePixFormat* p_format_out, const eCameraPixelFormat format_in ) noexcept
  {
      wse::ePixFormat& format_out = *p_format_out;
      ...
  ```

- An `_in` parameter is `const`, whether it passes by value, by pointer, or by reference. A
  non-trivial type passes as a `const` reference. An arithmetic type, an enumeration, a pointer, and
  an opaque handle such as `sTextureHandle` stay `const` by value, because a reference to them costs
  more than it saves.
- An `_out` or `_inout` parameter is the thing being written, so the destination itself is not
  `const`.
- When directions mix across several lines, the type column aligns on the `[in]` types. `[in]`
  carries `const`, so `[in,out]` and `[out]` reserve the width `const ` would occupy.

Four exceptions, all forced from outside rather than chosen:

- A move constructor and a move assignment take an rvalue reference and name it `X&& other_inout`.
- A flat C ABI function in `api/wse/capi/` cannot take a reference, so its out parameters stay
  pointers. It follows the ordering and the suffix rules like every other function.
- A function whose address is handed to a foreign callback typedef keeps that typedef's parameter
  list exactly, because the caller is the one that decides the order. Its parameters still carry
  direction suffixes, and it carries a comment saying why the shape is fixed.
- A `swap` overload keeps its two operands as references, because the standard library decides that
  shape: an unqualified `swap( a, b )` after `using std::swap` only finds an overload that takes
  references. The names still say the direction, so the signature reads
  `swap( X& obj1_inout, X& obj2_inout )`.

## Types and values

- A value that will not change is `const`.
- New C++ code uses `nullptr`. C code uses `NULL`.
- A cast is a C++ cast. C-style casts do not appear in new code.
- A fixed-width type comes from `<cstdint>`. Among the WSE aliases, prefer the `float64_*` spelling
  over `double_*`.
- A value carrying a unit says so in its name or in its comment: `timeout_ms`, `interval_us`.
- A value with meaning — a protocol constant, a port, a timeout, a control value — is a named
  constant. A zero, a one, or a plain index does not need one.

## Classes

- A single-argument or converting constructor is `explicit` unless the implicit conversion is
  intended.
- A move operation that cannot throw is `noexcept`.
- Rule of Zero comes first. A type that owns a resource directly considers Rule of Five.
- `final` marks a type that is not meant to be derived from, where that makes the intent clearer.

## Member initialization

- Initialize non-static data members in constructor initializer lists, never with an in-class `= value` or `{ value }` initializer. This applies to owned classes, structs, tests, examples, and native binding implementations.
- List members in declaration order, one per line; use `:` for the first and a leading comma thereafter.
- Pad member names with spaces so the opening `(` aligns within one initializer list, with at least one space after the longest name. Built-in arrays that require braces under C++17 align `{` in the same column.
- Use initialization, not assignment in the constructor body. Preserve copy/move behavior, constant-expression use, brace-call compatibility, and binary layout. Defaulted copy/move operations remain valid.
- Static constants, `static constexpr`, C ABI structs compiled as C, vendor sources, and generated code are outside this rule.

```cpp
Example::Example()
    : m_state      ( State::Created )
    , m_log_sink   ( nullptr )
{
}
```

## Alignment

Within one logical block, line up what the reader compares.

- Declarations align the name, the `=`, the value, and the trailing `//!<` comment.
- Struct and class members align the member name and the trailing comment.
- An enumeration puts one enumerator per line and aligns the identifier, the `=`, the value, and the
  comment. From the second entry on, the comma leads the line. A leading comma before the first
  entry is not valid C++ and is not used.

```cpp
enum class eCameraBackend : std::uint8_t
{
      Automatic = 0U  //!< The backend the platform selects.
    , MediaFoundation  //!< Windows Media Foundation.
};
```

Align within a block that means one thing. Do not pad a whole file, and do not insert a wall of
spaces to force alignment that the code does not want.

## Calls and aggregates

A call takes one argument per line when it has four or more arguments, when several arguments share
a type and could be transposed, when it carries several numeric literals, or when it configures a
network, a device, or a protocol. Each line gets a short `//` comment saying what it sets, and the
comments align.

A table or descriptor uses the leading-comma form, with a comment on each entry. A short fixed array
stays on one line.

## Documentation comments

- Every file WSE owns opens with a Doxygen file header carrying `@file` and a `@brief` in both
  Japanese and English, under `\~japanese` and `\~english`.
- A declaration in a header carries the detailed Doxygen. The definition in the source does not
  repeat it.
- A function that has no header carries its detailed Doxygen on the definition.
- A `void` function has no `@return`.
- A simple file-scope or namespace-scope variable or constant documents itself with a trailing
  `//!<`. A complex table takes a Doxygen comment above it.

## Ordinary comments

- The steps inside a function, the purpose of a block, a threading or network constraint, and a
  safety note are ordinary `//` comments, not Doxygen.
- A function whose body runs beyond roughly twenty lines carries a comment on each block of work.
  Blank lines, braces, and Doxygen do not count toward the twenty.
- A comment says why, or what a block achieves. It does not restate the line beneath it.
- A source file beyond roughly five hundred lines separates its responsibilities with section
  comments. A small file does not need them.
- A comment ends with an ASCII `.`. One file does not mix `.` and `。`.

## Function length and responsibility

Length alone does not force a split, but a body beyond roughly twenty lines carries block comments,
and one beyond roughly a hundred lines is a candidate for division. A function with two
responsibilities is a candidate whatever its length. Deep nesting simplifies through early return.

## Errors

- A component's own result and error contract carries the failure. Errors are not flattened into a
  single return convention across components.
- Division by zero, a range, and an invalid argument are checked at the boundary that receives them.
- An error is returned to the caller or recorded through the defined path. It is never swallowed.

## Compiler warnings

A warning is fixed in the implementation. Where a suppression cannot be avoided, it covers the
smallest possible scope and carries a comment giving the reason.

Core CI keeps GCC warning-clean with `WSE_WARNINGS_AS_ERRORS`; the MSVC sweep retains a reviewed
per-file baseline for existing DLL-interface diagnostics. The warning parser counts diagnostics
with and without column numbers, including Windows drive-letter paths. A baseline may only fall;
new files start at zero. Warning flags remain private to the native library target. Optional
component sweeps are separate from this Core guarantee. See the [build guide](../../en/BuildGuide.md).

## Review checklist

- [ ] Function names open with a verb, and only a data class's simple getters are the exception.
- [ ] `sPascalCase` structs, `ePascalCase` enumerations, `UPPER_SNAKE_CASE` constants.
- [ ] `g_` globals, `m_` members, `m_ptr_` pointer members.
- [ ] Every parameter carries `_in`, `_out`, or `_inout`, and every out or in-out pointer carries `p_`.
- [ ] Parameters run `[in,out]`, `[out]`, `[in]`, and the `@param` lines match.
- [ ] Every `[in]` is `const`; every out and in-out is a pointer aliased on the first line.
- [ ] Mixed-direction parameter lists align their type column on the `[in]` types.
- [ ] Declarations, members, and enumerators align within their block.
- [ ] A many-argument call is one argument per line with an aligned comment on each.
- [ ] Header and source do not repeat the same detailed Doxygen, and a `void` function has no `@return`.
- [ ] A function beyond twenty lines carries block comments, written as ordinary comments.
- [ ] Allman braces, four spaces, UTF-8 without a byte-order mark, LF.
- [ ] Units are stated, and values with meaning are named constants.
- [ ] Warnings are fixed rather than suppressed.
- [ ] Vendored and generated code is left alone.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |

//*****************************************************************************************************************
//! 
//! @file    Metadata.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//! @brief   
//!     \~japanese iui名前空間内の定数や列挙子を定義する.
//!     \~english  Defines constants and enumerators within the iui namespace.
//!
//! @details  
//!
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#ifndef WANDERSTEWENGINE_INPUTUSERINTERFASE_DEPEND_METADATA_H
#define WANDERSTEWENGINE_INPUTUSERINTERFASE_DEPEND_METADATA_H

#include <cstdint>
#include <string>

#pragma warning(push)
#pragma warning(disable: 4505)  // ../
#pragma warning(disable: 4514)  // ../

namespace wse
{
namespace iui
{

    static constexpr uint8_t ALPHABET_NUM = 26; //!< 'A' to 'Z'.
    static constexpr uint8_t NUMBER_NUM   = 10; //!< '0' to '9'.
    static constexpr uint8_t SYMBOLN_NUM  = 59; //!< ' ' '@'... .
    static constexpr uint8_t FANCTION_NUM = 24; //!< 'F1' to 'F24'.
    static constexpr uint8_t ARROW_NUM    =  4; //!< '←', '↑', '↓', '→'.
    static constexpr uint8_t LOCK_NUM     =  3; //!< 'CapsLock', 'NumLock', 'ScrollLock'
    static constexpr uint8_t COMMAND_NUM  =  9; //!< 'Space', 'Enter', 'Delete', 'Esc', 'Tab', 'Shift' ...
    static constexpr uint8_t ASCII_NUM    =128; //!< 'Space', 'Enter', 'Delete', 'Esc', 'Tab', 'Shift' ...

    
    static constexpr uint8_t ARROW_LEFT    =  0; //!< '←', '↑', '↓', '→'.
    static constexpr uint8_t ARROW_TOP     =  1; //!< '←', '↑', '↓', '→'.
    static constexpr uint8_t ARROW_LOW     =  2; //!< '←', '↑', '↓', '→'.
    static constexpr uint8_t ARROW_RIGHT   =  3; //!< '←', '↑', '↓', '→'.

    static constexpr uint8_t LOCK_CAPS     = 0;  //!< CapsLock.
    static constexpr uint8_t LOCK_NUMLK    = 1;  //!< NumLk.
    static constexpr uint8_t LOCK_SCROLL   = 2;  //!< Scroll Lock.

    static constexpr uint8_t COM_SPACE     = 0;  //!< Index of Space key.
    static constexpr uint8_t COM_ENTER     = 1;  //!< Index of Enter key.
    static constexpr uint8_t COM_DELETE    = 2;  //!< Index of Delete key.
    static constexpr uint8_t COM_ESC       = 3;  //!< Index of Esc key.
    static constexpr uint8_t COM_TAB       = 4;  //!< Index of Tab key.
    static constexpr uint8_t COM_SHIFT     = 5;  //!< Index of Shift key.
    static constexpr uint8_t COM_CONTROLL  = 6;  //!< Index of Controll key.
    static constexpr uint8_t COM_BACKSPACE = 7;  //!< Index of Backspace key.
    static constexpr uint8_t COM_MENU      = 8;  //!< Index of Menu key.

    
    static constexpr uint8_t ASCII_NULL    =   0;   //!< ASCII code Number: NULL
    static constexpr uint8_t ASCII_BACK    =   8;   //!< ASCII code Number: Back
    static constexpr uint8_t ASCII_TAB     =   9;   //!< ASCII code Number: Tab
    static constexpr uint8_t ASCII_CR      =  10;   //!< ASCII code Number: CR
    static constexpr uint8_t ASCII_LF      =  13;   //!< ASCII code Number: LF
    static constexpr uint8_t ASCII_ESC     =  27;   //!< ASCII code Number: ESC
    static constexpr uint8_t ASCII_DEL     = 127;   //!< ASCII code Number: DEL


};
};
#pragma warning(pop)

#endif //WONDERSTEWENGINE_TURTLECAMERALIB_DEPEND_METADATA_H

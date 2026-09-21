//*****************************************************************************************************************
//! 
//! @file    Keyboard.h
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New
//!   Aug-30, 2026   Add portable backend availability state.
//!
//! 
//! @brief
//!     \~japanese キーボードの入力イベントを受け取るクラス.
//!     \~english  Class that behaves as a keyboard device.
//! 
//! @details  
//!
//! 
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
#ifndef WANDERSTEWENGINE_INPUTUSERINTERFASE_DEVICE_KEYBOARD_H
#define WANDERSTEWENGINE_INPUTUSERINTERFASE_DEVICE_KEYBOARD_H

#include <atomic>
#include <functional>
#include <memory>

#include "../../dynamic.h"
#include "../../wse/stew.h"
#include "../depend/Metadata.h"

namespace  wse
{
namespace  iui
{
    //! \~japanese Keyboard Backendの読取可否State.
    //! \~english  Readiness state of the keyboard backend.
    enum class KeyboardAccessState : uint8_t
    {
          Starting         = 0U
        , Ready            = 1U
        , Unavailable      = 2U
        , PermissionDenied = 3U
        , Disconnected     = 4U
    };

    //! \~japanese 1回の更新で一貫したKeyboard入力Snapshot.
    //! \~english  Consistent keyboard-input snapshot from one update.
    struct KeyboardState
    {
        std::array< bool, ASCII_NUM    > ascii;
        std::array< bool, FANCTION_NUM > function;
        std::array< bool, ARROW_NUM    > arrow;
        std::array< bool, LOCK_NUM     > lock;
        std::array< bool, COMMAND_NUM  > command;
    };

    //! 
    //! @class  Keyboard
    //! 
    //! @brief 
    //!     \~japanese  キーボードデバイスとして振る舞うクラス.
    //!     \~english   Class that behaves as a keyboard device..
    //! 
    //! @details
    //!  \~japanese
    //!     オブジェクト生成時にスレッドを生成して,常時キーステートを監視する.
    //!  @n キーステートが更新されたらメンバー変数を変更する.
    //!  @n ユーザーはメンバー変数へのgetter関数からキーステートを取得する.
    //!  @n
    //!  @n 取り扱うキー::
    //!  @n  ASCIIコード       ( 0-9, a-z, A-Z, !,",#, $...etc )
    //!  @n  ファンクションキー( F1-F9 )
    //!  @n  矢印キー          ( <-,↑, ↓, -> )
    //!  @n  ロックキー        ( NumLock, Scroll Lock, .. etc )
    //!  @n  コマンドキー      ( TAB, Space, Enter, ... etc )
    //!  @n  詳細や配列に配置されているインデックスは以下のファイルを参照すること.
    //!  @n     "iui/depend/Metadata.h"
    //! 
    //!  \~english
    //!     A thread is created when the object is instantiated, and it constantly monitors the key state.
    //!  @n When the key state is updated, the member variables are changed.
    //!  @n The user obtains the key state from the getter functions for the member variables.
    //!  @n
    //!  @n Supported keys:
    //!  @n  ASCII codes       (0-9, a-z, A-Z, !,",#, $...etc)
    //!  @n  Function keys     (F1-F9)
    //!  @n  Arrow keys        (<-, ↑, ↓, ->)
    //!  @n  Lock keys         (NumLock, Scroll Lock, .. etc)
    //!  @n  Command keys      (TAB, Space, Enter, ... etc)
    //!  @n  For details and array indices, refer to the following file:
    //!  @n     "iui/depend/Metadata.h"
    //! 
    class WSE_API Keyboard final
    {
        public : using KeyCallback = std::function<void( const KeyboardState& )>;

        //-----------------------------------
        // Specific Constant
        //-----------------------------------

        //-----------------------------------
        // Member
        //-----------------------------------
        private : class Impl;
        private : static void deleteImpl( Impl* p_in );
        private : using ImplPtr = std::unique_ptr< Impl, void (*)( Impl* ) >;
        private : ImplPtr m_impl;                                        //!< \~japanese Worker所有の内部実装. \~english Internal implementation owning the worker.

        private : std::array< bool, ASCII_NUM    > m_ascii_state    ;    //!< Input state array of ASCII key.
        private : std::array< bool, FANCTION_NUM > m_function_state ;    //!< Input state array of Function key.
        private : std::array< bool, ARROW_NUM    > m_arrow_state    ;    //!< Input state array of Arrow key.
        private : std::array< bool, LOCK_NUM     > m_lock_state     ;    //!< Input state array of Lock key.
        private : std::array< bool, COMMAND_NUM  > m_command_state  ;    //!< Input state array of Commamd key.

        private : std::shared_ptr<const KeyCallback> m_key_callback;
        private : std::atomic< KeyboardAccessState > m_access_state;

        //----------------------------------------------------------------------
        // Setter/ Getter
        //----------------------------------------------------------------------

        //! 
        //! @brief Get member valiable .
        //! @return Input state array of ASCII Code.
        //! @retval true  press
        //! @retval false released
        //! 
        public : std::array< bool, ASCII_NUM    > ascii_state   ( void ) const;

        //! 
        //! @brief Get member valiable .
        //! @return Input state array of ASCII Code key.
        //! @retval true  press
        //! @retval false released
        //! 
        public : std::array< bool, FANCTION_NUM > function_state( void ) const;

        //! 
        //! @brief Get member valiable .
        //! @return Input state array of Function Key.
        //! @retval true  press
        //! @retval false released
        //! 
        public : std::array< bool, ARROW_NUM    > arrow_state   ( void ) const;

        //! 
        //! @brief Get member valiable .
        //! @return Input state array of Lock key.
        //! @retval true  press
        //! @retval false released
        //! 
        public : std::array< bool, LOCK_NUM     > lock_state    ( void ) const;

        //! 
        //! @brief Get member valiable .
        //! @return Input state array of Command Key.
        //! @retval true  press
        //! @retval false released
        //! 
        public : std::array< bool, COMMAND_NUM  > command_state ( void ) const;

        //! \~japanese 全Key groupを同一更新時点で取得する.
        //! \~english  Returns all key groups from one update point.
        public : KeyboardState snapshot( void ) const;

        //! \~japanese Backendの現在の読取可否Stateを返す.
        //! \~english  Returns the current backend readiness state.
        public : KeyboardAccessState accessState( void ) const noexcept
        {
            return this->m_access_state.load( std::memory_order_acquire );
        }

        //! \~japanese Readyならtrue。Linuxは読取可能Source、WindowsはPolling有効を表す。物理接続の保証ではない.
        //! \~english Returns true for Ready: readable sources on Linux, enabled polling on Windows; not proof of physical presence.
        public : bool isAvailable( void ) const noexcept
        {
            return this->accessState() == KeyboardAccessState::Ready;
        }

        //! \~japanese Key state変更Callbackを所有型として設定する.
        //! \~english  Sets an owned callback for key-state changes.
        public : void setCallback( KeyCallback callback_in );

        //! \~japanese Callbackを解除する. 実行中Callbackとは同期しない.
        //! \~english  Clears the callback without waiting for an active invocation.
        public : void clearCallback( void ) noexcept;

        //! 
        //! @brief Get pressed Ascii key code.
        //! @return Input state array of Command Key.
        //! 
        //! @note
        //!     Unsupported Multiple input.
        //!  @n If multiple codes are entered, the smaller code is returned.
        //! 
        public : int8_t getASCII( void ) const;

        //! 
        //! @brief Judge All key are Released.
        //! @return Input state array of Command Key.
        //! @retval true   All key are Released.
        //! @retval false  One or more keys are pressed.
        //! 
        public : bool isReleasedAllKey( void ) ;

        //! 
        //! @brief Initialize.
        //! 
        private : void initialize( void );

        //----------------------------------------------------------------------
        // Constructor/Destructor
        //----------------------------------------------------------------------

        //! 
        //! @brief  Default constoractor.
        //! 
        public : Keyboard( void );

        //!
        //! @brief  Destructor.
        //!
        public : ~Keyboard( void );

        //! 
        //! @brief Move constoractor.
        //! @param [in,out] obj_inout Move src object.
        //! 
        public : Keyboard( Keyboard &&obj_inout )noexcept;
               
        //! 
        //! @brief Move constoractor.
        //! @param [in] obj Move src object.
        //! 
        public : Keyboard& operator = ( Keyboard &&obj )noexcept;

        //! 
        //! @brief Copy operator.
        //! @param [in] obj_in Copy src object.
        //! 
        public : Keyboard( const Keyboard &obj_in );

        //! 
        //! @brief Copy operator.
        //! @param [in] obj Copy src object.
        //! 
        public: Keyboard& operator = ( const Keyboard& obj );

        //----------------------------------------------------------------------
        // MultiThread Process.
        //----------------------------------------------------------------------

        //! 
        //! @brief Internal Method:: Set Lock key state.
        //! 
        private : void setKeyStateLock( void );

        //! 
        //! @brief Internal Method:: Set Arrow key state.
        //! 
        private : void setKeyStateArrow( void );

        //! 
        //! @brief Internal Method:: Set Function key state.
        //! 
        private : void setKeyStateFunction( void );

        //! 
        //! @brief Internal Method:: Set Command key state.
        //! 
        private : void setKeyStateCommand( void );

        //! 
        //! @brief Internal Method:: Set ASCII key state ( a-z, A-Z ).
        //! 
        private : void setKeyStateAlphabet( void );

        //! 
        //! @brief Internal Method:: Set ASCII key state ( 0-9 ).
        //! 
        private : void setKeyStateNumeric( void );

        //! 
        //! @brief Internal Method:: Set ASCII key state ( Symbol ?, !,"...etc ).
        //! 
        private : void setKeyStateSymbol( void );

        //! 
        //! @brief Internal Method:: Set ASCII key state.
        //! 
        private : void setKeyStateASCII_Other( void );

        //!
        //! @brief
        //!     \~japanese 監視Worker本体. 内部Workerの上で実行され、公開APIではない.
        //!     \~english  Monitor worker body; runs on the internal worker and is not public API.
        //!
        private : void monitorLoop( void );

    };
};
};

#endif  //WANDERSTEWENGINE_INPUTUSERINTERFASE_DEVICE_KEYBOARD_H

//*****************************************************************************************************************
//! 
//! @file    Keyboard.cpp
//! @brief   \~japanese GetAsyncKeyStateを監視ThreadでPollingするWindowsのKeyboard実装.
//! @brief   \~english  Windows keyboard implementation that polls GetAsyncKeyState on the monitor thread.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New
//!
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
#include <utility>
#include "iui/device/Keyboard.h"
#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#include "../../../../core/wse/utility/wse_WorkerController.h"
#pragma warning(pop)
#include <windows.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace  wse
{
    namespace  iui
    {
        //! \~japanese Worker・排他・Scan進行度を持つ内部実装. \~english Internal implementation with the worker, lock, and scan progress.
        class Keyboard::Impl
        {
            //! @brief Construct all members with explicit defaults.
public:
            Impl()
                : worker         ()
                , mutex          ()
                , scan_condition ()
                , scan_count     ( 0U )
            {
            }
private:

            public : detail::WorkerController        worker;              //!< 監視Workerの所有者.
            public : mutable std::mutex              mutex;               //!< Key stateとCallbackの排他.
            public : std::condition_variable         scan_condition;      //!< Scan完了待ち.
            public : std::uint64_t                   scan_count;     //!< 完了したScan回数.
        };

        void Keyboard::deleteImpl( Impl* p_in )
        {
            delete p_in;
        }

        //----------------------------------------------------------------------
        // Setter/ Getter
        //----------------------------------------------------------------------

        KeyboardState Keyboard::snapshot( void ) const
        {
            std::lock_guard<std::mutex> lock( this->m_impl->mutex );
            return KeyboardState{
                this->m_ascii_state,
                this->m_function_state,
                this->m_arrow_state,
                this->m_lock_state,
                this->m_command_state
            };
        }

        std::array< bool, ASCII_NUM > Keyboard::ascii_state( void ) const
        {
            return this->snapshot().ascii;
        }

        std::array< bool, FANCTION_NUM > Keyboard::function_state( void ) const
        {
            return this->snapshot().function;
        }

        std::array< bool, ARROW_NUM > Keyboard::arrow_state( void ) const
        {
            return this->snapshot().arrow;
        }

        std::array< bool, LOCK_NUM > Keyboard::lock_state( void ) const
        {
            return this->snapshot().lock;
        }

        std::array< bool, COMMAND_NUM > Keyboard::command_state( void ) const
        {
            return this->snapshot().command;
        }

        void Keyboard::setCallback( KeyCallback callback_in )
        {
            std::shared_ptr<const KeyCallback> owned_callback;
            if( callback_in )
            {
                owned_callback = std::make_shared<const KeyCallback>( std::move( callback_in ) );
            }
            std::lock_guard<std::mutex> lock( this->m_impl->mutex );
            this->m_key_callback = std::move( owned_callback );
        }

        void Keyboard::clearCallback( void ) noexcept
        {
            std::lock_guard<std::mutex> lock( this->m_impl->mutex );
            this->m_key_callback.reset();
        }

        int8_t Keyboard::getASCII() const
        {
            const auto ascii = this->ascii_state();
            for( size_t i = 0; i < ascii.size(); ++i )
            {
                if( ascii[ i ] )
                {
                    return static_cast< int8_t >( i );
                }
            }
            return ASCII_NULL;
        }

        bool Keyboard::isReleasedAllKey( void )
        {
            // 呼出し後に完了した新しいScanを1回待つ. Worker停止・Backend不能でも固まらないよう上限を置く.
            {
                std::unique_lock<std::mutex> lock( this->m_impl->mutex );
                const std::uint64_t entry_count = this->m_impl->scan_count;
                Impl* p_impl = this->m_impl.get();
                this->m_impl->scan_condition.wait_for(
                      lock
                    , std::chrono::milliseconds( 100 )
                    , [ p_impl, entry_count ]() { return p_impl->scan_count != entry_count; } );
            }

            const KeyboardState state = this->snapshot();
            bool is_pressed = false;
            for( const bool is_press : state.ascii    ){ is_pressed |= is_press; }
            for( const bool is_press : state.function ){ is_pressed |= is_press; }
            for( const bool is_press : state.arrow    ){ is_pressed |= is_press; }
            for( const bool is_press : state.command  ){ is_pressed |= is_press; }
            return !is_pressed;
        }

        void Keyboard::initialize( void )
        {
            for( bool& state : this->m_ascii_state    ){ state = false; }
            for( bool& state : this->m_function_state ){ state = false; }
            for( bool& state : this->m_arrow_state    ){ state = false; }
            for( bool& state : this->m_lock_state     ){ state = false; }
            for( bool& state : this->m_command_state  ){ state = false; }
        }

        //----------------------------------------------------------------------
        // Constructor/Destructor
        //----------------------------------------------------------------------

        //!
        //! @brief  デフォルトコンストラクタ.
        //!
        Keyboard::Keyboard( void )
            : m_impl           ( new Impl(), &Keyboard::deleteImpl )
            , m_ascii_state    ()
            , m_function_state ()
            , m_arrow_state    ()
            , m_lock_state     ()
            , m_command_state  ()
            , m_key_callback   ()
            , m_access_state   ( KeyboardAccessState::Ready )
        {
            initialize();

            this->m_impl->worker.start( [ this ]() { this->monitorLoop(); } );
        }

        //!
        //! @brief  デストラクタ.
        //!
        Keyboard::~Keyboard( void )
        {
            if( this->m_impl != nullptr )
            {
                this->m_impl->worker.stop();
            }
        }

        //!
        //! @brief ムーブコンストラクタ. 移動元の監視を停止し、自分の監視を開始する.
        //!
        Keyboard::Keyboard( Keyboard &&obj_inout )noexcept
            : m_impl           ( new Impl(), &Keyboard::deleteImpl )
            , m_ascii_state    ()
            , m_function_state ()
            , m_arrow_state    ()
            , m_lock_state     ()
            , m_command_state  ()
            , m_key_callback   ()
            , m_access_state   ( obj_inout.m_access_state.load( std::memory_order_acquire ) )
        {
            obj_inout.m_impl->worker.stop();
            {
                std::lock_guard<std::mutex> lock( obj_inout.m_impl->mutex );
                this->m_ascii_state    = std::move( obj_inout.m_ascii_state    );
                this->m_function_state = std::move( obj_inout.m_function_state );
                this->m_arrow_state    = std::move( obj_inout.m_arrow_state    );
                this->m_lock_state     = std::move( obj_inout.m_lock_state     );
                this->m_command_state  = std::move( obj_inout.m_command_state  );
                this->m_key_callback   = std::move( obj_inout.m_key_callback   );
            }
            this->m_impl->worker.start( [ this ]() { this->monitorLoop(); } );
        }

        //!
        //! @brief ムーブオペレーター. 双方の監視を停止し、自分の監視を再開する.
        //!
        Keyboard& Keyboard::operator = ( Keyboard &&obj )noexcept
        {
            if( this != &obj )
            {
                this->m_impl->worker.stop();
                obj.m_impl->worker.stop();
                {
                    std::scoped_lock lock( this->m_impl->mutex, obj.m_impl->mutex );
                    this->m_ascii_state    = std::move( obj.m_ascii_state    );
                    this->m_function_state = std::move( obj.m_function_state );
                    this->m_arrow_state    = std::move( obj.m_arrow_state    );
                    this->m_lock_state     = std::move( obj.m_lock_state     );
                    this->m_command_state  = std::move( obj.m_command_state  );
                    this->m_key_callback   = std::move( obj.m_key_callback   );
                }
                this->m_access_state.store(
                    obj.m_access_state.load( std::memory_order_acquire ),
                    std::memory_order_release );
                this->m_impl->worker.start( [ this ]() { this->monitorLoop(); } );
            }
            return *this;
        }

        //!
        //! @brief コピーコンストラクタ. 複製元の監視は継続し、自分の監視を開始する.
        //!
        Keyboard::Keyboard( const Keyboard &obj_in )
            : m_impl           ( new Impl(), &Keyboard::deleteImpl )
            , m_ascii_state    ()
            , m_function_state ()
            , m_arrow_state    ()
            , m_lock_state     ()
            , m_command_state  ()
            , m_key_callback   ()
            , m_access_state   ( obj_in.m_access_state.load( std::memory_order_acquire ) )
        {
            {
                std::lock_guard<std::mutex> lock( obj_in.m_impl->mutex );
                this->m_ascii_state = obj_in.m_ascii_state;
                this->m_function_state = obj_in.m_function_state;
                this->m_arrow_state = obj_in.m_arrow_state;
                this->m_lock_state = obj_in.m_lock_state;
                this->m_command_state = obj_in.m_command_state;
                this->m_key_callback = obj_in.m_key_callback;
            }
            this->m_impl->worker.start( [ this ]() { this->monitorLoop(); } );
        }

        //!
        //! @brief コピーオペレーター.
        //!
        Keyboard& Keyboard::operator = ( const Keyboard& obj )
        {
            if( this != &obj )
            {
                this->m_impl->worker.stop();
                {
                    std::scoped_lock lock( this->m_impl->mutex, obj.m_impl->mutex );
                    this->m_ascii_state = obj.m_ascii_state;
                    this->m_function_state = obj.m_function_state;
                    this->m_arrow_state = obj.m_arrow_state;
                    this->m_lock_state = obj.m_lock_state;
                    this->m_command_state = obj.m_command_state;
                    this->m_key_callback = obj.m_key_callback;
                }
                this->m_access_state.store(
                    obj.m_access_state.load( std::memory_order_acquire ),
                    std::memory_order_release );
                this->m_impl->worker.start( [ this ]() { this->monitorLoop(); } );
            }
            return *this;

        }


        //----------------------------------------------------------------------
        // MultiThread Process.
        //----------------------------------------------------------------------

        void Keyboard::setKeyStateLock( void )
        {
            this->m_lock_state[ LOCK_CAPS   ] = ( GetKeyState( VK_CAPITAL  ) & 0x0001 );
            this->m_lock_state[ LOCK_NUMLK  ] = ( GetKeyState( VK_NUMLOCK  ) & 0x0001 );
            this->m_lock_state[ LOCK_SCROLL ] = ( GetKeyState( VK_SCROLL   ) & 0x0001 );
        
        }

        void Keyboard::setKeyStateArrow( void )
        {
            this->m_arrow_state[ ARROW_LEFT  ] = ( GetAsyncKeyState( VK_LEFT  ) & 0x8000 );
            this->m_arrow_state[ ARROW_TOP   ] = ( GetAsyncKeyState( VK_UP    ) & 0x8000 );
            this->m_arrow_state[ ARROW_LOW   ] = ( GetAsyncKeyState( VK_DOWN  ) & 0x8000 );
            this->m_arrow_state[ ARROW_RIGHT ] = ( GetAsyncKeyState( VK_RIGHT ) & 0x8000 );
        }

        void Keyboard::setKeyStateFunction( void )
        {
            
            this->m_function_state[  0 ] = GetAsyncKeyState( VK_F1  ) & 0x8000;
            this->m_function_state[  1 ] = GetAsyncKeyState( VK_F2  ) & 0x8000;
            this->m_function_state[  2 ] = GetAsyncKeyState( VK_F3  ) & 0x8000;
            this->m_function_state[  3 ] = GetAsyncKeyState( VK_F4  ) & 0x8000;
            this->m_function_state[  4 ] = GetAsyncKeyState( VK_F5  ) & 0x8000;
            this->m_function_state[  5 ] = GetAsyncKeyState( VK_F6  ) & 0x8000;
            this->m_function_state[  6 ] = GetAsyncKeyState( VK_F7  ) & 0x8000;
            this->m_function_state[  7 ] = GetAsyncKeyState( VK_F8  ) & 0x8000;
            this->m_function_state[  8 ] = GetAsyncKeyState( VK_F9  ) & 0x8000;
            this->m_function_state[  9 ] = GetAsyncKeyState( VK_F10 ) & 0x8000;
            this->m_function_state[ 10 ] = GetAsyncKeyState( VK_F11 ) & 0x8000;
            this->m_function_state[ 11 ] = GetAsyncKeyState( VK_F12 ) & 0x8000;
            this->m_function_state[ 12 ] = GetAsyncKeyState( VK_F13 ) & 0x8000;
            this->m_function_state[ 13 ] = GetAsyncKeyState( VK_F14 ) & 0x8000;
            this->m_function_state[ 14 ] = GetAsyncKeyState( VK_F15 ) & 0x8000;
            this->m_function_state[ 15 ] = GetAsyncKeyState( VK_F16 ) & 0x8000;
            this->m_function_state[ 16 ] = GetAsyncKeyState( VK_F17 ) & 0x8000;
            this->m_function_state[ 17 ] = GetAsyncKeyState( VK_F18 ) & 0x8000;
            this->m_function_state[ 18 ] = GetAsyncKeyState( VK_F19 ) & 0x8000;
            this->m_function_state[ 19 ] = GetAsyncKeyState( VK_F21 ) & 0x8000;
            this->m_function_state[ 20 ] = GetAsyncKeyState( VK_F22 ) & 0x8000;
            this->m_function_state[ 21 ] = GetAsyncKeyState( VK_F23 ) & 0x8000;
            this->m_function_state[ 22 ] = GetAsyncKeyState( VK_F24 ) & 0x8000;

        }

        void Keyboard::setKeyStateCommand( void )
        {
            this->m_command_state[ COM_SPACE     ] = ( GetAsyncKeyState( VK_SPACE   ) & 0x8000 );
            this->m_command_state[ COM_ENTER     ] = ( GetAsyncKeyState( VK_RETURN  ) != 0 );
            this->m_command_state[ COM_DELETE    ] = ( GetAsyncKeyState( VK_DELETE  ) & 0x8000 );
            this->m_command_state[ COM_ESC       ] = ( GetAsyncKeyState( VK_ESCAPE  ) & 0x8000 );
            this->m_command_state[ COM_TAB       ] = ( GetAsyncKeyState( VK_TAB     ) & 0x8000 );
            this->m_command_state[ COM_SHIFT     ] = ( GetAsyncKeyState( VK_RSHIFT  ) & 0x8000 ) || ( GetAsyncKeyState( VK_LSHIFT ) & 0x8000 );
            this->m_command_state[ COM_CONTROLL  ] = ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 );
            this->m_command_state[ COM_BACKSPACE ] = ( GetAsyncKeyState( VK_BACK   ) & 0x8000 );
            this->m_command_state[ COM_MENU      ] = ( GetAsyncKeyState( VK_MENU   ) & 0x8000 );
        }

        void Keyboard::setKeyStateAlphabet( void )
        {
            
            // Caps lock is true EXOR Shift is true
            const bool is_big_alphabet = this->m_lock_state[ LOCK_CAPS   ] ^ this->m_command_state[ COM_SHIFT     ];

            if( is_big_alphabet )
            {
                this->m_ascii_state[ 'A' ] = ( GetAsyncKeyState( 'A' ) & 0x8000 );
                this->m_ascii_state[ 'B' ] = ( GetAsyncKeyState( 'B' ) & 0x8000 );
                this->m_ascii_state[ 'C' ] = ( GetAsyncKeyState( 'C' ) & 0x8000 );
                this->m_ascii_state[ 'D' ] = ( GetAsyncKeyState( 'D' ) & 0x8000 );
                this->m_ascii_state[ 'E' ] = ( GetAsyncKeyState( 'E' ) & 0x8000 );
                this->m_ascii_state[ 'F' ] = ( GetAsyncKeyState( 'F' ) & 0x8000 );
                this->m_ascii_state[ 'G' ] = ( GetAsyncKeyState( 'G' ) & 0x8000 );
                this->m_ascii_state[ 'H' ] = ( GetAsyncKeyState( 'H' ) & 0x8000 );
                this->m_ascii_state[ 'I' ] = ( GetAsyncKeyState( 'I' ) & 0x8000 );
                this->m_ascii_state[ 'J' ] = ( GetAsyncKeyState( 'J' ) & 0x8000 );
                this->m_ascii_state[ 'K' ] = ( GetAsyncKeyState( 'K' ) & 0x8000 );
                this->m_ascii_state[ 'L' ] = ( GetAsyncKeyState( 'L' ) & 0x8000 );
                this->m_ascii_state[ 'M' ] = ( GetAsyncKeyState( 'M' ) & 0x8000 );
                this->m_ascii_state[ 'N' ] = ( GetAsyncKeyState( 'N' ) & 0x8000 );
                this->m_ascii_state[ 'O' ] = ( GetAsyncKeyState( 'O' ) & 0x8000 );
                this->m_ascii_state[ 'P' ] = ( GetAsyncKeyState( 'P' ) & 0x8000 );
                this->m_ascii_state[ 'Q' ] = ( GetAsyncKeyState( 'Q' ) & 0x8000 );
                this->m_ascii_state[ 'R' ] = ( GetAsyncKeyState( 'R' ) & 0x8000 );
                this->m_ascii_state[ 'S' ] = ( GetAsyncKeyState( 'S' ) & 0x8000 );
                this->m_ascii_state[ 'T' ] = ( GetAsyncKeyState( 'T' ) & 0x8000 );
                this->m_ascii_state[ 'U' ] = ( GetAsyncKeyState( 'U' ) & 0x8000 );
                this->m_ascii_state[ 'V' ] = ( GetAsyncKeyState( 'V' ) & 0x8000 );
                this->m_ascii_state[ 'W' ] = ( GetAsyncKeyState( 'W' ) & 0x8000 );
                this->m_ascii_state[ 'X' ] = ( GetAsyncKeyState( 'X' ) & 0x8000 );
                this->m_ascii_state[ 'Y' ] = ( GetAsyncKeyState( 'Y' ) & 0x8000 );
                this->m_ascii_state[ 'Z' ] = ( GetAsyncKeyState( 'Z' ) & 0x8000 );
            }
            else
            {
                this->m_ascii_state[ 'a' ] = ( GetAsyncKeyState( 'A' ) & 0x8000 );
                this->m_ascii_state[ 'b' ] = ( GetAsyncKeyState( 'B' ) & 0x8000 );
                this->m_ascii_state[ 'c' ] = ( GetAsyncKeyState( 'C' ) & 0x8000 );
                this->m_ascii_state[ 'd' ] = ( GetAsyncKeyState( 'D' ) & 0x8000 );
                this->m_ascii_state[ 'e' ] = ( GetAsyncKeyState( 'E' ) & 0x8000 );
                this->m_ascii_state[ 'f' ] = ( GetAsyncKeyState( 'F' ) & 0x8000 );
                this->m_ascii_state[ 'g' ] = ( GetAsyncKeyState( 'G' ) & 0x8000 );
                this->m_ascii_state[ 'h' ] = ( GetAsyncKeyState( 'H' ) & 0x8000 );
                this->m_ascii_state[ 'i' ] = ( GetAsyncKeyState( 'I' ) & 0x8000 );
                this->m_ascii_state[ 'j' ] = ( GetAsyncKeyState( 'J' ) & 0x8000 );
                this->m_ascii_state[ 'k' ] = ( GetAsyncKeyState( 'K' ) & 0x8000 );
                this->m_ascii_state[ 'l' ] = ( GetAsyncKeyState( 'L' ) & 0x8000 );
                this->m_ascii_state[ 'm' ] = ( GetAsyncKeyState( 'M' ) & 0x8000 );
                this->m_ascii_state[ 'n' ] = ( GetAsyncKeyState( 'N' ) & 0x8000 );
                this->m_ascii_state[ 'o' ] = ( GetAsyncKeyState( 'O' ) & 0x8000 );
                this->m_ascii_state[ 'p' ] = ( GetAsyncKeyState( 'P' ) & 0x8000 );
                this->m_ascii_state[ 'q' ] = ( GetAsyncKeyState( 'Q' ) & 0x8000 );
                this->m_ascii_state[ 'r' ] = ( GetAsyncKeyState( 'R' ) & 0x8000 );
                this->m_ascii_state[ 's' ] = ( GetAsyncKeyState( 'S' ) & 0x8000 );
                this->m_ascii_state[ 't' ] = ( GetAsyncKeyState( 'T' ) & 0x8000 );
                this->m_ascii_state[ 'u' ] = ( GetAsyncKeyState( 'U' ) & 0x8000 );
                this->m_ascii_state[ 'v' ] = ( GetAsyncKeyState( 'V' ) & 0x8000 );
                this->m_ascii_state[ 'w' ] = ( GetAsyncKeyState( 'W' ) & 0x8000 );
                this->m_ascii_state[ 'x' ] = ( GetAsyncKeyState( 'X' ) & 0x8000 );
                this->m_ascii_state[ 'y' ] = ( GetAsyncKeyState( 'Y' ) & 0x8000 );
                this->m_ascii_state[ 'z' ] = ( GetAsyncKeyState( 'Z' ) & 0x8000 );
            }
                
        }

        void Keyboard::setKeyStateNumeric( void )
        {
            const bool is_numlock = this->m_lock_state[ LOCK_NUMLK    ];
            const bool is_shift   = this->m_command_state[ COM_SHIFT     ];

            this->m_ascii_state[ '0' ] = ( !is_shift && ( GetAsyncKeyState( '0' ) & 0x8000 ) ) || ( is_numlock && ( GetAsyncKeyState( VK_NUMPAD0 ) & 0x8000 ) );
            this->m_ascii_state[ '1' ] = ( !is_shift && ( GetAsyncKeyState( '1' ) & 0x8000 ) ) || ( is_numlock && ( GetAsyncKeyState( VK_NUMPAD1 ) & 0x8000 ) );
            this->m_ascii_state[ '2' ] = ( !is_shift && ( GetAsyncKeyState( '2' ) & 0x8000 ) ) || ( is_numlock && ( GetAsyncKeyState( VK_NUMPAD2 ) & 0x8000 ) );
            this->m_ascii_state[ '3' ] = ( !is_shift && ( GetAsyncKeyState( '3' ) & 0x8000 ) ) || ( is_numlock && ( GetAsyncKeyState( VK_NUMPAD3 ) & 0x8000 ) );
            this->m_ascii_state[ '4' ] = ( !is_shift && ( GetAsyncKeyState( '4' ) & 0x8000 ) ) || ( is_numlock && ( GetAsyncKeyState( VK_NUMPAD4 ) & 0x8000 ) );
            this->m_ascii_state[ '5' ] = ( !is_shift && ( GetAsyncKeyState( '5' ) & 0x8000 ) ) || ( is_numlock && ( GetAsyncKeyState( VK_NUMPAD5 ) & 0x8000 ) );
            this->m_ascii_state[ '6' ] = ( !is_shift && ( GetAsyncKeyState( '6' ) & 0x8000 ) ) || ( is_numlock && ( GetAsyncKeyState( VK_NUMPAD6 ) & 0x8000 ) );
            this->m_ascii_state[ '7' ] = ( !is_shift && ( GetAsyncKeyState( '7' ) & 0x8000 ) ) || ( is_numlock && ( GetAsyncKeyState( VK_NUMPAD7 ) & 0x8000 ) );
            this->m_ascii_state[ '8' ] = ( !is_shift && ( GetAsyncKeyState( '8' ) & 0x8000 ) ) || ( is_numlock && ( GetAsyncKeyState( VK_NUMPAD8 ) & 0x8000 ) );
            this->m_ascii_state[ '9' ] = ( !is_shift && ( GetAsyncKeyState( '9' ) & 0x8000 ) ) || ( is_numlock && ( GetAsyncKeyState( VK_NUMPAD9 ) & 0x8000 ) );
        }

        void Keyboard::setKeyStateSymbol( void )
        {
            const bool is_shift = this->m_command_state[ COM_SHIFT     ];

            this->m_ascii_state[ ' ' ] = this->m_command_state[ COM_SPACE     ];  //VK_SPACE
            this->m_ascii_state[ '!' ] = ( is_shift && ( GetAsyncKeyState( '1' ) & 0x8000 ) );
            this->m_ascii_state[ '"' ] = ( is_shift && ( GetAsyncKeyState( '2' ) & 0x8000 ) );
            this->m_ascii_state[ '#' ] = ( is_shift && ( GetAsyncKeyState( '3' ) & 0x8000 ) );
            this->m_ascii_state[ '$' ] = ( is_shift && ( GetAsyncKeyState( '4' ) & 0x8000 ) );
            this->m_ascii_state[ '%' ] = ( is_shift && ( GetAsyncKeyState( '5' ) & 0x8000 ) );
            this->m_ascii_state[ '&' ] = ( is_shift && ( GetAsyncKeyState( '6' ) & 0x8000 ) );
            this->m_ascii_state['\'' ] = ( is_shift && ( GetAsyncKeyState( '7' ) & 0x8000 ) );     // '
            this->m_ascii_state[ '(' ] = ( is_shift && ( GetAsyncKeyState( '8' ) & 0x8000 ) );
            this->m_ascii_state[ ')' ] = ( is_shift && ( GetAsyncKeyState( '9' ) & 0x8000 ) );

            this->m_ascii_state[ '*' ] = ( !is_shift && ( GetAsyncKeyState( VK_OEM_1      ) & 0x8000 ) ) || ( GetAsyncKeyState( VK_MULTIPLY   ) & 0x8000 );
            this->m_ascii_state[ '+' ] = (  is_shift && ( GetAsyncKeyState( VK_OEM_PLUS   ) & 0x8000 ) ) || ( GetAsyncKeyState( VK_ADD        ) & 0x8000 );
            this->m_ascii_state[ ',' ] = ( !is_shift && ( GetAsyncKeyState( VK_OEM_COMMA  ) & 0x8000 ) );
            this->m_ascii_state[ '-' ] = ( !is_shift && ( GetAsyncKeyState( VK_OEM_MINUS  ) & 0x8000 ) ) || ( GetAsyncKeyState( VK_SUBTRACT   ) & 0x8000 );
            this->m_ascii_state[ '.' ] = ( !is_shift && ( GetAsyncKeyState( VK_OEM_PERIOD ) & 0x8000 ) ) || ( GetAsyncKeyState( VK_DECIMAL    ) & 0x8000 );
            this->m_ascii_state[ '/' ] = (  is_shift && ( GetAsyncKeyState( VK_OEM_2      ) & 0x8000 ) ) || ( GetAsyncKeyState( VK_DIVIDE     ) & 0x8000 );
            this->m_ascii_state[ ':' ] = ( !is_shift && ( GetAsyncKeyState( VK_OEM_1 ) & 0x8000 ) );
            this->m_ascii_state[ ';' ] = ( !is_shift && ( GetAsyncKeyState( VK_OEM_PLUS ) & 0x8000 ) );
            this->m_ascii_state[ '<' ] = (  is_shift && ( GetAsyncKeyState( VK_OEM_COMMA ) & 0x8000 ) );
            this->m_ascii_state[ '=' ] = (  is_shift && ( GetAsyncKeyState( VK_OEM_MINUS ) & 0x8000 ) );
            this->m_ascii_state[ '>' ] = (  is_shift && ( GetAsyncKeyState( VK_OEM_PERIOD ) & 0x8000 ) );
            this->m_ascii_state[ '?' ] = ( !is_shift && ( GetAsyncKeyState( VK_OEM_2 ) & 0x8000 ) );
            this->m_ascii_state[ '@' ] = ( is_shift && ( VkKeyScanA( '9' ) & 0x8000 ) );
            this->m_ascii_state[ '[' ] = ( !is_shift && ( GetAsyncKeyState( VK_OEM_4 ) & 0x8000 ) );
            this->m_ascii_state['\\' ] = ( !is_shift && ( GetAsyncKeyState( VK_OEM_5 ) & 0x8000 ) );         // \.
            this->m_ascii_state[ ']' ] = ( !is_shift && ( GetAsyncKeyState( VK_OEM_6 ) & 0x8000 ) );
            this->m_ascii_state[ '^' ] = (  is_shift && ( GetAsyncKeyState( VK_OEM_3 ) & 0x8000 ) );
            this->m_ascii_state[ '_' ] = ( is_shift && ( GetAsyncKeyState( '9' ) & 0x8000 ) );
            this->m_ascii_state[ '`' ] = ( is_shift && ( GetAsyncKeyState( '9' ) & 0x8000 ) );
            this->m_ascii_state[ '{' ] = (  is_shift && ( GetAsyncKeyState( VK_OEM_4 ) & 0x8000 ) );
            this->m_ascii_state[ '|' ] = (  is_shift && ( GetAsyncKeyState( VK_OEM_5 ) & 0x8000 ) );
            this->m_ascii_state[ '}' ] = (  is_shift && ( GetAsyncKeyState( VK_OEM_6 ) & 0x8000 ) );
            this->m_ascii_state[ '~' ] = (  is_shift && ( GetAsyncKeyState( '~' ) & 0x8000 ) );
        }

        void Keyboard::setKeyStateASCII_Other( void )
        {
                
            this->m_ascii_state[ ASCII_BACK ] = this->m_command_state[ COM_BACKSPACE ];  // VK_BACK
            this->m_ascii_state[ ASCII_TAB  ] = this->m_command_state[ COM_TAB       ];  // VK_TAB
            this->m_ascii_state[ ASCII_CR   ] = this->m_command_state[ COM_ENTER     ];
            this->m_ascii_state[ ASCII_LF   ] = this->m_command_state[ COM_ENTER     ];
            this->m_ascii_state[ ASCII_ESC  ] = this->m_command_state[ COM_ESC       ];
            this->m_ascii_state[ ASCII_DEL  ] = this->m_command_state[ COM_DELETE    ];  //VK_DELETE
            // this->m_ascii_state[   0 ] = ( GetAsyncKeyState(   0 ) & 0x8000 );
            // this->m_ascii_state[   1 ] = ( GetAsyncKeyState(   1 ) & 0x8000 );
            // this->m_ascii_state[   2 ] = ( GetAsyncKeyState(   2 ) & 0x8000 );
            // this->m_ascii_state[   3 ] = ( GetAsyncKeyState(   3 ) & 0x8000 );
            // this->m_ascii_state[   4 ] = ( GetAsyncKeyState(   4 ) & 0x8000 );
            // this->m_ascii_state[   5 ] = ( GetAsyncKeyState(   5 ) & 0x8000 );
            // this->m_ascii_state[   6 ] = ( GetAsyncKeyState(   6 ) & 0x8000 );
            // this->m_ascii_state[   7 ] = ( GetAsyncKeyState(   7 ) & 0x8000 );
            // this->m_ascii_state[  11 ] = ( GetAsyncKeyState(  11 ) & 0x8000 );
            // this->m_ascii_state[  12 ] = ( GetAsyncKeyState(  12 ) & 0x8000 );
            // this->m_ascii_state[  14 ] = ( GetAsyncKeyState(  14 ) & 0x8000 );
            // this->m_ascii_state[  15 ] = ( GetAsyncKeyState(  15 ) & 0x8000 );
            // this->m_ascii_state[  16 ] = ( GetAsyncKeyState(  16 ) & 0x8000 );
            // this->m_ascii_state[  17 ] = ( GetAsyncKeyState(  17 ) & 0x8000 );
            // this->m_ascii_state[  18 ] = ( GetAsyncKeyState(  18 ) & 0x8000 );
            // this->m_ascii_state[  19 ] = ( GetAsyncKeyState(  19 ) & 0x8000 );
            // this->m_ascii_state[  20 ] = ( GetAsyncKeyState(  20 ) & 0x8000 );
            // this->m_ascii_state[  21 ] = ( GetAsyncKeyState(  21 ) & 0x8000 );
            // this->m_ascii_state[  22 ] = ( GetAsyncKeyState(  22 ) & 0x8000 );
            // this->m_ascii_state[  23 ] = ( GetAsyncKeyState(  23 ) & 0x8000 );
            // this->m_ascii_state[  24 ] = ( GetAsyncKeyState(  24 ) & 0x8000 );
            // this->m_ascii_state[  25 ] = ( GetAsyncKeyState(  25 ) & 0x8000 );
            // this->m_ascii_state[  26 ] = ( GetAsyncKeyState(  26 ) & 0x8000 );
            // this->m_ascii_state[  28 ] = ( GetAsyncKeyState(  28 ) & 0x8000 );
            // this->m_ascii_state[  29 ] = ( GetAsyncKeyState(  29 ) & 0x8000 );
            // this->m_ascii_state[  30 ] = ( GetAsyncKeyState(  30 ) & 0x8000 );
            // this->m_ascii_state[  31 ] = ( GetAsyncKeyState(  31 ) & 0x8000 );
        
        }


        //! @brief  監視Worker本体.
        void Keyboard::monitorLoop( void )
        {
            // 5msec間隔のPollingは約200Hz。人の打鍵より十分速く、CPUをほぼ使わない釣り合いの点.
            // 停止要求は待機中のWorkerを即座に起こす.
            static const std::chrono::milliseconds PROCESS_WAIT{ 5 };

            Impl& impl = *this->m_impl;
            while( !impl.worker.waitFor( PROCESS_WAIT ) )
            {
                KeyboardState previous;
                KeyboardState current;
                std::shared_ptr<const KeyCallback> callback;
                {
                    std::lock_guard<std::mutex> lock( impl.mutex );
                    previous = KeyboardState{
                        this->m_ascii_state,
                        this->m_function_state,
                        this->m_arrow_state,
                        this->m_lock_state,
                        this->m_command_state
                    };

                    initialize();
                    setKeyStateLock();
                    setKeyStateCommand();
                    setKeyStateFunction();
                    setKeyStateArrow();
                    setKeyStateAlphabet();
                    setKeyStateNumeric();
                    setKeyStateSymbol();
                    setKeyStateASCII_Other();

                    current = KeyboardState{
                        this->m_ascii_state,
                        this->m_function_state,
                        this->m_arrow_state,
                        this->m_lock_state,
                        this->m_command_state
                    };
                    callback = this->m_key_callback;
                }

                const bool changed =
                    previous.ascii    != current.ascii    ||
                    previous.function != current.function ||
                    previous.arrow    != current.arrow    ||
                    previous.lock     != current.lock     ||
                    previous.command  != current.command;
                // Callbackの呼び出しはLockの外で行う。利用者のCallbackがKeyboardのAPIを呼び返しても
                // Deadlockしないためであり、shared_ptrのCopyを持つのでclearCallbackと競合しても安全.
                if( changed && callback != nullptr && !impl.worker.isStopRequested() )
                {
                    try
                    {
                        ( *callback )( current );
                    }
                    catch( ... )
                    {
                        // 例外を投げるCallbackは以後信用できないので登録を外し、監視自体は続ける.
                        this->clearCallback();
                    }
                }

                // Scan完了を通知する. isReleasedAllKey()が新しいScanを1回待つために使う.
                {
                    std::lock_guard<std::mutex> lock( impl.mutex );
                    ++impl.scan_count;
                }
                impl.scan_condition.notify_all();
            }
        }

    };
};

//*****************************************************************************************************************
//!
//! @file    Keyboard.cpp
//! @brief   \~japanese Linux evdevを使用するPortable Keyboard監視を実装する.
//! @brief   \~english  Implements portable keyboard monitoring with Linux evdev.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Aug-30, 2026   Create New.
//!
//*****************************************************************************************************************
#include "iui/device/Keyboard.h"
#include "LinuxKeyboardDevices.h"
#include "../../../../core/wse/utility/wse_WorkerController.h"

#include <linux/input.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace
{
    bool isPressed(
        const std::array< bool, KEY_MAX + 1U >& keys_in,
        const std::size_t key_in )
    {
        return key_in < keys_in.size() && keys_in[ key_in ];
    }

    void setAscii(
        wse::iui::KeyboardState* const p_state_inout,
        const unsigned char ascii_in,
        const bool pressed_in )
    {
        p_state_inout->ascii[ ascii_in ] =
            p_state_inout->ascii[ ascii_in ] || pressed_in;
    }

    wse::iui::KeyboardState mapKeyboardState(
        const std::array< bool, KEY_MAX + 1U >& keys_in,
        const std::array< bool, LED_MAX + 1U >& leds_in )
    {
        using namespace wse::iui;

        KeyboardState state{};
        const bool shift = isPressed( keys_in, KEY_LEFTSHIFT ) ||
            isPressed( keys_in, KEY_RIGHTSHIFT );
        const bool caps_lock = leds_in[ LED_CAPSL ];
        const bool num_lock = leds_in[ LED_NUML ];

        state.lock[ LOCK_CAPS ] = caps_lock;
        state.lock[ LOCK_NUMLK ] = num_lock;
        state.lock[ LOCK_SCROLL ] = leds_in[ LED_SCROLLL ];

        state.command[ COM_SPACE ] = isPressed( keys_in, KEY_SPACE );
        state.command[ COM_ENTER ] = isPressed( keys_in, KEY_ENTER ) ||
            isPressed( keys_in, KEY_KPENTER );
        state.command[ COM_DELETE ] = isPressed( keys_in, KEY_DELETE );
        state.command[ COM_ESC ] = isPressed( keys_in, KEY_ESC );
        state.command[ COM_TAB ] = isPressed( keys_in, KEY_TAB );
        state.command[ COM_SHIFT ] = shift;
        state.command[ COM_CONTROLL ] = isPressed( keys_in, KEY_LEFTCTRL ) ||
            isPressed( keys_in, KEY_RIGHTCTRL );
        state.command[ COM_BACKSPACE ] = isPressed( keys_in, KEY_BACKSPACE );
        state.command[ COM_MENU ] = isPressed( keys_in, KEY_LEFTALT ) ||
            isPressed( keys_in, KEY_RIGHTALT ) || isPressed( keys_in, KEY_MENU );

        state.arrow[ ARROW_LEFT ] = isPressed( keys_in, KEY_LEFT );
        state.arrow[ ARROW_TOP ] = isPressed( keys_in, KEY_UP );
        state.arrow[ ARROW_LOW ] = isPressed( keys_in, KEY_DOWN );
        state.arrow[ ARROW_RIGHT ] = isPressed( keys_in, KEY_RIGHT );

        static constexpr std::array< int, FANCTION_NUM > FUNCTION_KEYS{
            KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
            KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
            KEY_F13, KEY_F14, KEY_F15, KEY_F16, KEY_F17, KEY_F18,
            KEY_F19, KEY_F20, KEY_F21, KEY_F22, KEY_F23, KEY_F24 };
        for( std::size_t index = 0U; index < FUNCTION_KEYS.size(); ++index )
        {
            state.function[ index ] = isPressed( keys_in, FUNCTION_KEYS[ index ] );
        }

        static constexpr std::array< int, ALPHABET_NUM > ALPHABET_KEYS{
            KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G,
            KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N,
            KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U,
            KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z };
        const bool uppercase = caps_lock != shift;
        for( std::size_t index = 0U; index < ALPHABET_KEYS.size(); ++index )
        {
            const unsigned char ascii = static_cast< unsigned char >(
                ( uppercase ? 'A' : 'a' ) + index );
            setAscii( &state, ascii, isPressed( keys_in, ALPHABET_KEYS[ index ] ) );
        }

        static constexpr std::array< int, NUMBER_NUM > NUMBER_KEYS{
            KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
            KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };
        static constexpr std::array< int, NUMBER_NUM > KEYPAD_KEYS{
            KEY_KP0, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP4,
            KEY_KP5, KEY_KP6, KEY_KP7, KEY_KP8, KEY_KP9 };
        static constexpr std::array< unsigned char, NUMBER_NUM > SHIFTED_NUMBERS{
            ')', '!', '@', '#', '$', '%', '^', '&', '*', '(' };
        for( std::size_t index = 0U; index < NUMBER_KEYS.size(); ++index )
        {
            const bool main_pressed = isPressed( keys_in, NUMBER_KEYS[ index ] );
            const bool keypad_pressed = num_lock && isPressed( keys_in, KEYPAD_KEYS[ index ] );
            setAscii(
                &state,
                static_cast< unsigned char >( '0' + index ),
                ( !shift && main_pressed ) || keypad_pressed );
            setAscii( &state, SHIFTED_NUMBERS[ index ], shift && main_pressed );
        }

        setAscii( &state, ' ', state.command[ COM_SPACE ] );
        setAscii( &state, '-', !shift && isPressed( keys_in, KEY_MINUS ) );
        setAscii( &state, '_', shift && isPressed( keys_in, KEY_MINUS ) );
        setAscii( &state, '=', !shift && isPressed( keys_in, KEY_EQUAL ) );
        setAscii( &state, '+', shift && isPressed( keys_in, KEY_EQUAL ) );
        setAscii( &state, '[', !shift && isPressed( keys_in, KEY_LEFTBRACE ) );
        setAscii( &state, '{', shift && isPressed( keys_in, KEY_LEFTBRACE ) );
        setAscii( &state, ']', !shift && isPressed( keys_in, KEY_RIGHTBRACE ) );
        setAscii( &state, '}', shift && isPressed( keys_in, KEY_RIGHTBRACE ) );
        setAscii( &state, '\\', !shift && isPressed( keys_in, KEY_BACKSLASH ) );
        setAscii( &state, '|', shift && isPressed( keys_in, KEY_BACKSLASH ) );
        setAscii( &state, ';', !shift && isPressed( keys_in, KEY_SEMICOLON ) );
        setAscii( &state, ':', shift && isPressed( keys_in, KEY_SEMICOLON ) );
        setAscii( &state, '\'', !shift && isPressed( keys_in, KEY_APOSTROPHE ) );
        setAscii( &state, '"', shift && isPressed( keys_in, KEY_APOSTROPHE ) );
        setAscii( &state, '`', !shift && isPressed( keys_in, KEY_GRAVE ) );
        setAscii( &state, '~', shift && isPressed( keys_in, KEY_GRAVE ) );
        setAscii( &state, ',', !shift && isPressed( keys_in, KEY_COMMA ) );
        setAscii( &state, '<', shift && isPressed( keys_in, KEY_COMMA ) );
        setAscii( &state, '.', ( !shift && isPressed( keys_in, KEY_DOT ) ) ||
            ( num_lock && isPressed( keys_in, KEY_KPDOT ) ) );
        setAscii( &state, '>', shift && isPressed( keys_in, KEY_DOT ) );
        setAscii( &state, '/', !shift && isPressed( keys_in, KEY_SLASH ) );
        setAscii( &state, '?', shift && isPressed( keys_in, KEY_SLASH ) );
        setAscii( &state, '*', isPressed( keys_in, KEY_KPASTERISK ) );
        setAscii( &state, '+', isPressed( keys_in, KEY_KPPLUS ) );
        setAscii( &state, '-', isPressed( keys_in, KEY_KPMINUS ) );
        setAscii( &state, '/', isPressed( keys_in, KEY_KPSLASH ) );

        setAscii( &state, ASCII_BACK, state.command[ COM_BACKSPACE ] );
        setAscii( &state, ASCII_TAB, state.command[ COM_TAB ] );
        setAscii( &state, ASCII_CR, state.command[ COM_ENTER ] );
        setAscii( &state, ASCII_LF, state.command[ COM_ENTER ] );
        setAscii( &state, ASCII_ESC, state.command[ COM_ESC ] );
        setAscii( &state, ASCII_DEL, state.command[ COM_DELETE ] );
        return state;
    }

    bool statesDiffer(
        const wse::iui::KeyboardState& left_in,
        const wse::iui::KeyboardState& right_in )
    {
        return left_in.ascii != right_in.ascii ||
            left_in.function != right_in.function ||
            left_in.arrow != right_in.arrow ||
            left_in.lock != right_in.lock ||
            left_in.command != right_in.command;
    }
}

namespace wse
{
namespace iui
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

        public : wse::detail::WorkerController   worker;              //!< 監視Workerの所有者.
        public : mutable std::mutex              mutex;               //!< Key stateとCallbackの排他.
        public : std::condition_variable         scan_condition;      //!< Scan完了待ち.
        public : std::uint64_t                   scan_count;     //!< 完了したScan回数.
    };

    void Keyboard::deleteImpl( Impl* p_in )
    {
        delete p_in;
    }

    KeyboardState Keyboard::snapshot( void ) const
    {
        std::lock_guard< std::mutex > lock( this->m_impl->mutex );
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
        std::shared_ptr< const KeyCallback > owned_callback;
        if( callback_in )
        {
            owned_callback = std::make_shared< const KeyCallback >( std::move( callback_in ) );
        }
        std::lock_guard< std::mutex > lock( this->m_impl->mutex );
        this->m_key_callback = std::move( owned_callback );
    }

    void Keyboard::clearCallback( void ) noexcept
    {
        std::lock_guard< std::mutex > lock( this->m_impl->mutex );
        this->m_key_callback.reset();
    }

    int8_t Keyboard::getASCII( void ) const
    {
        const auto ascii = this->ascii_state();
        for( std::size_t index = 0U; index < ascii.size(); ++index )
        {
            if( ascii[ index ] )
            {
                return static_cast< int8_t >( index );
            }
        }
        return ASCII_NULL;
    }

    bool Keyboard::isReleasedAllKey( void )
    {
        // 呼出し後に完了した新しいScanを1回待つ. Worker停止・Backend不能でも固まらないよう上限を置く.
        {
            std::unique_lock< std::mutex > lock( this->m_impl->mutex );
            const std::uint64_t entry_count = this->m_impl->scan_count;
            Impl* p_impl = this->m_impl.get();
            this->m_impl->scan_condition.wait_for(
                  lock
                , std::chrono::milliseconds( 100 )
                , [ p_impl, entry_count ]() { return p_impl->scan_count != entry_count; } );
        }

        const KeyboardState state = this->snapshot();
        bool pressed = false;
        for( const bool value : state.ascii ){ pressed = pressed || value; }
        for( const bool value : state.function ){ pressed = pressed || value; }
        for( const bool value : state.arrow ){ pressed = pressed || value; }
        for( const bool value : state.command ){ pressed = pressed || value; }
        return !pressed;
    }

    void Keyboard::initialize( void )
    {
        this->m_ascii_state.fill( false );
        this->m_function_state.fill( false );
        this->m_arrow_state.fill( false );
        this->m_lock_state.fill( false );
        this->m_command_state.fill( false );
    }

    Keyboard::Keyboard( void )
        : m_impl           ( new Impl(), &Keyboard::deleteImpl )
        , m_ascii_state    ()
        , m_function_state ()
        , m_arrow_state    ()
        , m_lock_state     ()
        , m_command_state  ()
        , m_key_callback   ()
        , m_access_state   ( KeyboardAccessState::Starting )
    {
        this->initialize();
        this->m_impl->worker.start( [ this ]() { this->monitorLoop(); } );
    }

    Keyboard::~Keyboard( void )
    {
        if( this->m_impl != nullptr )
        {
            this->m_impl->worker.stop();
        }
    }

    Keyboard::Keyboard( Keyboard&& obj_inout ) noexcept
        : m_impl           ( new Impl(), &Keyboard::deleteImpl )
        , m_ascii_state    ()
        , m_function_state ()
        , m_arrow_state    ()
        , m_lock_state     ()
        , m_command_state  ()
        , m_key_callback   ()
        , m_access_state   ( KeyboardAccessState::Starting )
    {
        obj_inout.m_impl->worker.stop();
        {
            std::lock_guard< std::mutex > lock( obj_inout.m_impl->mutex );
            this->m_ascii_state = std::move( obj_inout.m_ascii_state );
            this->m_function_state = std::move( obj_inout.m_function_state );
            this->m_arrow_state = std::move( obj_inout.m_arrow_state );
            this->m_lock_state = std::move( obj_inout.m_lock_state );
            this->m_command_state = std::move( obj_inout.m_command_state );
            this->m_key_callback = std::move( obj_inout.m_key_callback );
        }
        this->m_impl->worker.start( [ this ]() { this->monitorLoop(); } );
    }

    Keyboard& Keyboard::operator = ( Keyboard&& obj ) noexcept
    {
        if( this != &obj )
        {
            this->m_impl->worker.stop();
            obj.m_impl->worker.stop();
            {
                std::scoped_lock lock( this->m_impl->mutex, obj.m_impl->mutex );
                this->m_ascii_state = std::move( obj.m_ascii_state );
                this->m_function_state = std::move( obj.m_function_state );
                this->m_arrow_state = std::move( obj.m_arrow_state );
                this->m_lock_state = std::move( obj.m_lock_state );
                this->m_command_state = std::move( obj.m_command_state );
                this->m_key_callback = std::move( obj.m_key_callback );
            }
            this->m_access_state.store(
                KeyboardAccessState::Starting, std::memory_order_release );
            this->m_impl->worker.start( [ this ]() { this->monitorLoop(); } );
        }
        return *this;
    }

    Keyboard::Keyboard( const Keyboard& obj_in )
        : m_impl           ( new Impl(), &Keyboard::deleteImpl )
        , m_ascii_state    ()
        , m_function_state ()
        , m_arrow_state    ()
        , m_lock_state     ()
        , m_command_state  ()
        , m_key_callback   ()
        , m_access_state   ( KeyboardAccessState::Starting )
    {
        {
            std::lock_guard< std::mutex > lock( obj_in.m_impl->mutex );
            this->m_ascii_state = obj_in.m_ascii_state;
            this->m_function_state = obj_in.m_function_state;
            this->m_arrow_state = obj_in.m_arrow_state;
            this->m_lock_state = obj_in.m_lock_state;
            this->m_command_state = obj_in.m_command_state;
            this->m_key_callback = obj_in.m_key_callback;
        }
        this->m_impl->worker.start( [ this ]() { this->monitorLoop(); } );
    }

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
                KeyboardAccessState::Starting, std::memory_order_release );
            this->m_impl->worker.start( [ this ]() { this->monitorLoop(); } );
        }
        return *this;
    }

    void Keyboard::monitorLoop( void )
    {
        static const std::chrono::milliseconds PROCESS_WAIT{ 5 };
        static const uint64_t RESCAN_INTERVAL_TICKS = 200U;

        Impl& impl = *this->m_impl;
        detail::NativeKeyboardOperations operations;
        detail::LinuxKeyboardDevices<> devices( operations );
        uint64_t rescan_ticks = 0U;

        while( !impl.worker.waitFor( PROCESS_WAIT ) )
        {
            KeyboardAccessState access_state = this->accessState();
            if( rescan_ticks == 0U )
            {
                access_state = devices.scan();
                rescan_ticks = RESCAN_INTERVAL_TICKS;
            }
            else
            {
                --rescan_ticks;
            }

            KeyboardState current{};
            if( access_state == KeyboardAccessState::Ready )
            {
                std::array< bool, KEY_MAX + 1U > keys{};
                std::array< bool, LED_MAX + 1U > leds{};
                access_state = devices.capture( &keys, &leds );
                if( access_state == KeyboardAccessState::Ready )
                {
                    current = mapKeyboardState( keys, leds );
                }
                else
                {
                    rescan_ticks = RESCAN_INTERVAL_TICKS;
                }
            }
            this->m_access_state.store( access_state, std::memory_order_release );

            KeyboardState previous{};
            std::shared_ptr< const KeyCallback > callback;
            {
                std::lock_guard< std::mutex > lock( impl.mutex );
                previous = KeyboardState{
                    this->m_ascii_state,
                    this->m_function_state,
                    this->m_arrow_state,
                    this->m_lock_state,
                    this->m_command_state
                };
                this->m_ascii_state = current.ascii;
                this->m_function_state = current.function;
                this->m_arrow_state = current.arrow;
                this->m_lock_state = current.lock;
                this->m_command_state = current.command;
                callback = this->m_key_callback;
            }

            if( statesDiffer( previous, current ) && callback != nullptr &&
                !impl.worker.isStopRequested() )
            {
                try
                {
                    ( *callback )( current );
                }
                catch( ... )
                {
                    this->clearCallback();
                }
            }

            // Scan完了を通知する. isReleasedAllKey()が新しいScanを1回待つために使う.
            {
                std::lock_guard< std::mutex > lock( impl.mutex );
                ++impl.scan_count;
            }
            impl.scan_condition.notify_all();
        }
    }
}
}

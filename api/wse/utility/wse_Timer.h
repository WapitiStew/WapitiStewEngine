//*****************************************************************************************************************
//!
//! @file    wse_Timer.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-13, 2026   Replace the legacy worker-base inheritance with an owned worker (legacy removal program, LEGACY-006).
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 指定間隔でCallbackを呼び出すTimerの定義。
//!     \~english  Definition of the timer invoking a callback at a fixed interval.
//!
//! @details
//!     \~japanese
//!         TimerはWorker Threadを内部に所有するfinalなClassである。Thread・Atomic・Mutexおよび
//!         Generic Worker APIを公開しない。Lifecycleは`start()`／`stop()`／`isRunning()`だけで
//!         あり、間隔は`setInterval()`で実行中にも変更できる。
//!     \~english
//!         Timer is a final class owning its worker thread internally. It exposes no thread,
//!         atomic, mutex, or generic worker API. The lifecycle is `start()` / `stop()` /
//!         `isRunning()`, and `setInterval()` changes the interval, also while running.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#ifndef WONDERSTEWENGINE_UTILITY_TIMER_H
#define WONDERSTEWENGINE_UTILITY_TIMER_H

#include <chrono>
#include <functional>
#include <memory>

#include "../../dynamic.h"

// Warning Disable.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251)  // Dynamicライブラリでユーザー側に公開されないメンバーが含まれる
#pragma warning(disable: 4514)  // 参照されていないインライン関数は削除の警告.
#pragma warning(disable: 4820)  // 参照されていないインライン関数は削除の警告.
#endif

namespace  wse
{
    //!
    //!
    //! @class  Timer
    //! @brief
    //!     \~japanese 指定した間隔でCallbackを呼び出すTimer。Worker Threadを内部に所有する。
    //!     \~english  Timer invoking a callback at a fixed interval on an internally owned worker thread.
    //!
    //! @details
    //!     \~japanese
    //!         @n Callbackは`start()`で束ねられ、TimerがCaptureを所有する。停止すると解放される。
    //!         @n Callback内から`stop()`を呼べる。CallbackがExceptionを送出した場合、Exceptionは
    //!         Thread境界を越えず、Timerは停止する。
    //!         @n Copy不可・Move可能。Moveは移動元を停止させ、移動先は停止状態から始まる。
    //!         @n Callback実行中のDestructは、Callbackの完了を待ってからThreadをjoinする。
    //!     \~english
    //!         @n The callback is bound by `start()` and its captures are owned by the timer;
    //!         they are released when the timer stops.
    //!         @n `stop()` may be called from inside the callback. An exception thrown by the
    //!         callback never crosses the thread boundary; the timer stops.
    //!         @n Non-copyable, movable. A move stops the source, and the destination starts
    //!         in the stopped state.
    //!         @n Destruction during a callback waits for the callback to finish, then joins.
    //!
    class WSE_API Timer final
    {
        public : using TimerCallback = std::function<void( void )>;

        //-----------------------------------
        // Member
        //-----------------------------------
        private : class Impl;
        private : static void deleteImpl( Impl* p_in );
        private : using ImplPtr = std::unique_ptr< Impl, void (*)( Impl* ) >;
        private : ImplPtr m_impl;   //!< \~japanese 内部実装. \~english The internal implementation.

        //-----------------------------------
        // Lifecycle
        //-----------------------------------

        //!
        //! @brief
        //!     \~japanese TimerのWorkerを起動し、間隔ごとのCallback呼び出しを開始する。
        //!     \~english  Starts the worker and begins invoking the callback every interval.
        //!
        //! @param [in] interval_in  \~japanese Callback間隔. 正の値であること.
        //!                          \~english  The callback interval; must be positive.
        //! @param [in] callback_in  \~japanese 呼び出すCallback. 空でないこと. CaptureはTimerが所有する.
        //!                          \~english  The callback to invoke; must not be empty. Captures are owned by the timer.
        //! @return
        //!     \~japanese 起動できた場合true. 既に実行中の場合false.
        //!     \~english  True when started; false while already running.
        //!
        //! @note
        //!     \~japanese 0以下の間隔と空のCallbackは`std::invalid_argument`で拒否する.
        //!     \~english  A non-positive interval and an empty callback are rejected with `std::invalid_argument`.
        //!
        public : bool start(
                const std::chrono::milliseconds interval_in,
                TimerCallback callback_in
        );

        //!
        //! @brief
        //!     \~japanese Timerを停止し、Workerをjoinする。冪等であり、Callback内からも呼べる。
        //!     \~english  Stops the timer and joins the worker; idempotent and callable from the callback.
        //!
        //! @note
        //!     \~japanese Callback内からの呼び出しは停止要求のみを行い、joinはDestructorまたは
        //!     次の`start()`が引き受ける。復帰後に新しいCallback呼び出しは開始されない。
        //!     \~english  Called from the callback it only requests the stop; the join is taken
        //!     over by the destructor or the next `start()`. No new invocation begins after it returns.
        //!
        public : void stop( void ) noexcept;

        //!
        //! @return
        //!     \~japanese Workerが実行中の場合true.
        //!     \~english  True while the worker is running.
        //!
        public : bool isRunning( void ) const noexcept;

        //!
        //! @brief
        //!     \~japanese Callback間隔を変更する。次の待機から有効になる。
        //!     \~english  Changes the callback interval, effective from the next wait.
        //!
        //! @param [in] interval_in  \~japanese 新しい間隔. 正の値であること.
        //!                          \~english  The new interval; must be positive.
        //!
        //! @note
        //!     \~japanese 0以下の間隔は`std::invalid_argument`で拒否する.
        //!     \~english  A non-positive interval is rejected with `std::invalid_argument`.
        //!
        public : void setInterval( const std::chrono::milliseconds interval_in );

        //!
        //! @return
        //!     \~japanese 現在のCallback間隔.
        //!     \~english  The current callback interval.
        //!
        public : std::chrono::milliseconds interval( void ) const noexcept;

        //-----------------------------------
        // Constructor/Destructor
        //-----------------------------------

        //!
        //! @brief Default constructor.
        //!
        public : Timer( void );

        //!
        //! @brief
        //!     \~japanese Timerを停止してWorkerをjoinし、破棄する。
        //!     \~english  Stops the timer, joins the worker, and destroys it.
        //!
        public : ~Timer( void );

        public : Timer( const Timer& ) = delete;
        public : Timer& operator = ( const Timer& ) = delete;

        //!
        //! @brief  Move constructor.
        //! @note
        //!     \~japanese 移動元を停止させる。移動先は停止状態から始まる。
        //!     \~english  Stops the source; the destination starts stopped.
        //!
        public : Timer( Timer &&obj_inout )noexcept;

        //!
        //! @brief Move assignment operator.
        //!
        public : Timer& operator = ( Timer &&obj_inout )noexcept;
    };
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif  //WONDERSTEWENGINE_UTILITY_TIMER_H

//*****************************************************************************************************************
//!
//! @file    wse_WorkerController.h
//! @brief   \~japanese 内部Worker Threadの生存期間・停止要求・待機を所有するControllerを定義する.
//! @brief   \~english  Defines the internal controller owning a worker thread's lifetime, stop request, and waits.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-13, 2026   Create the internal worker controller (legacy removal program, LEGACY-006 PR 5-A).
//!
//! @details
//!     \~japanese
//!         本HeaderはInstall Packageへ含まれない内部実装である. 公開APIへThread・Atomic・Mutexを
//!         露出したLegacyのBMultiThreadを置き換えるため、Thread所有・Start／Stop・停止要求・join・
//!         Worker内からのStop・割込み可能な待機・Exception containmentだけをここへ閉じ込める.
//!         Task状態機械や事象Callbackは持たない. それらは利用側（Timer／Keyboard）の責務である.
//!     \~english
//!         This header is internal and never installed. It replaces the legacy BMultiThread, which
//!         leaked threads, atomics, and mutexes through the public API, by confining thread
//!         ownership, start/stop, the stop request, join, stop-from-worker, interruptible waiting,
//!         and exception containment here. There is no task state machine and no event callback;
//!         those belong to the owning feature (Timer / Keyboard).
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#pragma once

#ifndef WONDERSTEWENGINE_CORE_UTILITY_WORKERCONTROLLER_H
#define WONDERSTEWENGINE_CORE_UTILITY_WORKERCONTROLLER_H

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace wse
{
namespace detail
{
    //!
    //! @class WorkerController
    //!
    //! @brief
    //!     \~japanese 1本のWorker Threadを所有し、Start／Stop・停止要求・割込み可能な待機を提供する.
    //!     \~english  Owns one worker thread and provides start/stop, the stop request, and interruptible waits.
    //!
    //! @details
    //!     \~japanese
    //!         Threadは常に本Controllerだけが所有し、外部へPointerを渡さない. Worker本体は
    //!         `start()`へ渡された関数で、次の形で停止要求へ協調する.
    //!         @n `while( !controller.waitFor( interval ) ) { tick(); }`
    //!         @n `waitFor()`は条件変数で待つため、`requestStop()`は待機中のWorkerを即座に起こす.
    //!         Worker本体が送出したExceptionはThread境界を越えず、Workerは停止として終了する.
    //!         `stop()`はWorker Thread自身から呼ばれた場合はjoinせず停止要求だけを行い、joinは
    //!         Destructorまたは次の`start()`が引き受ける. Destructorは停止を要求してjoinする.
    //!     \~english
    //!         The thread is owned exclusively by this controller and no pointer to it ever leaves.
    //!         The worker body is the function handed to `start()` and cooperates with the stop
    //!         request as
    //!         @n `while( !controller.waitFor( interval ) ) { tick(); }`
    //!         @n `waitFor()` blocks on a condition variable, so `requestStop()` wakes a waiting
    //!         worker immediately. An exception thrown by the body never crosses the thread
    //!         boundary; the worker ends as stopped. `stop()` called from the worker thread itself
    //!         only requests the stop and leaves the join to the destructor or the next `start()`;
    //!         the destructor requests the stop and joins.
    //!
    //! @note
    //!     \~japanese CopyもMoveも不可. 所有者のPIMPL内に固定Memberとして置く.
    //!     \~english  Neither copyable nor movable; owners embed it inside their PIMPL.
    //!
    class WorkerController final
    {
        private : mutable std::mutex            m_mutex;            //!< \~japanese 内部状態の排他.            \~english Guards the internal state.
        private : mutable std::condition_variable m_wake;           //!< \~japanese 停止要求で待機を起こす.    \~english Wakes waits on a stop request.
        private : std::thread                   m_thread;           //!< \~japanese 所有するWorker Thread.     \~english The owned worker thread.
        private : std::thread::id               m_worker_id;        //!< \~japanese WorkerのThread ID.         \~english The worker's thread id.
        private : bool                          m_running;//!< \~japanese Worker本体が実行中の場合true. \~english True while the body runs.
        private : bool                          m_stop_requested; //!< \~japanese 停止要求済みの場合true. \~english True once a stop is requested.

        public : WorkerController( void )
            : m_mutex          ()
            , m_wake           ()
            , m_thread         ()
            , m_worker_id      ()
            , m_running        ( false )
            , m_stop_requested ( false )
        {
        }
        //!
        //! @brief
        //!     \~japanese 停止を要求し、Workerをjoinして破棄する.
        //!     \~english  Requests the stop, joins the worker, and destroys the controller.
        //!
        public : ~WorkerController( void );

        public : WorkerController( const WorkerController& ) = delete;
        public : WorkerController& operator = ( const WorkerController& ) = delete;
        public : WorkerController( WorkerController&& ) = delete;
        public : WorkerController& operator = ( WorkerController&& ) = delete;

        //!
        //! @brief
        //!     \~japanese Worker Threadを起動し、本体関数を実行する.
        //!     \~english  Starts the worker thread and runs the body.
        //!
        //! @param [in] body_in  \~japanese Worker本体. 停止要求へ協調する関数.
        //!                      \~english  The worker body; must cooperate with the stop request.
        //! @return
        //!     \~japanese 起動できた場合true. 実行中、または本体が空の場合false.
        //!     \~english  True when started; false while already running or when the body is empty.
        //!
        //! @note
        //!     \~japanese 前回のWorkerが停止済みであればjoinしてから再起動する.
        //!     \~english  A finished previous worker is joined before the restart.
        //!
        public : bool start( std::function< void( void ) > body_in );

        //!
        //! @brief
        //!     \~japanese 停止を要求し、待機中のWorkerを起こす. joinしない.
        //!     \~english  Requests the stop and wakes a waiting worker without joining.
        //!
        public : void requestStop( void ) noexcept;

        //!
        //! @brief
        //!     \~japanese 停止を要求してWorkerをjoinする. Worker Thread自身からは要求のみを行う.
        //!     \~english  Requests the stop and joins; called from the worker thread it only requests.
        //!
        //! @note
        //!     \~japanese 冪等であり、未起動でも安全に呼べる.
        //!     \~english  Idempotent and safe before any start.
        //!
        public : void stop( void ) noexcept;

        //!
        //! @return
        //!     \~japanese Worker本体が実行中の場合true.
        //!     \~english  True while the worker body is running.
        //!
        public : bool isRunning( void ) const noexcept;

        //!
        //! @return
        //!     \~japanese 現在のWorkerへ停止要求済みの場合true.
        //!     \~english  True once the current worker has been asked to stop.
        //!
        public : bool isStopRequested( void ) const noexcept;

        //!
        //! @brief
        //!     \~japanese 停止要求まで、最長で指定時間待つ.
        //!     \~english  Waits up to the duration or until a stop is requested.
        //!
        //! @param [in] duration_in  \~japanese 待機時間.  \~english The wait duration.
        //! @return
        //!     \~japanese 停止要求があった場合true. 時間切れはfalse.
        //!     \~english  True when the stop was requested; false on timeout.
        //!
        public : bool waitFor( const std::chrono::milliseconds duration_in ) const;
    };

} // namespace detail
} // namespace wse

#endif // WONDERSTEWENGINE_CORE_UTILITY_WORKERCONTROLLER_H

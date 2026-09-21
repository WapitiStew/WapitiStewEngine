//*****************************************************************************************************************
//!
//! @file    wse_WorkerController.cpp
//! @brief   \~japanese 内部Worker Controllerの実装.
//! @brief   \~english  Implements the internal worker controller.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-13, 2026   Create the internal worker controller (legacy removal program, LEGACY-006 PR 5-A).
//!
//! @details
//!     \~japanese 依存は標準Libraryだけに保つ. Log・Exception・他のWSE型へ依存しない.
//!     \~english  Depends only on the standard library: no logging, no WSE exception, no other WSE type.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#include "wse_WorkerController.h"

#include <utility>

namespace wse
{
namespace detail
{
    WorkerController::~WorkerController( void )
    {
        this->stop();
    }

    bool WorkerController::start( std::function< void( void ) > body_in )
    {
        if( !body_in )
        {
            return false;
        }

        std::thread finished_worker;  //!< \~japanese joinする停止済みWorker. \~english A finished worker to join.
        {
            std::lock_guard< std::mutex > lock( this->m_mutex );
            if( this->m_running )
            {
                return false;
            }
            // 前回のWorkerが停止済みならここで引き取る. Lockの外でjoinする.
            finished_worker = std::move( this->m_thread );
        }
        if( finished_worker.joinable() )
        {
            finished_worker.join();
        }

        std::lock_guard< std::mutex > lock( this->m_mutex );
        if( this->m_running || this->m_thread.joinable() )
        {
            // 並行するstart()が先に起動した.
            return false;
        }
        this->m_stop_requested = false;
        this->m_running        = true;
        this->m_thread         = std::thread( [ this, body = std::move( body_in ) ]( void )
        {
            {
                std::lock_guard< std::mutex > worker_lock( this->m_mutex );
                this->m_worker_id = std::this_thread::get_id();
            }
            try
            {
                body();
            }
            catch( ... )
            {
                // Worker本体のExceptionはThread境界を越えない. Workerは停止として終了する.
            }
            {
                std::lock_guard< std::mutex > worker_lock( this->m_mutex );
                this->m_running   = false;
                this->m_worker_id = std::thread::id();
            }
            this->m_wake.notify_all();
        } );
        this->m_worker_id = this->m_thread.get_id();
        return true;
    }

    void WorkerController::requestStop( void ) noexcept
    {
        {
            std::lock_guard< std::mutex > lock( this->m_mutex );
            this->m_stop_requested = true;
        }
        this->m_wake.notify_all();
    }

    void WorkerController::stop( void ) noexcept
    {
        this->requestStop();

        std::thread worker;  //!< \~japanese joinするWorker. \~english The worker to join.
        {
            std::lock_guard< std::mutex > lock( this->m_mutex );
            if( !this->m_thread.joinable()
                || ( this->m_worker_id == std::this_thread::get_id() ) )
            {
                // Worker自身からのstop()は要求のみ. joinはDestructorまたは次のstart()が行う.
                return;
            }
            worker = std::move( this->m_thread );
        }
        worker.join();
    }

    bool WorkerController::isRunning( void ) const noexcept
    {
        std::lock_guard< std::mutex > lock( this->m_mutex );
        return this->m_running;
    }

    bool WorkerController::isStopRequested( void ) const noexcept
    {
        std::lock_guard< std::mutex > lock( this->m_mutex );
        return this->m_stop_requested;
    }

    bool WorkerController::waitFor( const std::chrono::milliseconds duration_in ) const
    {
        std::unique_lock< std::mutex > lock( this->m_mutex );
        const WorkerController* p_self = this;
        return this->m_wake.wait_for(
              lock
            , duration_in
            , [ p_self ]( void ) { return p_self->m_stop_requested; } );
    }

} // namespace detail
} // namespace wse

//*****************************************************************************************************************
//!
//! @file    wse_Timer.cpp
//! @brief   \~japanese 所有Worker上でCallbackを周期呼び出しするTimerの実装.
//! @brief   \~english  Implements Timer, which invokes a callback periodically on its owned worker.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-13, 2026   Replace the BMultiThread inheritance with the internal worker controller (LEGACY-006).
//!   Feb-17, 2025   Create New
//!
//!
//! @details
//!     \~japanese
//!         Worker Lifecycleは`wse::detail::WorkerController`が所有する. 停止要求は待機中のWorkerを
//!         即座に起こすため、旧実装のような間隔の分割Pollingを行わない. CallbackのExceptionは
//!         Thread境界を越えず、Timerを停止させる.
//!     \~english
//!         The worker lifecycle is owned by `wse::detail::WorkerController`. A stop request wakes
//!         a waiting worker immediately, so the legacy subdivided polling of the interval is gone.
//!         A callback exception never crosses the thread boundary and stops the timer.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#endif
#include <utility>
#include "../common.h"
#include "wse_WorkerController.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <stdexcept>

namespace  wse
{
    class Timer::Impl
    {
        //! @brief Construct all members with explicit defaults.
public:
        Impl()
            : worker   ()
            , mutex    ()
            , interval ( 1 )
            , callback ()
        {
        }
private:

        public : detail::WorkerController                    worker;            //!< Worker Lifecycleの所有者.
        public : mutable std::mutex                          mutex;             //!< 設定とCallbackの排他.
        public : std::chrono::milliseconds                   interval;     //!< Callback間隔.
        public : std::shared_ptr< const TimerCallback >      callback;          //!< 所有Callback.
    };

    void Timer::deleteImpl( Impl* p_in )
    {
        delete p_in;
    }

    bool Timer::start(
            const std::chrono::milliseconds interval_in,
            TimerCallback callback_in
    )
    {
        if( interval_in <= std::chrono::milliseconds::zero() )
        {
            throw std::invalid_argument( "Timer::start requires a positive interval" );
        }
        if( !callback_in )
        {
            throw std::invalid_argument( "Timer::start requires a callback" );
        }

        Impl& impl = *this->m_impl;
        Impl* p_impl = this->m_impl.get();

        // start全体をImpl Mutexで直列化する. Worker本体の設定読取りも同じMutexを使うため、
        // Workerは設定が確定するまでここでBlockされる.
        std::lock_guard< std::mutex > lock( impl.mutex );
        if( impl.worker.isRunning() )
        {
            return false;
        }

        const bool started = impl.worker.start( [ p_impl ]( void )
        {
            for( ;; )
            {
                std::chrono::milliseconds interval{ 1 };
                {
                    std::lock_guard< std::mutex > interval_lock( p_impl->mutex );
                    interval = p_impl->interval;
                }
                if( p_impl->worker.waitFor( interval ) )
                {
                    break;
                }

                std::shared_ptr< const TimerCallback > callback;
                {
                    std::lock_guard< std::mutex > callback_lock( p_impl->mutex );
                    callback = p_impl->callback;
                }
                if( callback == nullptr )
                {
                    break;
                }
                try
                {
                    ( *callback )();
                }
                catch( ... )
                {
                    // CallbackのExceptionはThread境界を越えない. Timerは停止する.
                    break;
                }
            }

            // 停止時にCaptureを解放する. 所有権はTimerにあり、停止後は保持しない.
            std::lock_guard< std::mutex > worker_lock( p_impl->mutex );
            p_impl->callback.reset();
        } );

        if( started )
        {
            impl.interval = interval_in;
            impl.callback = std::make_shared< const TimerCallback >( std::move( callback_in ) );
        }
        return started;
    }

    void Timer::stop( void ) noexcept
    {
        this->m_impl->worker.stop();
    }

    bool Timer::isRunning( void ) const noexcept
    {
        return this->m_impl->worker.isRunning();
    }

    void Timer::setInterval( const std::chrono::milliseconds interval_in )
    {
        if( interval_in <= std::chrono::milliseconds::zero() )
        {
            throw std::invalid_argument( "Timer::setInterval requires a positive interval" );
        }
        std::lock_guard< std::mutex > lock( this->m_impl->mutex );
        this->m_impl->interval = interval_in;
    }

    std::chrono::milliseconds Timer::interval( void ) const noexcept
    {
        std::lock_guard< std::mutex > lock( this->m_impl->mutex );
        return this->m_impl->interval;
    }

    //!
    //! @brief  デフォルトコンストラクタ.
    //!
    Timer::Timer( void )
        : m_impl ( new Impl(), &Timer::deleteImpl )
    {
    }
    //!
    //! @brief Destructor.
    //!
    Timer::~Timer( void )
    {
        if( this->m_impl != nullptr )
        {
            this->m_impl->worker.stop();
        }
    }

    //!
    //! @brief ムーブコンストラクタ. 移動元を停止させ、間隔だけを引き継ぐ.
    //!
    Timer::Timer( Timer &&obj_inout )noexcept
        : Timer ()
    {
        obj_inout.stop();
        std::scoped_lock lock( this->m_impl->mutex, obj_inout.m_impl->mutex );
        this->m_impl->interval = obj_inout.m_impl->interval;
    }

    //!
    //! @brief ムーブオペレーター. 双方を停止させ、間隔だけを引き継ぐ.
    //!
    Timer& Timer::operator = ( Timer &&obj_inout )noexcept
    {
        if( this != &obj_inout )
        {
            this->stop();
            obj_inout.stop();
            std::scoped_lock lock( this->m_impl->mutex, obj_inout.m_impl->mutex );
            this->m_impl->interval = obj_inout.m_impl->interval;
        }
        return *this;
    }
};

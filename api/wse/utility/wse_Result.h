//*****************************************************************************************************************
//!
//! @file    wse_Result.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-08, 2026   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 全Componentが共有する正準のResult／Status／PartialResult契約を定義するファイル。
//!     \~english  Defines the canonical Result / Status / PartialResult contract shared by every component.
//!
//! @details
//!     \~japanese
//!         設計は doc/design/ja/ResultContract.md が正である。Result は成功時に値だけを、失敗時に
//!         Errorだけを保持し、Factory以外の構築手段を持たない。Status は値なしOperationの成否で、
//!         無意味なbool Payloadを持たない。PartialResult は「Errorと並んで値が意味を持つ」と文書化
//!         された部分進捗Operation (転送のByte数、HTTPの4xx/5xx応答) 専用の契約である。
//!     \~english
//!         The normative design is doc/design/en/ResultContract.md. A Result holds only a value on
//!         success and only an error on failure, and cannot be built except through its factories.
//!         A Status is the outcome of a value-less operation and carries no meaningless bool
//!         payload. A PartialResult is the dedicated contract for partial-progress operations
//!         (transfer byte counts, HTTP 4xx/5xx responses) whose value is documented to stay
//!         meaningful alongside the error.
//!
//! @note
//!     \~japanese Error型 E は既定構築で ok() == true となり、category() / code() / message() /
//!     nativeCode() / ok() を公開すること。
//!     \~english  The error type E must be default-constructible with ok() == true and expose
//!     category() / code() / message() / nativeCode() / ok().
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
#pragma once

#ifndef WONDERSTEWENGINE_UTILITY_RESULT_H
#define WONDERSTEWENGINE_UTILITY_RESULT_H

#include "../../dynamic.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace wse
{
    //!
    //! @class ResultAccessError
    //!
    //! @brief
    //!     \~japanese Result／Statusの前提条件違反を報告するException。
    //!     \~english  Exception reporting a Result / Status precondition violation.
    //!
    //! @details
    //!     \~japanese
    //!         成功Resultのerror()、失敗Resultのvalue()、Errorなしのfailure()といった呼び出し側の
    //!         契約違反だけが投げる。Operationの失敗はErrorとして値で返り、この型では報告しない。
    //!     \~english
    //!         Thrown only for caller contract violations: error() on a successful result, value()
    //!         on a failed one, or failure() without an error. An operation failure is returned as
    //!         an error value and never reported through this type.
    //!
    class ResultAccessError final : public std::logic_error
    {
        //!
        //! @brief
        //!     \~japanese 違反内容を示す理由文で構築する。
        //!     \~english  Builds the error from the reason describing the violation.
        //!
        //! @param [in] reason_in  Human-readable description of the violated precondition.
        //!
        public : explicit ResultAccessError( const std::string& reason_in )
            : std::logic_error ( reason_in )
        {
        }
    };

    //!
    //! @class Result
    //!
    //! @brief
    //!     \~japanese 成功なら値だけ、失敗ならErrorだけを保持する正準Result。
    //!     \~english  The canonical result holding only a value on success and only an error on failure.
    //!
    //! @details
    //!     \~japanese
    //!         公開ConstructorはなくFactoryだけがある。failure() は空のErrorを拒否するため、
    //!         「Errorなしの失敗」も「Errorありの成功」も構築できない。前提条件に反した
    //!         value() / error() の読み出しは ResultAccessError を投げ、違反を無音にしない。
    //!     \~english
    //!         There is no public constructor, only the factories. failure() rejects an empty
    //!         error, so neither an error-less failure nor an error-carrying success can exist.
    //!         Reading value() / error() against its precondition throws ResultAccessError so a
    //!         violation is loud rather than silent.
    //!
    template< typename T, typename E >
    class [[nodiscard]] Result final
    {
        private : std::optional< T > m_value;   //!< \~english Present exactly when the result succeeded. \~japanese 成功時にだけ値を持つ。
        private : E                  m_error;   //!< \~english Meaningful exactly when the result failed.  \~japanese 失敗時にだけ意味を持つ。

        private : Result( void ) = default;

        //!
        //! @brief
        //!     \~japanese 成功Resultを生成する。
        //!     \~english  Builds a successful result.
        //!
        //! @param [in] value_in  Success value.
        //! @return Successful result holding value_in.
        //!
        public : static Result success( T value_in )
        {
            Result result;
            result.m_value.emplace( std::move( value_in ) );
            return result;
        }

        //!
        //! @brief
        //!     \~japanese 失敗Resultを生成する。
        //!     \~english  Builds a failed result.
        //!
        //! @param [in] error_in  Operation error. Must not be ok().
        //! @return Failed result holding error_in.
        //!
        //! @note
        //!     \~japanese error_in.ok() が真 (Errorなし) の場合は ResultAccessError を投げる。
        //!     Errorなしの失敗は存在できない。
        //!     \~english  Throws ResultAccessError when error_in.ok() is true; an error-less
        //!     failure cannot exist.
        //!
        public : static Result failure( E error_in )
        {
            if( error_in.ok() )
            {
                throw ResultAccessError( "Result::failure requires a non-empty error" );
            }
            Result result;
            result.m_error = std::move( error_in );
            return result;
        }

        //!
        //! @brief
        //!     \~japanese 成功したかどうかを返す。
        //!     \~english  Whether the operation succeeded.
        //!
        //! @return true when the result holds a value.
        //!
        public : [[nodiscard]] bool succeeded( void ) const noexcept { return this->m_value.has_value(); }

        //!
        //! @brief
        //!     \~japanese 成功値への参照を返す。
        //!     \~english  Returns a reference to the success value.
        //!
        //! @return Reference to the success value.
        //!
        //! @note
        //!     \~japanese succeeded() が前提条件。失敗Resultでは保持するErrorのCategory／Code／
        //!     Messageを載せた ResultAccessError を投げる。非throwの読み出しは valueOr() を使う。
        //!     \~english  Requires succeeded(). On a failed result throws ResultAccessError
        //!     carrying the stored error's category, code, and message. valueOr() is the
        //!     non-throwing read.
        //!
        public : const T& value( void ) const
        {
            this->requireValue();
            return *this->m_value;
        }

        //!
        //! @brief
        //!     \~japanese 変更可能な成功値への参照を返す。
        //!     \~english  Returns a mutable reference to the success value.
        //!
        //! @return Mutable reference to the success value.
        //!
        //! @note
        //!     \~japanese succeeded() が前提条件。違反時の挙動はconst版と同じ。
        //!     \~english  Requires succeeded(); a violation behaves as the const overload.
        //!
        public : T& value( void )
        {
            this->requireValue();
            return *this->m_value;
        }

        //!
        //! @brief
        //!     \~japanese 成功なら値を、失敗なら代替値を返す。
        //!     \~english  Returns the value on success, or the fallback on failure.
        //!
        //! @param [in] fallback_in  Value returned when the result failed.
        //! @return Success value or fallback_in.
        //!
        public : T valueOr( T fallback_in ) const
        {
            return this->succeeded() ? *this->m_value : std::move( fallback_in );
        }

        //!
        //! @brief
        //!     \~japanese Operation Errorへの参照を返す。
        //!     \~english  Returns a reference to the operation error.
        //!
        //! @return Reference to the error.
        //!
        //! @note
        //!     \~japanese !succeeded() が前提条件。成功Resultでは ResultAccessError を投げる。
        //!     \~english  Requires !succeeded(); on a successful result throws ResultAccessError.
        //!
        public : const E& error( void ) const
        {
            if( this->succeeded() )
            {
                throw ResultAccessError( "Result::error read on a successful result" );
            }
            return this->m_error;
        }

        //! \~japanese value() の前提条件検査。失敗時はErrorの識別情報を理由文へ載せる。
        //! \~english  Precondition check for value(); the reason carries the error identity.
        private : void requireValue( void ) const
        {
            if( !this->succeeded() )
            {
                const std::string reason = "Result::value read on a failed result: category="
                    + std::to_string( static_cast< long long >( this->m_error.category() ) )
                    + " code=" + std::to_string( static_cast< long long >( this->m_error.code() ) )
                    + " message=" + this->m_error.message();
                throw ResultAccessError( reason );
            }
        }
    };

    //!
    //! @class Result<void, E>
    //!
    //! @brief
    //!     \~japanese 値なしOperationの成否。無意味なbool Payloadを持たないStatusの実体。
    //!     \~english  Outcome of a value-less operation; the Status shape without a meaningless bool payload.
    //!
    template< typename E >
    class [[nodiscard]] Result< void, E > final
    {
        private : bool m_succeeded; //!< \~english True when the operation succeeded. \~japanese 成功したとき真。
        private : E    m_error;     //!< \~english Meaningful exactly when failed.     \~japanese 失敗時にだけ意味を持つ。

        private : explicit Result( const bool succeeded_in )
            : m_succeeded ( succeeded_in )
            , m_error     ()
        {
        }
        //!
        //! @brief
        //!     \~japanese 成功Statusを生成する。
        //!     \~english  Builds a successful status.
        //!
        //! @return Successful status.
        //!
        public : static Result success( void ) { return Result( true ); }

        //!
        //! @brief
        //!     \~japanese 失敗Statusを生成する。
        //!     \~english  Builds a failed status.
        //!
        //! @param [in] error_in  Operation error. Must not be ok().
        //! @return Failed status holding error_in.
        //!
        //! @note
        //!     \~japanese error_in.ok() が真の場合は ResultAccessError を投げる。
        //!     \~english  Throws ResultAccessError when error_in.ok() is true.
        //!
        public : static Result failure( E error_in )
        {
            if( error_in.ok() )
            {
                throw ResultAccessError( "Status::failure requires a non-empty error" );
            }
            Result result( false );
            result.m_error = std::move( error_in );
            return result;
        }

        //!
        //! @brief
        //!     \~japanese 成功したかどうかを返す。
        //!     \~english  Whether the operation succeeded.
        //!
        //! @return true when the operation succeeded.
        //!
        public : [[nodiscard]] bool succeeded( void ) const noexcept { return this->m_succeeded; }

        //!
        //! @brief
        //!     \~japanese Operation Errorへの参照を返す。
        //!     \~english  Returns a reference to the operation error.
        //!
        //! @return Reference to the error.
        //!
        //! @note
        //!     \~japanese !succeeded() が前提条件。成功Statusでは ResultAccessError を投げる。
        //!     \~english  Requires !succeeded(); on a successful status throws ResultAccessError.
        //!
        public : const E& error( void ) const
        {
            if( this->m_succeeded )
            {
                throw ResultAccessError( "Status::error read on a successful status" );
            }
            return this->m_error;
        }
    };

    //! \~japanese 値なしOperationの成否の別名。 \~english Alias for the outcome of a value-less operation.
    template< typename E >
    using Status = Result< void, E >;

    //!
    //! @class PartialResult
    //!
    //! @brief
    //!     \~japanese Errorと並んで値が意味を持つ、部分進捗Operation専用の契約。
    //!     \~english  Dedicated contract for partial-progress operations whose value stays meaningful alongside the error.
    //!
    //! @details
    //!     \~japanese
    //!         Byteを送った後に失敗する転送や、4xx/5xx応答で完了するHTTP交換のための形である。
    //!         汎用Resultと違い、値は常に存在し、失敗時にも読める。succeeded() はErrorが空で
    //!         あることを意味するだけで、値の有無を意味しない。
    //!     \~english
    //!         The shape for a transfer that moved bytes before failing and for an HTTP exchange
    //!         that completed with a 4xx/5xx. Unlike the general Result the value always exists
    //!         and stays readable on failure; succeeded() only means the error is empty, never
    //!         whether a value exists.
    //!
    template< typename T, typename E >
    class [[nodiscard]] PartialResult final
    {
        private : T m_value;   //!< \~english Progress or response value, always present. \~japanese 常に存在する進捗または応答の値。
        private : E m_error;   //!< \~english Empty on full success.                      \~japanese 完全成功時は空。

        //!
        //! @brief
        //!     \~japanese 完全成功の部分進捗Resultを生成する。
        //!     \~english  Builds a fully successful partial result.
        //!
        //! @param [in] value_in  Progress or response value.
        //!
        public : explicit PartialResult( T value_in )
            : m_value ( std::move( value_in ) )
            , m_error ()
        {
        }
        //!
        //! @brief
        //!     \~japanese 値とErrorを同時に運ぶ部分進捗Resultを生成する。
        //!     \~english  Builds a partial result carrying the value and the error together.
        //!
        //! @param [in] value_in  Progress or response value, meaningful alongside the error.
        //! @param [in] error_in  Operation error; empty means full success.
        //!
        public : PartialResult( T value_in, E error_in )
            : m_value ( std::move( value_in ) )
            , m_error ( std::move( error_in ) )
        {
        }
        //!
        //! @brief
        //!     \~japanese Errorなしで完了したかどうかを返す。
        //!     \~english  Whether the operation completed without an error.
        //!
        //! @return true when the error is empty.
        //!
        public : [[nodiscard]] bool succeeded( void ) const noexcept { return this->m_error.ok(); }

        //!
        //! @brief
        //!     \~japanese 進捗または応答の値への参照を返す。失敗時も意味を持つ。
        //!     \~english  Returns the progress or response value, meaningful on failure too.
        //!
        //! @return Reference to the value.
        //!
        public : const T& value( void ) const noexcept { return this->m_value; }

        //!
        //! @brief
        //!     \~japanese 変更可能な値への参照を返す。
        //!     \~english  Returns a mutable reference to the value.
        //!
        //! @return Mutable reference to the value.
        //!
        public : T& value( void ) noexcept { return this->m_value; }

        //!
        //! @brief
        //!     \~japanese Operation Errorへの参照を返す。空なら完全成功である。
        //!     \~english  Returns the operation error; empty means full success.
        //!
        //! @return Reference to the error.
        //!
        public : const E& error( void ) const noexcept { return this->m_error; }
    };
};

#endif //WONDERSTEWENGINE_UTILITY_RESULT_H

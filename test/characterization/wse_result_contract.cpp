// @file engine/wse/test/characterization/wse_result_contract.cpp
// @brief 正準Result／Status／PartialResult契約 (doc/design/en/ResultContract.md) を固定する。
// @details ここで固定するのは型が構築を禁じる状態と、前提条件違反が無音にならないことである.
//          Errorなしの失敗はFactoryが拒否し、成功Resultのerror()と失敗Resultのvalue()は
//          ResultAccessErrorで顕在化する. Statusにはbool Payloadが存在せず、Status(false)の
//          曖昧さは表現できない. PartialResultだけが値とErrorの同時保持を契約として許す.
//          全ComponentのResultはこの1つのTemplateの別名なので、この契約が全APIの契約である.

#include <wse/stew.h>
#include <wse/binding/Error.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace
{
	int failures = 0;

	// 失敗しても中断せず数え上げる. 1回の実行で壊れた項目をすべて報告するためである.
	void expect(const bool condition_in, const char* const message_in)
	{
		if (!condition_in)
		{
			std::cerr << "FAILED: " << message_in << '\n';
			++failures;
		}
	}

	// 前提条件違反がResultAccessError (std::logic_error) で顕在化することを固定する.
	template <typename Callable>
	void expectContractViolation(Callable&& callable_in, const char* const message_in)
	{
		bool rejected = false;
		try
		{
			callable_in();
		}
		catch (const wse::ResultAccessError&)
		{
			rejected = true;
		}
		expect(rejected, message_in);
	}

	wse::binding::Error sampleError()
	{
		return wse::binding::Error(
			wse::binding::eErrorCategory::Timeout, 42, "sample timeout", 7);
	}

	using IntResult = wse::Result<int, wse::binding::Error>;
	using BindingStatus = wse::Status<wse::binding::Error>;
	using IntPartial = wse::PartialResult<int, wse::binding::Error>;

	// 違反Exceptionは標準のlogic_error系であり、Legacy例外階層に属さないことを固定する.
	static_assert(std::is_base_of<std::logic_error, wse::ResultAccessError>::value,
		"ResultAccessError derives from std::logic_error");
}

int main()
{
	// --- 成功Result. 値だけを運び、error()は前提条件違反として顕在化する.
	{
		IntResult result = IntResult::success(21);
		expect(result.succeeded(), "success() reports succeeded");
		expect(result.value() == 21, "success value is readable");
		result.value() = 42;
		expect(result.value() == 42, "success value is mutable");
		expect(result.valueOr(0) == 42, "valueOr returns the value on success");
		expectContractViolation([&result] { (void)result.error(); },
			"error() on a successful result is a loud violation");
	}

	// --- 失敗Result. Errorだけを運び、value()は保持Errorの識別情報を載せて顕在化する.
	{
		const IntResult result = IntResult::failure(sampleError());
		expect(!result.succeeded(), "failure() reports failed");
		expect(result.error().category() == wse::binding::eErrorCategory::Timeout
			&& result.error().code() == 42, "failure error identity is readable");
		expect(result.valueOr(-1) == -1, "valueOr returns the fallback on failure");
		bool informative = false;
		try
		{
			(void)result.value();
		}
		catch (const wse::ResultAccessError& exception)
		{
			const std::string reason = exception.what();
			informative = reason.find("category=5") != std::string::npos
				&& reason.find("code=42") != std::string::npos;
		}
		expect(informative, "value() violation carries the stored error identity");
	}

	// --- Errorなしの失敗は構築できない. Factory自体が拒否する.
	expectContractViolation([] { (void)IntResult::failure(wse::binding::Error()); },
		"Result::failure rejects an empty error");
	expectContractViolation([] { (void)BindingStatus::failure(wse::binding::Error()); },
		"Status::failure rejects an empty error");

	// --- Status. bool Payloadを持たない成否だけの形.
	{
		const BindingStatus ok = BindingStatus::success();
		expect(ok.succeeded(), "Status::success reports succeeded");
		expectContractViolation([&ok] { (void)ok.error(); },
			"error() on a successful status is a loud violation");

		const BindingStatus failed = BindingStatus::failure(sampleError());
		expect(!failed.succeeded() && failed.error().code() == 42,
			"Status::failure carries the error");
	}

	// --- PartialResult. 値とErrorの同時保持を契約として許す唯一の形.
	{
		const IntPartial complete(10);
		expect(complete.succeeded() && complete.value() == 10, "Complete partial result");

		const IntPartial interrupted(3, sampleError());
		expect(!interrupted.succeeded(), "Interrupted partial result reports the error");
		expect(interrupted.value() == 3,
			"Partial value stays meaningful alongside the error");
		expect(interrupted.error().category() == wse::binding::eErrorCategory::Timeout,
			"Partial error identity is readable");
	}

	return failures == 0 ? 0 : 1;
}

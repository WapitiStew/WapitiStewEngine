// @file wse_worker_controller_contract.cpp
// @brief 内部WorkerController (core/wse/utility/wse_WorkerController.h) のLifecycle契約を固定する.
// @details 対象は公開APIではなく、BMultiThreadを置き換える内部実装である (LEGACY-006 PR 5-A).
//          固定するのは、Start/Stopの反復、二重Start拒否、二重Stopの冪等性、Worker内からのStop、
//          Exception containment、停止要求による待機の即時Wake、Destructorによるjoinである.
//          Public headerを含まないため、本TestはLibraryへLinkせず実装Fileを直接Compileする.

#include "core/wse/utility/wse_WorkerController.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

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

	using wse::detail::WorkerController;
	using namespace std::chrono_literals;

	// 条件成立を上限まで待つ. Threadの終了観測をSleep固定にしないためのHelperである.
	template <typename Predicate>
	bool eventually(Predicate&& predicate_in, const std::chrono::milliseconds limit_in = 2000ms)
	{
		const auto deadline = std::chrono::steady_clock::now() + limit_in;
		while (std::chrono::steady_clock::now() < deadline)
		{
			if (predicate_in())
			{
				return true;
			}
			std::this_thread::sleep_for(1ms);
		}
		return predicate_in();
	}
}

int main()
{
	// --- 空の本体は起動できない.
	{
		WorkerController controller;
		expect(!controller.start(nullptr), "an empty body must be rejected");
		expect(!controller.isRunning(), "a rejected start must not report running");
	}

	// --- Start/Stopの反復. 各Sessionで停止要求は新しく始まる.
	{
		WorkerController controller;
		for (int iteration = 0; iteration < 20; ++iteration)
		{
			std::atomic<int> ticks{0};
			const bool started = controller.start([&controller, &ticks]
			{
				while (!controller.waitFor(1ms))
				{
					++ticks;
				}
			});
			expect(started, "start must succeed after the previous session stopped");
			expect(!controller.isStopRequested(), "a new session must begin without a stop request");
			expect(eventually([&] { return ticks.load() >= 1; }), "the body must tick");
			controller.stop();
			expect(!controller.isRunning(), "stop must leave the worker joined and not running");
		}
	}

	// --- 二重Startは拒否され、実行中のWorkerを壊さない.
	{
		WorkerController controller;
		std::atomic<bool> entered{false};
		controller.start([&controller, &entered]
		{
			entered = true;
			while (!controller.waitFor(5ms)) {}
		});
		expect(eventually([&] { return entered.load(); }), "the first body must run");
		expect(!controller.start([] {}), "a second start while running must be rejected");
		expect(controller.isRunning(), "the rejected start must not stop the running worker");
		controller.stop();
	}

	// --- 二重Stopと未起動Stopは冪等である.
	{
		WorkerController controller;
		controller.stop();
		controller.stop();
		expect(!controller.isRunning(), "stop before any start must be safe");
		controller.start([&controller] { while (!controller.waitFor(5ms)) {} });
		controller.stop();
		controller.stop();
		expect(!controller.isRunning(), "a second stop must stay idempotent");
	}

	// --- Worker内からのstop()は要求のみを行い、Deadlockしない. 次のstart()が引き取る.
	{
		WorkerController controller;
		controller.start([&controller]
		{
			controller.stop();  // 自Threadからのstopはjoinしない.
		});
		expect(eventually([&] { return !controller.isRunning(); }),
			"a self-stop must end the worker without deadlocking");
		std::atomic<bool> second_ran{false};
		expect(eventually([&controller, &second_ran]
			{
				return controller.start([&second_ran] { second_ran = true; });
			}),
			"start after a self-stop must join the finished worker and succeed");
		expect(eventually([&] { return second_ran.load(); }), "the second body must run");
		controller.stop();
	}

	// --- Worker本体のExceptionはThread境界を越えず、Workerは停止として終了する.
	{
		WorkerController controller;
		controller.start([] { throw 42; });
		expect(eventually([&] { return !controller.isRunning(); }),
			"an exception in the body must end the worker as stopped");
		std::atomic<bool> ran{false};
		expect(eventually([&controller, &ran]
			{
				return controller.start([&ran] { ran = true; });
			}),
			"start after a contained exception must succeed");
		controller.stop();
	}

	// --- 停止要求は待機中のWorkerを即座に起こす. Legacyの200ms Pollingを持ち込まない.
	{
		WorkerController controller;
		std::atomic<bool> waited_out{false};
		controller.start([&controller, &waited_out]
		{
			// 停止要求が来るまで最長10秒待つ. 即時Wakeなら数ms〜数十msで戻る.
			waited_out = !controller.waitFor(10000ms);
		});
		std::this_thread::sleep_for(20ms);
		const auto requested_at = std::chrono::steady_clock::now();
		controller.stop();
		const auto stop_latency = std::chrono::steady_clock::now() - requested_at;
		expect(!waited_out.load(), "the wait must end by the stop request, not by timing out");
		expect(stop_latency < 2000ms, "the stop request must wake the waiting worker promptly");
	}

	// --- DestructorはWorkerをjoinする. 生存中のCaptureへ触れない形で観測する.
	{
		std::atomic<bool> finished{false};
		{
			WorkerController controller;
			controller.start([&controller, &finished]
			{
				while (!controller.waitFor(5ms)) {}
				finished = true;
			});
		}
		expect(finished.load(), "the destructor must request the stop and join the worker");
	}

	if (failures > 0)
	{
		std::cerr << "wse_worker_controller_contract failed: " << failures << '\n';
		return 1;
	}
	std::cout << "wse_worker_controller_contract passed" << '\n';
	return 0;
}

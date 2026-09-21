// @file engine/wse/test/stress/core_lifecycle_soak.cpp
// @brief CoreのTimer Lifecycleを大量反復し、Deadlock・Crash・状態残留がないことを固定する.
// @details Timerのstart→stopを数百回、Callback内からのstop要求、設定変更の並走を通す.
//          1回の成功では出ない競合 - joinし損ねたThread、2回目のstartで壊れる状態、stop中のCallback -
//          を反復で炙り出す. 経過が返ってくること自体が合格であり、TIMEOUTがDeadlock検出器である.

#include <wse/stew.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace
{
	int failures = 0;

	void expect(const bool condition_in, const char* const message_in)
	{
		if (!condition_in)
		{
			std::cerr << "FAILED: " << message_in << '\n';
			++failures;
		}
	}

	using namespace std::chrono_literals;
}

int main()
{
	// --- Timer start/stopの大量反復. 各周回で初期化からやり直し、状態の持ち越しを許さない.
	{
		std::atomic<int> ticks{ 0 };
		for (int cycle = 0; cycle < 300; ++cycle)
		{
			wse::Timer timer;
			timer.start(1ms, [&ticks]() { ticks.fetch_add(1); });
			timer.stop();
		}
		expect(true, "300 timer start/stop cycles complete");
	}

	// --- 動作中の設定変更の並走. 変更とTickが同時でも壊れない.
	{
		std::atomic<int> ticks{ 0 };
		wse::Timer timer;
		timer.start(1ms, [&ticks]() { ticks.fetch_add(1); });
		// Churn the interval while the timer runs. A tick is not guaranteed within any fixed
		// wall-clock window under load, so wait until one is observed (bounded) rather than
		// asserting on a fixed sleep - the point of this case is that churn does not deadlock or
		// crash, and that a tick still eventually arrives.
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		for (int i = 0; i < 200; ++i)
		{
			timer.setInterval(std::chrono::milliseconds(1 + (i % 3)));
		}
		while (ticks.load() == 0 && std::chrono::steady_clock::now() < deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		timer.stop();
		expect(ticks.load() > 0, "Timer ticked while its config churned");
	}

	// --- Callback内からのstop要求. Callback Threadとstop Threadの合流でDeadlockしない.
	{
		std::atomic<bool> stop_requested{ false };
		wse::Timer timer;
		timer.start(1ms, [&stop_requested, &timer]()
		{
			stop_requested.store(true);
			timer.stop();
		});
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (!stop_requested.load() && std::chrono::steady_clock::now() < deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		timer.stop();
		expect(stop_requested.load(), "Callback observed before the external stop");
	}

	// --- Timerの二重stopと未start stop. 冪等でなければここで落ちる.
	{
		wse::Timer timer;
		timer.stop();
		timer.start(5ms, []() {});
		timer.stop();
		timer.stop();
		expect(true, "Stop is safe before start and twice after");
	}

	std::cout << "core lifecycle soak complete\n";
	return failures == 0 ? 0 : 1;
}

// @file engine/wse/test/hardware/iui_keyboard_hardware_smoke.cpp
// @brief Opt-in physical-keyboard smoke test. Press and release Q when prompted.
// @brief 実Keyboardと操作者を要するため既定Gateには入らないOpt-in Smoke。人がQを押して
//        離すことで、Polling経路（getASCII）とCallback経路の両方が同じ物理Eventを観測し、
//        全Key解放が検出されるまでを一巡で確かめる。Exit codeは段階別（2=Device無し、
//        3=押下未観測、4=解放未観測）で、HARDWARE_*の検証記録がこの区別に依存する.

#include <iui/stew.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>

int main()
{
	using wse::iui::Keyboard;
	using wse::iui::KeyboardAccessState;

	// Captures outlive Keyboard, whose destructor joins the polling worker.
	std::atomic<bool> callbackObserved{false};
	std::atomic<bool> callbackReleased{false};
	Keyboard keyboard;
	for (int attempt = 0;
		attempt < 500 && keyboard.accessState() == KeyboardAccessState::Starting;
		++attempt)
	{
		wse::Wait::msec(10U);
	}
	if (!keyboard.isAvailable())
	{
		std::cerr << "No readable physical keyboard is available.\n";
		return 2;
	}

	keyboard.setCallback([&callbackObserved, &callbackReleased](const wse::iui::KeyboardState& state) {
		const bool pressed = state.ascii[static_cast<std::size_t>('q')] ||
			state.ascii[static_cast<std::size_t>('Q')];
		if (pressed)
			callbackObserved.store(true, std::memory_order_release);
		else if (callbackObserved.load(std::memory_order_acquire))
			callbackReleased.store(true, std::memory_order_release);
	});

	std::cout << "Press and release the Q key within 60 seconds." << std::endl;
	bool pressObserved = false;
	const auto pressDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
	while (std::chrono::steady_clock::now() < pressDeadline)
	{
		const int8_t ascii = keyboard.getASCII();
		if (ascii == static_cast<int8_t>('q') || ascii == static_cast<int8_t>('Q'))
		{
			pressObserved = true;
		}
		if (pressObserved && callbackObserved.load(std::memory_order_acquire))
		{
			break;
		}
		wse::Wait::msec(10U);
	}
	if (!pressObserved || !callbackObserved.load(std::memory_order_acquire))
	{
		std::cerr << "Q key press or callback was not observed.\n";
		return 3;
	}

	for (int attempt = 0; attempt < 500; ++attempt)
	{
		if (keyboard.isReleasedAllKey() && callbackReleased.load(std::memory_order_acquire))
		{
			std::cout << "Physical keyboard smoke passed." << std::endl;
			return 0;
		}
		wse::Wait::msec(10U);
	}
	std::cerr << "Key release was not observed.\n";
	return 4;
}

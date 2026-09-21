// @file engine/wse/test/characterization/iui_keyboard_lifecycle.cpp
// @brief 物理入力値に依存せずKeyboardの定数とThread lifecycleを確認する。
// @details Keyboardは生成と同時にMonitor Threadを1本起こし, 破棄で停止と合流を行う. ここが壊れる
//          と, 利用者のApplicationが終了時にHangするか, 解放済みのCallbackを別Threadが呼ぶ.
//          本Testが固定するのは4点である. KeyboardStateの各Group幅がWSE 0.1のABIから動いて
//          いないこと, accessState()がStartingで止まらず有効な状態を返す
//          こと, setCallback()とCopyがCallableを所有し解除後に最終的に破棄する
//          こと, そしてCopy/Move/代入と生成破棄の反復がHangやCrashなく完結することである.
//          Linux Descriptorの解放回数は別のkeyboard_devices_contractで検査する.
//          物理Keyは1つも押さない. どのKeyが押されているかというKey mapping自体はここでは
//          検証しておらず, Backendが常に全解放を返しても本Testは通る.

#include <iui/stew.h>

#include <memory>
#include <type_traits>

int main()
{
	// Group幅はKeyboardStateのLayoutそのものであり, 公開済みBinaryのABIである. 値を1つ
	// 動かすと既存のBindingやCompile済み利用者がずれたIndexを読む. Compile時に止める.
	// FANCTION_NUMの綴りはWSE 0.1からの互換名で, 誤字ではあるが訂正できない.
	static_assert(wse::iui::ASCII_NUM == 128, "ASCII state width");
	static_assert(wse::iui::FANCTION_NUM == 24, "Function-key state width");
	static_assert(wse::iui::ARROW_NUM == 4, "Arrow-key state width");
	static_assert(wse::iui::LOCK_NUM == 3, "Lock-key state width");
	static_assert(wse::iui::COMMAND_NUM == 9, "Command-key state width");
	// Thread所有型でありながらCopyとMoveが可能であることもWSE 0.1からの契約である.
	// Copyは独立したMonitorを起こすため, 削ると値渡しで書かれた既存Codeが壊れる.
	static_assert(std::is_copy_constructible<wse::iui::Keyboard>::value, "Legacy Keyboard is copyable");
	static_assert(std::is_move_constructible<wse::iui::Keyboard>::value, "Legacy Keyboard is movable");

	{
		// Monitorの初回Probeが終わるまでStateはStartingである. 実機Keyboardの有無で結果が
		// 変わらないよう, Ready/Unavailable/PermissionDeniedのどれに落ち着いても先へ進み,
		// 「有限時間でStartingを抜ける」ことだけを見る. 抜けないのはWorkerが起動していないか
		// 初回Probeで固まった場合であり, 利用者から見ると永久にisAvailable()がfalseになる.
		// 1ms x 200回の上限はctestの5秒Timeoutに対して十分小さく, かつ低速環境のDevice走査が
		// 終わる程度に長い. 短すぎると偽陽性でreturn 6になる.
		wse::iui::Keyboard keyboard;
		for (int attempt = 0;
			attempt < 200 && keyboard.accessState() == wse::iui::KeyboardAccessState::Starting;
			++attempt)
		{
			wse::Wait::msec(1U);
		}
		if (keyboard.accessState() == wse::iui::KeyboardAccessState::Starting)
		{
			return 6;
		}
		// State can change between observers; separate calls are not an atomic pair.
		(void)keyboard.isAvailable();
		switch (keyboard.accessState())
		{
		case wse::iui::KeyboardAccessState::Ready:
		case wse::iui::KeyboardAccessState::Unavailable:
		case wse::iui::KeyboardAccessState::PermissionDenied:
		case wse::iui::KeyboardAccessState::Disconnected:
			break;
		default:
			return 7;
		}
#if defined(_WIN32)
		// Windows Ready describes enabled polling, including systems without a probed device.
		if (keyboard.accessState() != wse::iui::KeyboardAccessState::Ready || !keyboard.isAvailable())
			return 10;
#endif

		// Backendが読めない状態でもsnapshot()は全解放の所有Snapshotを返し, 例外も部分的な
		// Groupも返さない. ここで実際に効いているのはその「返ってくる」ことのほうで, 幅の
		// 比較自体はKeyboardStateがstd::arrayである限りCompile時に決まるため破れない.
		const wse::iui::KeyboardState state = keyboard.snapshot();
		if (state.ascii.size() != wse::iui::ASCII_NUM ||
			state.function.size() != wse::iui::FANCTION_NUM ||
			state.arrow.size() != wse::iui::ARROW_NUM ||
			state.lock.size() != wse::iui::LOCK_NUM ||
			state.command.size() != wse::iui::COMMAND_NUM)
		{
			return 1;
		}

		// Callbackの所有権を寿命で観測する. shared_ptrをLambdaに捕まえ, 呼び出し側の参照を
		// 手放してもweak_ptrが生きていれば, Keyboardが参照ではなくCallableを所有している.
		// Copy also retains the callable. Clearing one owner must not release another
		// owner's capture. Clearing both is not an invocation barrier: the worker may
		// already hold its own shared reference, even in a scan with no key change.
		auto callbackOwner = std::make_shared<int>(1);
		std::weak_ptr<int> callbackOwnerLifetime = callbackOwner;
		keyboard.setCallback([callbackOwner](const wse::iui::KeyboardState&) { (void)callbackOwner; });
		callbackOwner.reset();
		if (callbackOwnerLifetime.expired())
		{
			return 2;
		}
		wse::iui::Keyboard callbackCopy(keyboard);
		keyboard.clearCallback();
		if (callbackOwnerLifetime.expired())
		{
			return 8;
		}
		callbackCopy.clearCallback();
		for (int attempt = 0; attempt < 200 && !callbackOwnerLifetime.expired(); ++attempt)
		{
			wse::Wait::msec(1U);
		}
		if (!callbackOwnerLifetime.expired())
		{
			return 3;
		}

		// Copy, Move, Copy代入, Move代入を1本に繋げ, 最後にMove後のObjectからsnapshot()を
		// 取る. 代入で置き換えられた側は自分のMonitorを停止して合流しなければならず, Move元は
		// 監視をやめる. Assertは置いていない. 契約が破れたときはHang, 二重停止によるCrash,
		// あるいはSanitizerの検出という形で現れる.
		wse::iui::Keyboard copied(keyboard);
		wse::iui::Keyboard moved(std::move(copied));
		keyboard = moved;
		moved = std::move(keyboard);
		(void)moved.snapshot();
	}

	// 生成と破棄を続けて8回繰り返す. 1回では気付けない, Thread, FileDescriptor, Device
	// Handleの取りこぼしを積み上げて見つけるためである. 回数はctestの5秒Timeoutに収まる
	// 範囲で選んであり, 停止と合流に失敗する実装はここでTimeoutとして現れる.
	// これもAssertを持たず, Hang, Crash, Sanitizerの検出でしか失敗しない.
	for (int lifecycle = 0; lifecycle < 8; ++lifecycle)
	{
		wse::iui::Keyboard keyboard;
		(void)keyboard.snapshot();
	}

	return 0;
}

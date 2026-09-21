// @file engine/wse/test/characterization/core_runtime_contract.cpp
// @brief CoreのWait、Timer、Structured Log契約を固定する。
// ここが落ちると、Core上に載る全ComponentのTiming待ち、Flag監視、周期実行、Log出力のどれかが
// 壊れている. 具体的に固定するのは、Waitが要求した時間Scaleで待つこと、Timerが停止後に
// Callbackを出さずCaptureを停止時に手放すこと、Log SinkのLevel filterと登録解除が効くこと.

#include <wse/stew.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
	// 違反を数えるだけでabortしない. 1回の実行で全ての違反を出し、最後にmainの終了Codeへ畳む.
	int failures = 0;

	void expect(const bool condition_in, const char* const message_in)
	{
		if (!condition_in)
		{
			std::cerr << "FAILED: " << message_in << '\n';
			++failures;
		}
	}

	// 受け取ったLog Recordをそのまま貯めるSink. Level filterが効いているかを後から数えるために使う.
	class RecordingSink final : public wse::LogSink
	{
		public: void onLog(const wse::LogRecord& record_in) override
		{
			std::lock_guard<std::mutex> lock(mutex);
			records.push_back(record_in);
		}

		public: std::vector<wse::LogRecord> snapshot()
		{
			std::lock_guard<std::mutex> lock(mutex);
			return records;
		}

		private: std::mutex mutex;
		private: std::vector<wse::LogRecord> records;
	};

	// A sink can remove itself and log recursively without deadlocking the registry.
	class RemovingSink final : public wse::LogSink
	{
		public: wse::LogSinkHandle handle = 0;
		public: int calls = 0;
		public: void onLog(const wse::LogRecord& record_in) override
		{
			(void)record_in;
			++calls;
			wse::unregisterLogSink(handle);
			wse::writeLog(wse::LogLevel::Error, "CoreTest", "nested sink message");
		}
	};

	class ThrowingSink final : public wse::LogSink
	{
		public: int calls = 0;
		public: void onLog(const wse::LogRecord& record_in) override
		{
			(void)record_in;
			++calls;
			throw std::runtime_error("test sink failure");
		}
	};
}

int main()
{
	// Waitが要求したScaleで待つことを確認する. 上限500 msは、待ち過ぎ（Scale取り違え）を捕まえる
	// ための緩い上限であり、下限は即時returnでないことだけを見る.
	const auto waitStart = std::chrono::steady_clock::now();
	wse::Wait::msec(5);
	const auto waitElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - waitStart);
	expect(waitElapsed.count() >= 1, "Millisecond wait does not return immediately");
	expect(waitElapsed < std::chrono::milliseconds(500), "Millisecond wait uses millisecond scale");

	// usecとnsecも同じScale取り違えを別単位で見る. 下限は要求値の約1/4に置いてあり、Scheduler
	// の粒度による早戻りは許すが、単位を1000倍取り違えた実装は下限か上限のどちらかで落ちる.
	const auto microsecondWaitStart = std::chrono::steady_clock::now();
	wse::Wait::usec(2000);
	const auto microsecondWaitElapsed = std::chrono::steady_clock::now() - microsecondWaitStart;
	expect(microsecondWaitElapsed >= std::chrono::microseconds(500), "Microsecond wait uses microsecond scale");
	expect(microsecondWaitElapsed < std::chrono::milliseconds(500), "Microsecond wait remains below second scale");

	const auto nanosecondWaitStart = std::chrono::steady_clock::now();
	wse::Wait::nsec(1000000);
	const auto nanosecondWaitElapsed = std::chrono::steady_clock::now() - nanosecondWaitStart;
	expect(nanosecondWaitElapsed >= std::chrono::microseconds(250), "Nanosecond wait uses nanosecond scale");
	expect(nanosecondWaitElapsed < std::chrono::milliseconds(500), "Nanosecond wait remains below second scale");


	// Timerの契約は、不正な周期を受け付けないこと、走っている間だけCallbackを出すこと、
	// Callbackの寿命を自分で持つことの3つ. 以下はその順に確認する.
	std::atomic_int timerCount{0};
	{
		using namespace std::chrono_literals;
		wse::Timer timer;
		// Intervalは周期の分母なので0はinvalid_argumentで境界側が弾く. 通ると即Busy loopになる.
		bool rejectedZeroInterval = false;
		try
		{
			timer.start(0ms, [&timerCount]() { timerCount.fetch_add(1); });
		}
		catch (const std::invalid_argument&)
		{
			rejectedZeroInterval = true;
		}
		expect(rejectedZeroInterval, "Timer rejects a zero interval");
		bool rejectedEmptyCallback = false;
		try
		{
			timer.start(10ms, wse::Timer::TimerCallback());
		}
		catch (const std::invalid_argument&)
		{
			rejectedEmptyCallback = true;
		}
		expect(rejectedEmptyCallback, "Timer rejects an empty callback");
		expect(!timer.isRunning(), "A rejected start leaves the timer stopped");

		// 10 ms周期で3回以上入ることを見る. 1回では単発起動と区別が付かないので3回が最小、
		// 待ちの上限1秒はCallbackが一度も来ない場合にTestが固まらないための打ち切り.
		expect(timer.start(10ms, [&timerCount]() { timerCount.fetch_add(1); }),
			"Timer starts from the stopped state");
		expect(timer.isRunning(), "A started timer reports running");
		expect(!timer.start(10ms, [] {}), "A second start while running is rejected");
		const auto timerDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
		while (timerCount.load() < 3 && std::chrono::steady_clock::now() < timerDeadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		expect(timerCount.load() >= 3, "Timer invokes its periodic callback");
		// stopは冪等で、返った時点でCallbackは打ち切られている. 20 msは周期10 msの2倍で、
		// 停止後に取りこぼした1発が遅れて走ればここで数が動く.
		timer.stop();
		timer.stop();
		expect(!timer.isRunning(), "A stopped timer reports not running");
		const int stoppedCount = timerCount.load();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		expect(timerCount.load() == stoppedCount, "Timer dispatches no callback after stop returns");

		// 同一Instanceを8回作り直す. expectを持たないのは意図で、Handleの二重解放や
		// 停止待ちのDeadlockはCrashかHangとして出るため、通り抜けること自体が結果になる.
		for (int lifecycle = 0; lifecycle < 8; ++lifecycle)
		{
			timer.start(1ms, [] {});
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			timer.stop();
		}

		// setIntervalは実行中にも効き、不正値を弾く.
		timer.start(50ms, [] {});
		timer.setInterval(1ms);
		expect(timer.interval() == 1ms, "setInterval takes effect");
		bool rejectedZeroReconfigure = false;
		try
		{
			timer.setInterval(0ms);
		}
		catch (const std::invalid_argument&)
		{
			rejectedZeroReconfigure = true;
		}
		expect(rejectedZeroReconfigure, "setInterval rejects a zero interval");
		timer.stop();

		// TimerはCallbackのCaptureを所有する. 呼び出し側がshared_ptrを手放しても実行中は
		// 生きていなければならず、そうでなければCallback実行中に解放済みObjectへ触れる.
		// 停止でCaptureは解放される.
		auto callbackOwner = std::make_shared<int>(7);
		std::weak_ptr<int> callbackOwnerLifetime = callbackOwner;
		timer.start(1ms, [callbackOwner]() { (void)callbackOwner; });
		callbackOwner.reset();
		expect(!callbackOwnerLifetime.expired(), "Timer owns callback captures while running");
		timer.stop();
		expect(callbackOwnerLifetime.expired(), "Timer releases callback captures when stopped");

		// CallbackのExceptionはThread境界を越えず、Timerを停止させる.
		timer.start(1ms, []() { throw 42; });
		const auto exceptionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
		while (timer.isRunning() && std::chrono::steady_clock::now() < exceptionDeadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		expect(!timer.isRunning(), "A throwing callback stops the timer");
		timer.stop();

		// Move後のTimerは停止状態から始まる. 移動元のThreadを引き継いだまま走り続けると、
		// Scope脱出時に所有者不明のCallbackが残る.
		wse::Timer moved(std::move(timer));
		expect(!moved.isRunning(), "Moved Timer starts in a stopped state");
		moved.stop();
	}


	// Callbackの中からstopを呼びつつ、別Threadがstopを要求する競合を作る.
	// 停止Lockを保持したままCallbackを待つ実装だとここで互いに待ち合ってDeadlockする.
	// releaseCallbackでCallbackを掴んだままにするのが、その競合窓を確実に開く仕掛け.
	{
		using namespace std::chrono_literals;
		wse::Timer reentrantTimer;
		std::atomic_bool callbackEntered{false};
		std::atomic_bool releaseCallback{false};
		reentrantTimer.start(1ms, [&]()
		{
			callbackEntered.store(true);
			while (!releaseCallback.load())
			{
				std::this_thread::yield();
			}
			reentrantTimer.stop();
		});
		// 1 ms周期に対し100 msはCallbackへ入るのに十分な余裕. ここで入れないと以降の競合が
		// 成立せず、続くexpectは何も見ないまま通ってしまう.
		const auto enterDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
		while (!callbackEntered.load() && std::chrono::steady_clock::now() < enterDeadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		expect(callbackEntered.load(), "Timer entered the reentrant-stop callback");

		// 5 msは外部停止側がstopの中で待ちに入るまでの間. 先にCallbackを解放すると
		// 競合が起きる前に停止が完了してしまい、Deadlockを踏む機会がなくなる.
		std::thread stopOwner([&reentrantTimer]() { reentrantTimer.stop(); });
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		releaseCallback.store(true);
		stopOwner.join();
		reentrantTimer.stop();
		expect(!reentrantTimer.isRunning(), "Callback stop does not deadlock an owner stop");

		// Callback内stopの後も再startできる. 前WorkerのjoinはstartまたはDestructorが引き取る.
		std::atomic_bool restarted{false};
		const auto restartDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
		bool restartAccepted = false;
		while (!restartAccepted && std::chrono::steady_clock::now() < restartDeadline)
		{
			restartAccepted = reentrantTimer.start(1ms, [&restarted]() { restarted.store(true); });
			if (!restartAccepted)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		expect(restartAccepted, "Timer restarts after a callback-initiated stop");
		const auto tickDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
		while (!restarted.load() && std::chrono::steady_clock::now() < tickDeadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		expect(restarted.load(), "The restarted timer ticks");
		reentrantTimer.stop();
	}

	// Structured Logの契約は4つ. 登録の入口がnullptrを弾くこと、最低Levelが往復すること、
	// 通ったRecordのField（Level、Tag、Source、Message、Details、Timestamp）が欠けないこと、
	// そして登録解除で配送が止まること. Sinkを1つ挿し、Warning以上に絞って2件流して数える.
	const auto sink = std::make_shared<RecordingSink>();
	expect(wse::registerLogSink(nullptr) == 0, "Null log sink is rejected");
	const wse::LogSinkHandle handle = wse::registerLogSink(sink);
	expect(handle != 0, "Log sink registration returns a handle");
	wse::setMinimumLogLevel(wse::LogLevel::Warning);
	const wse::sLogConfig format_config{ "CoreTest", wse::eLogMode::NONE,
		wse::eLogSeparate::SPACE, wse::eLogPrefix::NONE, wse::eLogSuffix::LINE_BREAK };
	const auto formatted = wse::formatLogMessage( format_config, "format only", "C:/private/Source.cpp", 123 );
	expect(formatted.find("CoreTest") != std::string::npos
		&& formatted.find("Source.cpp") != std::string::npos
		&& formatted.find("123") != std::string::npos, "Standard formatter preserves tag and caller location");
	expect(formatted.find("private") == std::string::npos, "Standard formatter uses a filename, not a source tree path");
	expect(sink->snapshot().empty(), "Formatting never dispatches to registered sinks");
	expect(wse::minimumLogLevel() == wse::LogLevel::Warning, "Minimum log level round-trip");
	wse::writeLog(wse::LogLevel::Info, "Phase1", "filtered");
	wse::writeLog(wse::LogLevel::Error, "Phase1", "Runtime", "failure", "code=7");
	wse::flushLog();

	std::vector<wse::LogRecord> records = sink->snapshot();
	expect(records.size() == 1, "Log level filters lower-severity records");
	if (records.size() == 1)
	{
		expect(records[0].level == wse::LogLevel::Error, "Log record severity");
		expect(records[0].tag == "Phase1", "Log record tag");
		expect(records[0].source == "Runtime", "Log record source");
		expect(records[0].message == "failure" && records[0].details == "code=7", "Log record payload");
		expect(!records[0].timestamp.empty(), "Log record timestamp");
	}

	// 解除後も配送が続くと、寿命の切れたSinkへ書き込むことになる. 件数が動かないことで見る.
	// 最後にLevelを既定へ戻すのは、Log設定がProcess全体のGlobal状態で、戻さないと後続へ漏れるため.
	wse::unregisterLogSink(handle);
	wse::writeLog(wse::LogLevel::Error, "Phase1", "after unregister");
	expect(sink->snapshot().size() == records.size(), "Unregistered sink receives no further records");
	wse::setMinimumLogLevel(wse::LogLevel::Info);

	// CORE-SVC-04: registry order is unspecified, so verify counts without assuming sink order.
	{
		const auto recording = std::make_shared<RecordingSink>();
		const auto removing = std::make_shared<RemovingSink>();
		const auto throwing = std::make_shared<ThrowingSink>();
		const auto recordingHandle = wse::registerLogSink(recording);
		removing->handle = wse::registerLogSink(removing);
		const auto throwingHandle = wse::registerLogSink(throwing);
		wse::writeLog(wse::LogLevel::Error, "CoreTest", "outer sink message");
		expect(removing->calls == 1, "A sink may unregister itself during delivery");
		expect(throwing->calls == 1 && recording->snapshot().size() == 1U,
			"Nested writes skip all custom sinks and a throwing sink does not stop peers");
		wse::writeLog(wse::LogLevel::Error, "CoreTest", "after self-removal");
		expect(removing->calls == 1 && throwing->calls == 2 && recording->snapshot().size() == 2U,
			"Self-removal affects subsequent dispatch without disabling other sinks");
		wse::unregisterLogSink(recordingHandle);
		wse::unregisterLogSink(throwingHandle);
	}

// 以降はLinux専用. Device列挙はPlatform実装のある側でしか意味を持たないため、Windowsでは
// このBlockごと消え、列挙契約は他のTestで担保する必要がある.
#if defined(__linux__)
	// 実機構成に依存しないよう、件数ではなく「取れた要素が空でない」ことだけを見る.
	// Serialは1本も無い環境があり得るのでLoopが空回りしても失敗にしない.
	bool enumeratedPortableDevices = true;
	try
	{
		const std::vector<wse::sSerialInfo> serials = wse::pickupDeviceInfo::Serials();
		for (const wse::sSerialInfo& serial : serials)
		{
			expect(!serial.port_name.empty(), "Linux serial enumeration returns a device path");
		}

		const std::vector<wse::sNetworkInfo> networks = wse::pickupDeviceInfo::Networks();
		expect(!networks.empty(), "Linux network enumeration returns at least one interface");
		for (const wse::sNetworkInfo& network : networks)
		{
			expect(!network.adapter_name.empty(), "Linux network interface has an adapter name");
		}
	}
	catch (...)
	{
		enumeratedPortableDevices = false;
	}
	expect(enumeratedPortableDevices, "Linux network and serial enumeration succeeds");

	// 未実装のMonitor列挙は、空を黙って返すのではなくstd::logic_errorを投げるのが契約.
	// 空返しへ変わると、呼び出し側は「Monitorが1台も無い」と誤解して先へ進んでしまう.
	bool rejectedUnsupportedDeviceEnumeration = false;
	try
	{
		(void)wse::pickupDeviceInfo::Monitors();
	}
	catch (const std::logic_error&)
	{
		rejectedUnsupportedDeviceEnumeration = true;
	}
	expect(rejectedUnsupportedDeviceEnumeration, "Unsupported Linux device enumeration throws std::logic_error");
#endif

	return failures == 0 ? 0 : 1;
}

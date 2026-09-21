// @file engine/wse/test/stress/xpt_failure_injection.cpp
// @brief XPTの異常経路 - 接続Reset・切断後の操作・Cancel殺到 - を注入し、
//        Crash・Deadlock・沈黙成功が起きないことを固定する.
// @details In-processのサーバSocketを意図的にResetし、そのTransport上のsend/receiveがErrorを
//          返して停止すること、切断後の操作がErrorになること、Cancel済みContextが即座に諦めること、
//          並列Cancelが伝搬することを通す. 経過が返ることが合格で、TIMEOUTがDeadlock検出器である.

#include <utility>
#include <xpt/stew.h>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

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

#if defined( _WIN32 )
	using RawSocket = SOCKET;
	constexpr RawSocket INVALID_RAW = INVALID_SOCKET;
	void closeRaw(const RawSocket s) { closesocket(s); }
	void setLingerZero(const RawSocket s)
	{
		linger value{ 1, 0 };
		setsockopt(s, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&value), sizeof(value));
	}
#else
	using RawSocket = int;
	constexpr RawSocket INVALID_RAW = -1;
	void closeRaw(const RawSocket s) { close(s); }
	void setLingerZero(const RawSocket s)
	{
		linger value{ 1, 0 };
		setsockopt(s, SOL_SOCKET, SO_LINGER, &value, sizeof(value));
	}
#endif

	// 1接続を受け付け、Handleを渡すだけの最小サーバ. 呼び手がそのSocketをResetする.
	struct OneShotServer
	{
		RawSocket listener;
		std::uint16_t port;
		std::thread worker;
		std::atomic<RawSocket> connection;

		bool start()
		{
			listener = socket(AF_INET, SOCK_STREAM, 0);
			if (listener == INVALID_RAW) { return false; }
			sockaddr_in address = {};
			address.sin_family = AF_INET;
			address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			address.sin_port = 0;
			if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
				listen(listener, 1) != 0) { return false; }
			socklen_t length = sizeof(address);
			getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length);
			port = ntohs(address.sin_port);
			worker = std::thread([this] {
				const RawSocket c = accept(listener, nullptr, nullptr);
				connection.store(c);
			});
			return true;
		}

		void resetConnection()
		{
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
			while (connection.load() == INVALID_RAW &&
				std::chrono::steady_clock::now() < deadline)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			const RawSocket c = connection.load();
			if (c != INVALID_RAW)
			{
				setLingerZero(c);  // close now sends RST rather than FIN
				closeRaw(c);
			}
		}

		void stop()
		{
			if (worker.joinable()) { worker.join(); }
			if (listener != INVALID_RAW) { closeRaw(listener); }
		}

        //! @brief Construct all members with explicit defaults.
        OneShotServer()
            : listener   ( INVALID_RAW )
            , port       ( 0 )
            , worker     ()
            , connection { INVALID_RAW }
        {
        }
	};

	wse::xpt::OperationContext context(const int ms)
	{
		return wse::xpt::OperationContext(
			wse::xpt::Timeout(std::chrono::milliseconds(ms)));
	}
}

int main()
{
#if defined( _WIN32 )
	WSADATA wsa = {};
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

	// --- 接続Reset中のreceive. RSTを受けてErrorで停止し、沈黙成功にならない.
	{
		OneShotServer server;
		expect(server.start(), "reset fixture server starts");
		wse::xpt::TcpClient client;
		const bool connected =
			client.connect(wse::xpt::Endpoint("127.0.0.1", server.port), context(2000)).succeeded();
		expect(connected, "reset fixture connects");
		server.resetConnection();
		bool ended = false;
		for (int attempt = 0; attempt < 200 && !ended; ++attempt)
		{
			const auto result = client.receive(64, context(200));
			if (!result.succeeded() && result.value().empty())
			{
				ended = true;
			}
		}
		expect(ended, "receive on a reset connection ends with an error");
		client.disconnect();
		server.stop();
	}

	// --- 切断後の操作. disconnect後のsend/receiveはErrorであり、Crashしない.
	{
		OneShotServer server;
		expect(server.start(), "post-disconnect fixture server starts");
		wse::xpt::TcpClient client;
		expect(client.connect(wse::xpt::Endpoint("127.0.0.1", server.port), context(2000)).succeeded(),
			"post-disconnect fixture connects");
		client.disconnect();
		expect(!client.isConnected(), "client reports disconnected");
		const std::vector<std::uint8_t> payload(16, 0x11);
		expect(!client.send(payload, context(200)).succeeded(),
			"send after disconnect fails");
		expect(!client.receive(16, context(200)).succeeded(),
			"receive after disconnect fails");
		server.resetConnection();
		server.stop();
	}

	// --- 先行Cancel. Cancel済みContextでのconnectは即座に諦める.
	{
		wse::xpt::CancellationSource source;
		source.cancel();
		wse::xpt::OperationContext cancelled(
			wse::xpt::Timeout(std::chrono::milliseconds(2000)), source.token());
		wse::xpt::TcpClient client;
		const auto begin = std::chrono::steady_clock::now();
		const bool ok = client.connect(wse::xpt::Endpoint("10.255.255.1", 9), cancelled).succeeded();
		const auto elapsed = std::chrono::steady_clock::now() - begin;
		expect(!ok, "connect under a pre-cancelled context does not succeed");
		expect(elapsed < std::chrono::milliseconds(1500),
			"pre-cancelled connect gives up well before the timeout");
	}

	// --- 並列Cancel殺到. 多数のTokenを一斉にCancelしても伝搬する.
	{
		wse::xpt::CancellationSource source;
		std::vector<std::thread> observers;
		std::atomic<int> observed{ 0 };
		for (int i = 0; i < 32; ++i)
		{
			observers.emplace_back([&source, &observed] {
				const wse::xpt::CancellationToken token = source.token();
				while (!token.isCancellationRequested())
				{
					std::this_thread::yield();
				}
				observed.fetch_add(1);
			});
		}
		source.cancel();
		for (std::thread& observer : observers)
		{
			observer.join();
		}
		expect(observed.load() == 32, "every concurrent token observed the cancellation");
	}

	std::cout << "xpt failure injection complete\n";
#if defined( _WIN32 )
	WSACleanup();
#endif
	return failures == 0 ? 0 : 1;
}

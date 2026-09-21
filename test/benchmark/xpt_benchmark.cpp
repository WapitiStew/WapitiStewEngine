// @file engine/wse/test/benchmark/xpt_benchmark.cpp
// @brief XPTのLoopback性能とCancellation／Retryの間接Costを計測する (PERF-002).
// @details In-processのEchoサーバに対してTCPのRTTとThroughput、UDPの送信Throughputを測り、
//          Socketに触れないCancellation伝搬とRetryPolicy判定のCostを併せて記録する.
//          Serial loopbackは実機のPort対を要するため、実機Phase (HW検証) まで対象外である.
//          判定はしない - 出力TSVがCommit間比較の材料である.

#include <utility>
#include <xpt/stew.h>

#include "benchmark_support.h"

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
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace
{
	void require(const bool value_in, const char* const message_in)
	{
		if (!value_in) { std::fprintf(stderr, "Benchmark failed: %s\n", message_in); std::exit(1); }
	}
#if defined( _WIN32 )
	using BenchSocket = SOCKET;
	constexpr BenchSocket INVALID_BENCH_SOCKET = INVALID_SOCKET;
	void closeBenchSocket(const BenchSocket socket_in) { closesocket(socket_in); }
#else
	using BenchSocket = int;
	constexpr BenchSocket INVALID_BENCH_SOCKET = -1;
	void closeBenchSocket(const BenchSocket socket_in) { close(socket_in); }
#endif

	//! 127.0.0.1の空きPortで待ち受け、受けたByteをそのまま返すEchoサーバ.
	struct EchoServer
	{
		BenchSocket listener;
		std::uint16_t port;
		std::thread worker;
		std::atomic<bool> stopping;

		bool start()
		{
			listener = socket(AF_INET, SOCK_STREAM, 0);
			if (listener == INVALID_BENCH_SOCKET)
			{
				return false;
			}
			sockaddr_in address = {};
			address.sin_family = AF_INET;
			address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			address.sin_port = 0;
			if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
				listen(listener, 1) != 0)
			{
				return false;
			}
			socklen_t length = sizeof(address);
			require(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0, "TCP endpoint");
			port = ntohs(address.sin_port);
			worker = std::thread([this] {
				const BenchSocket connection = accept(listener, nullptr, nullptr);
				if (connection == INVALID_BENCH_SOCKET)
				{
					return;
				}
				std::vector<char> buffer(64 * 1024);
				while (!stopping.load())
				{
					const auto received = recv(connection, buffer.data(),
						static_cast<int>(buffer.size()), 0);
					if (received <= 0)
					{
						break;
					}
					auto remaining = received;
					const char* cursor = buffer.data();
					while (remaining > 0)
					{
						const auto sent = send(connection, cursor, static_cast<int>(remaining), 0);
						if (sent <= 0)
						{
							return;
						}
						cursor += sent;
						remaining -= sent;
					}
				}
				closeBenchSocket(connection);
			});
			return true;
		}

		void stop()
		{
			stopping.store(true);
			if (listener != INVALID_BENCH_SOCKET)
			{
				closeBenchSocket(listener);
			}
			if (worker.joinable())
			{
				worker.join();
			}
		}

        //! @brief Construct all members with explicit defaults.
        EchoServer()
            : listener ( INVALID_BENCH_SOCKET )
            , port     ( 0 )
            , worker   ()
            , stopping ( false )
        {
        }
	};

	wse::xpt::OperationContext context()
	{
		return wse::xpt::OperationContext(
			wse::xpt::Timeout(std::chrono::milliseconds(2000)));
	}
}

int main()
{
#if defined( _WIN32 )
	WSADATA wsa = {};
	require(WSAStartup(MAKEWORD(2, 2), &wsa) == 0, "Winsock initialization");
#endif

	EchoServer server;
	if (!server.start())
	{
		std::fprintf(stderr, "echo server failed to start\n");
		return 1;
	}

	{
		wse::xpt::TcpClient client;
		if (!client.connect(wse::xpt::Endpoint("127.0.0.1", server.port), context()).succeeded())
		{
			std::fprintf(stderr, "loopback connect failed\n");
			server.stop();
			return 1;
		}

		// --- TCP RTT: 64Byteを送り、同じ64Byteが返るまで.
		const std::vector<std::uint8_t> ping(64, 0x5A);
		wse_bench::run("xpt.tcp.rtt_64B", 20, 300, [&] {
			require(client.send(ping, context()).succeeded(), "TCP ping send");
			std::size_t received = 0;
			while (received < ping.size())
			{
				const auto chunk = client.receive(ping.size() - received, context());
				require(chunk.succeeded() && !chunk.value().empty(), "TCP ping receive");
				for (const auto byte : chunk.value()) { require(byte == 0x5A, "TCP ping data"); }
				received += chunk.value().size();
			}
		});

		// --- TCP Throughput: 256KiBの往復.
		const std::vector<std::uint8_t> block(256 * 1024, 0xA5);
		wse_bench::run("xpt.tcp.echo_throughput_256KiB", 3, 30, [&] {
			require(client.send(block, context()).succeeded(), "TCP block send");
			std::size_t received = 0;
			while (received < block.size())
			{
				const auto chunk = client.receive(block.size() - received, context());
				require(chunk.succeeded() && !chunk.value().empty(), "TCP block receive");
				for (const auto byte : chunk.value()) { require(byte == 0xA5, "TCP block data"); }
				received += chunk.value().size();
			}
		}, static_cast<std::uint64_t>(block.size()) * 2ULL);
	}
	server.stop();

	// --- UDP送信Throughput: 受け手はOSのBufferに任せ、送信側Costだけを測る.
	{
		const BenchSocket sinkSocket = socket(AF_INET, SOCK_DGRAM, 0);
		require(sinkSocket != INVALID_BENCH_SOCKET, "UDP socket");
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = 0;
		require(bind(sinkSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "UDP bind");
		socklen_t length = sizeof(address);
		require(getsockname(sinkSocket, reinterpret_cast<sockaddr*>(&address), &length) == 0, "UDP endpoint");
		const std::uint16_t udpPort = ntohs(address.sin_port);

		wse::xpt::UdpClient sender;
		const std::vector<std::uint8_t> datagram(1200, 0x3C);
		const wse::xpt::Endpoint target("127.0.0.1", udpPort);
		wse_bench::run("xpt.udp.send_1200B", 20, 500, [&] {
			require(sender.sendTo(target, datagram, context()).succeeded(), "UDP send (not delivery)");
		}, datagram.size());
		closeBenchSocket(sinkSocket);
	}

	// --- Cancellation伝搬: 要求から観測可能になるまで.
	{
		// One sample is 1000 constructions/cancellations on the same thread, not a worker wakeup.
		wse_bench::run("xpt.cancellation.create_cancel_observe_batch1000", 10, 200, [&] {
			for (int i = 0; i < 1000; ++i)
			{
				wse::xpt::CancellationSource source;
				const wse::xpt::CancellationToken token = source.token();
				source.cancel();
				require(token.isCancellationRequested(), "same-thread cancellation");
			}
		});
	}

	// --- RetryPolicyの1判定Cost.
	{
		const wse::xpt::RetryPolicy policy;
		const wse::xpt::TransportError error(
			wse::xpt::eTransportErrorCategory::Timeout,
			wse::xpt::eTransportErrorCode::TimedOut, "bench", 0);
		wse_bench::run("xpt.retry.no_retry_decision_batch1000", 10, 200, [&] {
			for (int i = 0; i < 1000; ++i)
			{
				const auto decision = policy.evaluate(1U,
					wse::xpt::eRetryOperationSafety::Idempotent,
					wse::xpt::eRetryFailureDisposition::Retryable,
					error);
				require(!decision.shouldRetry(), "default retry decision");
			}
		});
	}

	std::printf("# xpt benchmark complete\n");
#if defined( _WIN32 )
	WSACleanup();
#endif
	return 0;
}

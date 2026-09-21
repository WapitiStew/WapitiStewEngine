// @file engine/wse/test/benchmark/benchmark_support.h
// @brief 全Benchmarkが共有する計測Harness。
// @details 固定回数のWarmupの後にM回計測し、p50／p95／meanと毎秒処理数をTSVで出力する.
//          出力行は `WSE_BENCH<TAB>name<TAB>...` の機械可読形式で、CIのArtifactとして保存され
//          Commit間の比較に使う. 絶対値の合否判定はしない - 環境差が大きいため、比較は同一Runner内で
//          行うという設計 (doc: PERF-001/PERF-002) である. 乱数は使わず、入力はすべて決定的である.

#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace wse_bench
{
	using Clock = std::chrono::steady_clock;

	//! 1回の呼び出しを計測対象として、warmup後にmeasured回実行し統計をTSVで1行出力する.
	//! bytes_per_iteration_in を与えるとThroughput (bytes/s) も併記する.
	template <typename Callable>
	void run(const char* const name_in, const int warmup_in, const int measured_in,
		Callable&& callable_in, const std::uint64_t bytes_per_iteration_in = 0)
	{
		for (int i = 0; i < warmup_in; ++i)
		{
			callable_in();
		}
		std::vector<std::uint64_t> samples;
		samples.reserve(static_cast<std::size_t>(measured_in));
		for (int i = 0; i < measured_in; ++i)
		{
			const Clock::time_point begin = Clock::now();
			callable_in();
			samples.push_back(static_cast<std::uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin).count()));
		}
		std::sort(samples.begin(), samples.end());
		std::uint64_t total = 0;
		for (const std::uint64_t sample : samples)
		{
			total += sample;
		}
		const std::uint64_t p50 = samples[samples.size() / 2];
		const std::uint64_t p95 = samples[(samples.size() * 95) / 100];
		const double mean = static_cast<double>(total) / static_cast<double>(samples.size());
		const double ops_per_second = mean > 0.0 ? 1.0e9 / mean : 0.0;
		if (bytes_per_iteration_in > 0)
		{
			const double bytes_per_second = ops_per_second * static_cast<double>(bytes_per_iteration_in);
			std::printf("WSE_BENCH\t%s\t%d\tp50_ns=%llu\tp95_ns=%llu\tmean_ns=%.0f\tbytes_per_s=%.0f\n",
				name_in, measured_in,
				static_cast<unsigned long long>(p50), static_cast<unsigned long long>(p95),
				mean, bytes_per_second);
		}
		else
		{
			std::printf("WSE_BENCH\t%s\t%d\tp50_ns=%llu\tp95_ns=%llu\tmean_ns=%.0f\tops_per_s=%.1f\n",
				name_in, measured_in,
				static_cast<unsigned long long>(p50), static_cast<unsigned long long>(p95),
				mean, ops_per_second);
		}
		std::fflush(stdout);
	}
}

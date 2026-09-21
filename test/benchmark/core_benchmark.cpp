// @file engine/wse/test/benchmark/core_benchmark.cpp
// @brief Coreの主要Operationの性能を計測する (PERF-001).
// @details Matrixの乗算と逆行列、ImageのBit深度Cast、Mapの1MiB Copy、Loggingの1行Costを測る.
//          入力は固定Seed生成の決定的な値で、結果の消費 (volatile相当の集約) によって
//          最適化での消去を防ぐ. 判定はしない - 出力TSVがCommit間比較の材料である.

#include <wse/stew.h>

#include "benchmark_support.h"

#include <cstdint>

namespace
{
	volatile double sink = 0;

	std::uint64_t lcg_state = 0x9E3779B97F4A7C15ULL;
	double nextValue()
	{
		lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<double>((lcg_state >> 33) % 1000) / 10.0 - 50.0;
	}

	wse::Matrix makeMatrix(const std::size_t dimension_in, const bool dominant_in)
	{
		wse::Matrix matrix(dimension_in, dimension_in);
		for (std::size_t row = 0; row < dimension_in; ++row)
		{
			for (std::size_t col = 0; col < dimension_in; ++col)
			{
				matrix[row][col] = nextValue();
			}
			if (dominant_in)
			{
				matrix[row][row] += 1000.0;
			}
		}
		return matrix;
	}
}

int main()
{
	// --- Matrix 64x64 乗算. O(n^3)の素朴実装の現在地を記録する.
	{
		const wse::Matrix left = makeMatrix(64, false);
		const wse::Matrix right = makeMatrix(64, false);
		wse_bench::run("core.matrix.multiply_64", 3, 30, [&] {
			const wse::Matrix product = left * right;
			sink += product[0][0];
		});
	}

	// --- Matrix 48x48 逆行列. 対角優位で可逆を保証する. LU化後のGauss-Jordanの現在地.
	{
		const wse::Matrix matrix = makeMatrix(48, true);
		wse_bench::run("core.matrix.inverse_48", 3, 30, [&] {
			const wse::Matrix inverse = matrix.tryInverse().value();
			sink += inverse[0][0];
		});
	}

	// --- Image縮小Cast 3c64 -> 3c08, 640x480. 右Shiftによるほぼ帯域律速の変換.
	{
		wse::img3c64_t wide(640, 480);
		wide[0][0][0] = UINT64_MAX;
		const std::uint64_t bytes = 640ULL * 480ULL * 3ULL * (8ULL + 1ULL);
		wse_bench::run("core.image.reduce_3c64_to_3c08_640x480", 2, 20, [&] {
			const wse::img3c08_t narrow =
				wse::castData::image<wse::ePixFormat::CH3D08, wse::ePixFormat::CH3D64>(wide);
			sink += narrow[0][0][0];
		}, bytes);
	}

	// --- Image拡大Cast 3c08 -> 3c16, 640x480.
	{
		wse::img3c08_t narrow(640, 480);
		narrow[0][0][0] = 200;
		const std::uint64_t bytes = 640ULL * 480ULL * 3ULL * (1ULL + 2ULL);
		wse_bench::run("core.image.enlarge_3c08_to_3c16_640x480", 2, 20, [&] {
			const wse::img3c16_t wide =
				wse::castData::image<wse::ePixFormat::CH3D16, wse::ePixFormat::CH3D08>(narrow);
			sink += wide[0][0][0];
		}, bytes);
	}

	// --- Map 1MiBのCopy構築. Buffer copyの現在地.
	{
		wse::U8_MAP source(1024, 1024);
		source[1023][1023] = 137;
		wse_bench::run("core.map.copy_1MiB", 3, 50, [&] {
			const wse::U8_MAP copy(source);
			sink += copy[1023][1023];
		}, 1024ULL * 1024ULL);
	}

	// Unregistered logging has no output; batch it above the clock resolution.
	{
		wse_bench::run("core.log.unconfigured_batch1000", 10, 200, [&] {
			for (int i = 0; i < 1000; ++i) { wse::WLog() << "benchmark log line " << 42; }
		});
		// The runner captures stdout: this measures formatting and pipe output, not disk I/O.
		wse::registDefaultLog();
		wse_bench::run("core.log.stdout_single_line", 10, 200, [&] {
			wse::WLog() << "benchmark log line " << 42;
		});
	}

	std::printf("# sink=%.9g\n", static_cast<double>(sink));
	return 0;
}

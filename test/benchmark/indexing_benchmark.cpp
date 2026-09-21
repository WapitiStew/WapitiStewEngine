// @file indexing_benchmark.cpp
// @brief Release comparison of unchecked indexed scans with direct storage pointer scans.
#include "benchmark_support.h"
#include <wse/stew.h>
#include <array>
#include <cstdint>

#if defined(_MSC_VER)
#define WSE_BENCH_NOINLINE __declspec(noinline)
#else
#define WSE_BENCH_NOINLINE __attribute__((noinline))
#endif

namespace
{
volatile double checksum = 0;
// Pair each measurement and alternate its order to distribute clock/load drift across both paths.
template<class PointerScan, class IndexedScan>
void compare(const char* const pointer_name_in, const char* const indexed_name_in,
             PointerScan&& pointer_scan_in, IndexedScan&& indexed_scan_in, const std::uint64_t bytes_in)
{
    constexpr int measured = 200;
    constexpr int warmup = 20;
    std::array<std::array<std::uint64_t, measured>, 2> samples{};
    for (int iteration = -warmup; iteration < measured; ++iteration)
    {
        for (int step = 0; step < 2; ++step)
        {
            const auto variant = static_cast<std::size_t>((iteration + warmup + step) % 2);
            const auto begin = wse_bench::Clock::now();
            checksum = variant == 0 ? pointer_scan_in() : indexed_scan_in();
            const auto elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                wse_bench::Clock::now() - begin).count());
            if (iteration >= 0) { samples[variant][static_cast<std::size_t>(iteration)] = elapsed; }
        }
    }
    for (std::size_t variant = 0; variant < 2; ++variant)
    {
        auto& values = samples[variant];
        std::sort(values.begin(), values.end());
        std::uint64_t total = 0;
        for (const auto value : values) { total += value; }
        const double mean = static_cast<double>(total) / measured;
        std::printf("WSE_BENCH\t%s\t%d\tp50_ns=%llu\tp95_ns=%llu\tmean_ns=%.0f\tbytes_per_s=%.0f\n",
            variant == 0 ? pointer_name_in : indexed_name_in, measured,
            static_cast<unsigned long long>(values[measured / 2]),
            static_cast<unsigned long long>(values[measured * 95 / 100]), mean,
            mean > 0.0 ? static_cast<double>(bytes_in) * 1.0e9 / mean : 0.0);
    }
}

template<bool Indexed>
WSE_BENCH_NOINLINE double sumMatrix(const wse::Matrix& matrix_in)
{
    double sum = 0;
    for (std::size_t y = 0; y < matrix_in.height(); ++y)
    {
        const auto* row = Indexed ? matrix_in[y] : matrix_in.elements() + y * matrix_in.width();
        for (std::size_t x = 0; x < matrix_in.width(); ++x) { sum += row[x]; }
    }
    return sum;
}
template<bool Indexed>
WSE_BENCH_NOINLINE std::uint64_t sumImage(const wse::img3c8_t& image_in)
{
    std::uint64_t sum = 0;
    for (std::size_t y = 0; y < image_in.height(); ++y)
    {
        const auto* row = Indexed ? image_in[y] : image_in.elements() + y * image_in.width();
        for (std::size_t x = 0; x < image_in.width(); ++x)
        {
            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                if constexpr (Indexed) { sum += row[x][channel]; }
                else { sum += row[x].data[channel]; }
            }
        }
    }
    return sum;
}
} // namespace

int main()
{
    // Matrix exports floating-point specializations in shared builds, not Matrix_<uint32_t>.
    wse::Matrix matrix(512, 512);
    for (std::size_t i = 0; i < matrix.size(); ++i) { matrix.elements()[i] = static_cast<double>(i % 251); }
    wse::img3c8_t image(640, 480);
    for (std::size_t i = 0; i < image.size(); ++i)
    {
        for (std::size_t channel = 0; channel < 3; ++channel)
        { image.elements()[i].data[channel] = static_cast<std::uint8_t>((i + channel) % 251); }
    }
    if (sumMatrix<true>(matrix) != sumMatrix<false>(matrix) || sumImage<true>(image) != sumImage<false>(image))
    { return 1; }
    // Both paths have identical traversal order and input; only the access spelling changes.
    compare("matrix_f64_pointer_512x512", "matrix_f64_indexed_512x512",
            [&] { return sumMatrix<false>(matrix); }, [&] { return sumMatrix<true>(matrix); }, matrix.memory_size());
    compare("image_pointer_640x480x3", "image_indexed_640x480x3",
            [&] { return sumImage<false>(image); }, [&] { return sumImage<true>(image); }, image.memory_size());
    return 0;
}

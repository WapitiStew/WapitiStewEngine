// Copyright (C) 2026 WapitiStew. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <gef/error/GefError.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace wse { namespace gef { namespace detail {

inline GefStatus limitFailure()
{
    return GefStatus::failure(GefError(eGefErrorCategory::Resource,
        eGefErrorCode::LimitExceeded, "the configured GEF read budget was exceeded"));
}

inline GefStatus writeFailure()
{
    return GefStatus::failure(GefError(eGefErrorCategory::Io,
        eGefErrorCode::WriteFailed, "the file write, flush, close or replacement failed"));
}

inline GefStatus finishWrite(std::ofstream& file_inout)
{
    file_inout.flush();
    const bool flushed = static_cast<bool>(file_inout);
    file_inout.close();
    return flushed && !file_inout.fail() ? GefStatus::success() : writeFailure();
}

// Exclusive sibling directory owns its one staging file. Only trusted, caller-controlled
// directories and ordinary files are supported. Cleanup is best effort after storage faults.
struct StagedFile
{
    std::filesystem::path directory;
    std::filesystem::path file;
    ~StagedFile()
    {
        if (directory.empty()) return;
        try
        {
            std::error_code ignored;
            std::filesystem::remove(file, ignored);
            std::filesystem::remove(directory, ignored);
        }
        catch (...) {} // Cleanup must not mask the original failure, including allocation failure.
    }
};

template<class Writer>
GefStatus atomicWrite(const std::string& path_in, const Writer& writer_in)
{
    namespace fs = std::filesystem;
    const fs::path destination(path_in);
    auto parent = destination.parent_path();
    if (parent.empty()) parent = ".";
    static std::atomic<std::uint64_t> sequence{0};
    StagedFile staged;
    for (unsigned attempt = 0; attempt < 64; ++attempt)
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        auto candidate = parent / (".wse-save-" + std::to_string(stamp) + "-" +
            std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)));
        auto candidate_file = candidate / "data";
        std::error_code error;
        if (fs::create_directory(candidate, error))
        {
            staged.directory.swap(candidate);
            staged.file.swap(candidate_file);
            break;
        }
        if (error) return writeFailure();
    }
    if (staged.directory.empty()) return writeFailure();
    const auto& temporary = staged.file;
    const auto status = writer_in(temporary.string());
    if (!status.succeeded()) return status;
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING))
        return writeFailure();
#else
    std::error_code error;
    fs::rename(temporary, destination, error);
    if (error) return writeFailure();
#endif
    return GefStatus::success();
}

} } }

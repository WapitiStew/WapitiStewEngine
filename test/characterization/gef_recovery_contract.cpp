// Copyright (C) 2026 WapitiStew. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
// Bounded reads, transactional state and failure-safe replacement via public GEF APIs.
#include <gef/stew.h>
#include "../../core/gef/FileOperations.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
int failures = 0;
void expect(bool condition_in, const char* message_in)
{
    if (!condition_in) { std::cerr << "FAILED: " << message_in << '\n'; ++failures; }
}
std::string bytes(const std::string& path_in)
{
    std::ifstream stream(path_in, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
void fixture(const std::string& path_in, const std::string& bytes_in)
{
    std::ofstream stream(path_in, std::ios::binary | std::ios::trunc);
    stream.write(bytes_in.data(), static_cast<std::streamsize>(bytes_in.size()));
}
bool limited(const wse::gef::GefStatus& status_in)
{
    return !status_in.succeeded() && status_in.error().category() == wse::gef::eGefErrorCategory::Resource &&
        status_in.error().code() == wse::gef::eGefErrorCode::LimitExceeded;
}
bool writeFailed(const wse::gef::GefStatus& status_in)
{
    return !status_in.succeeded() && status_in.error().category() == wse::gef::eGefErrorCategory::Io &&
        status_in.error().code() == wse::gef::eGefErrorCode::WriteFailed;
}
std::size_t stagingCount(const std::filesystem::path& path_in)
{
    std::size_t count = 0;
    for (const auto& item : std::filesystem::directory_iterator(path_in))
        if (item.path().filename().string().find(".wse-save-") == 0) ++count;
    return count;
}
}

int main(int argc_in, char** argv_in)
{
    if (argc_in != 2) return 2;
    using namespace wse::gef;
    namespace fs = std::filesystem;
    const fs::path root(argv_in[1]);
    fs::create_directories(root);
    const auto input = (root / "input").string();
    const auto output = (root / "output").string();
    const std::string wire("\x03\x02\0\0\0\0\0\0\0\x34\x12\xfe\xff", 13);
    fixture(input, wire);
    BINController bin;
    bin.setContents(std::vector<int32_t>{99});
    ReadLimits limits;
    limits.max_input_bytes = 13; limits.max_elements = 2; limits.max_blocks = 1;
    expect(bin.readWithLimits(input, limits).succeeded(), "BIN exact inclusive limits");
    expect(bin.index_num() == 1 && bin.contents<int16_t>(0) == std::vector<int16_t>({4660, -2}),
        "BIN read replaces existing state");
    expect(bin.write(output).succeeded() && bytes(output) == wire, "BIN read reconstructs writer order");
    expect(!bin.read((root / "missing" / "input").string()).succeeded() &&
        bin.write(output).succeeded() && bytes(output) == wire, "BIN open failure preserves writable state");
    for (int budget = 0; budget < 3; ++budget)
    {
        auto reduced = limits;
        if (budget == 0) --reduced.max_input_bytes;
        if (budget == 1) --reduced.max_elements;
        if (budget == 2) --reduced.max_blocks;
        expect(limited(bin.readWithLimits(input, reduced)), "BIN rejects one beyond each budget");
        expect(bin.write(output).succeeded() && bytes(output) == wire, "BIN budget failure keeps full writer state");
    }
    fixture(input, wire + wire.substr(0, 12));
    expect(!bin.read(input).succeeded(), "BIN truncated second block fails");
    expect(bin.write(output).succeeded() && bytes(output) == wire, "BIN partial read rolls back");
    fixture(input, std::string("\x07", 1) + std::string(8, '\xff'));
    expect(limited(bin.readWithLimits(input, ReadLimits{})), "BIN maximum count refused before payload allocation");
    fixture(input, wire);
    expect(bin.read(input).succeeded() && bin.read(input).succeeded() && bin.index_num() == 1,
        "BIN repeated read replaces rather than duplicates");
    bin.setContents(std::vector<int16_t>{});
    bin.setContents(std::vector<int16_t>{8});
    expect(bin.write(input).succeeded(), "BIN read then setContents writes");
    const auto withEmpty = bytes(input);
    expect(bin.read(input).succeeded() && bin.index_num() == 3 && bin.contents<int16_t>(2) == std::vector<int16_t>{8},
        "BIN empty and repeated tags preserve indices");
    expect(bin.write(output).succeeded() && bytes(output) == withEmpty, "BIN empty order survives read then write");
    BINController moved(std::move(bin));
    expect(bin.index_num() == 0 && moved.index_num() == 3, "BIN move leaves a reusable empty source");
    bin.setContents(std::vector<uint8_t>{7});
    expect(bin.index_num() == 1 && bin.contents<uint8_t>(0) == std::vector<uint8_t>{7}, "BIN moved source restarts at zero");
    fixture(input, "");
    ReadLimits zero; zero.max_input_bytes = zero.max_elements = zero.max_blocks = 0;
    expect(moved.readWithLimits(input, zero).succeeded() && moved.index_num() == 0, "BIN empty file replaces with zero budgets");
    BINController everyType;
    everyType.setContents(std::vector<uint8_t>{255});
    everyType.setContents(std::vector<int8_t>{-128});
    everyType.setContents(std::vector<int16_t>{-32768});
    everyType.setContents(std::vector<int32_t>{123456789});
    everyType.setContents(std::vector<int64_t>{-1234567890123LL});
    everyType.setContents(std::vector<wse::float32_t>{1.25F});
    everyType.setContents(std::vector<wse::float64_t>{-3.5});
    everyType.setContents(std::vector<uint8_t>{});
    everyType.setContents(std::vector<int32_t>{-7});
    expect(everyType.write(input).succeeded(), "all-type rewrite fixture writes");
    const auto everyWire = bytes(input);
    expect(everyType.read(input).succeeded() && everyType.write(output).succeeded() && bytes(output) == everyWire,
        "all seven types, empty and repeated blocks rewrite byte-identically");

    const std::string table = "Key,Category,Num,Remark,Param\nk,int32,1,n,7\n";
    fixture(input, table);
    CSVController csv;
    ReadLimits csvLimits;
    csvLimits.max_input_bytes = table.size(); csvLimits.max_rows = 2;
    csvLimits.max_cells = 10; csvLimits.max_cell_bytes = 8;
    expect(csv.readReplace(input, csvLimits).succeeded(), "CSV exact inclusive limits");
    const auto original = csv.contents();
    expect(!csv.readReplace((root / "missing" / "input").string()).succeeded() && csv.contents() == original,
        "CSV open failure preserves table");
    for (int budget = 0; budget < 4; ++budget)
    {
        auto reduced = csvLimits;
        if (budget == 0) --reduced.max_input_bytes;
        if (budget == 1) --reduced.max_rows;
        if (budget == 2) --reduced.max_cells;
        if (budget == 3) --reduced.max_cell_bytes;
        expect(limited(csv.readReplace(input, reduced)), "CSV rejects one beyond each budget");
        expect(csv.contents() == original, "CSV failed replacement preserves all cells");
    }
    expect(limited(csv.readWithLimits(input, csvLimits)) && csv.contents() == original,
        "CSV append budget includes retained rows");
    auto appendLimits = csvLimits; appendLimits.max_rows = 4; appendLimits.max_cells = 19;
    expect(limited(csv.readWithLimits(input, appendLimits)) && csv.contents() == original,
        "CSV partial append is rolled back when cumulative cells exceed budget");
    ++appendLimits.max_cells;
    expect(csv.readWithLimits(input, appendLimits).succeeded() && csv.contents().size() == 4,
        "CSV bounded append accepts exact cumulative limits");
    expect(csv.readReplace(input).succeeded() && csv.contents() == original, "CSV replacement drops old rows");
    expect(csv.read(input).succeeded() && csv.contents().size() == 4, "CSV legacy read retains append behavior");
    expect(csv.readReplace(input).succeeded(), "CSV recovers after previous calls");
    for (unsigned count = 0; count < 4; ++count)
    {
        std::string header;
        for (unsigned i = 0; i < count; ++i) header += "H,";
        fixture(input, header + "\nk,int32,1,n,7\n");
        expect(csv.readReplace(input).succeeded(), "CSV raw parser accepts short label rows");
        const auto result = csv.toDataMAP2D();
        expect(!result.succeeded() && result.error().code() == eGefErrorCode::MalformedData,
            "CSV map rejects every header shorter than four cells");
    }
    fixture(input, "a,,\n\n,x");
    expect(csv.readReplace(input).succeeded() && csv.contents() ==
        std::vector<std::vector<std::string>>{{"a", ""}, {}, {"", "x"}},
        "CSV preserves empty rows, adjacent cells, omitted trailing cell and final unterminated row");
    fixture(input, "");
    ReadLimits csvZero; csvZero.max_input_bytes = csvZero.max_rows = csvZero.max_cells = csvZero.max_cell_bytes = 0;
    expect(csv.readReplace(input, csvZero).succeeded() && csv.contents().empty(), "CSV zero budgets accept empty replacement");

    // Atomic save: compare sentinel bytes after failures, then retry on the same controllers.
    fixture(input, table); expect(csv.readReplace(input).succeeded(), "CSV save fixture loads");
    const auto stagingBefore = stagingCount(root);
    fixture(output, "keep original");
    expect(writeFailed(detail::atomicWrite(output, [](const std::string& temporary_in) {
        fixture(temporary_in, "partial output"); return detail::writeFailure();
    })) && bytes(output) == "keep original", "failed staged write never replaces original");
    bool threw = false;
    try { (void)detail::atomicWrite(output, [](const std::string& temporary_in) -> GefStatus {
        fixture(temporary_in, "partial output"); throw std::runtime_error("injected writer exception");
    }); } catch (const std::runtime_error&) { threw = true; }
    expect(threw && bytes(output) == "keep original", "exception during staging preserves original");
    const auto blocked = root / "blocked";
    fs::create_directories(blocked); fixture((blocked / "sentinel").string(), "keep");
    expect(writeFailed(csv.writeAtomic(blocked.string())) && bytes((blocked / "sentinel").string()) == "keep",
        "native replacement failure preserves destination directory");
    expect(csv.writeAtomic(output).succeeded(), "CSV atomic replacement retry succeeds");
    CSVController saved; expect(saved.readReplace(output).succeeded() && saved.contents() == csv.contents(),
        "CSV atomic replacement stores complete table");
    expect(bin.writeAtomic(output).succeeded(), "BIN atomically replaces existing file");
    BINController savedBin; expect(savedBin.read(output).succeeded() && savedBin.contents<uint8_t>(0) == std::vector<uint8_t>{7},
        "BIN atomic replacement stores complete blocks");
    const auto fresh = (root / "fresh").string(); fs::remove(fresh);
    expect(bin.writeAtomic(fresh).succeeded() && bytes(fresh) == bytes(output), "atomic save creates a new destination");
    expect(stagingCount(root) == stagingBefore, "success, error and exception remove owned staging paths");

    struct FailingSync : std::streambuf { int sync() override { return -1; } } failing;
    std::ofstream stream(input, std::ios::binary);
    auto* nativeBuffer = static_cast<std::ostream&>(stream).rdbuf(&failing);
    expect(writeFailed(detail::finishWrite(stream)), "flush failure is not reported as success");
    static_cast<std::ostream&>(stream).rdbuf(nativeBuffer);
#ifndef _WIN32
    expect(writeFailed(bin.write("/dev/full")), "BIN reports native late ENOSPC");
    expect(writeFailed(csv.write("/dev/full")), "CSV reports native late ENOSPC");
    const auto retained = csv.contents();
    const auto readFailure = csv.read(root.string());
    expect(!readFailure.succeeded() && readFailure.error().code() == eGefErrorCode::ReadFailed && csv.contents() == retained,
        "CSV native read error retains previous state");
#endif
    for (const auto& path : {input, output, fresh, (blocked / "sentinel").string()}) fs::remove(path);
    fs::remove(blocked);
    return failures == 0 ? 0 : 1;
}

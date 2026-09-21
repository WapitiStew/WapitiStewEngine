// @file engine/wse/test/characterization/gef_bin_contract.cpp
// @brief GEF binary blocks preserve type, order, values, and append behavior.
// @details BINController is the on-disk format WSE uses for saved settings and captured numeric
//          series, so a file it writes outlives the process that wrote it. A failure here is a
//          stored-data compatibility break rather than a local defect: a block written as float32
//          and read back as int32, a block index that shifts when a file is appended to, or a
//          malformed header that is accepted instead of refused all corrupt data that has already
//          left the program. The test pins four things - the block count and index order survive a
//          write/read cycle, every supported element type survives its extreme values, append adds
//          a block at the next free index without disturbing the earlier ones, and the error
//          contracts callers branch on: last_index() before the first block is a usage violation
//          reported with std::logic_error, and a missing file or an unknown block category is an
//          operational failure reported through the GefResult contract, never as plausible data.

#include <gef/stew.h>

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
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

    // contents<T>() resolves an index against the typed stores in a fixed order, so asking for the
    // wrong T does not fail loudly - it returns whatever store owns that index, converted. Reading
    // each block back through the type it was written with is therefore part of the assertion.
    template <typename T>
    void expectBlock(
        const wse::gef::BINController& binary_in,
        const std::size_t index_in,
        const std::vector<T>& expected_in,
        const char* const message_in)
    {
        expect(binary_in.contents<T>(index_in) == expected_in, message_in);
    }
}

int main(const int argc_in, char** const argv_in)
{
    if (argc_in != 2)
    {
        std::cerr << "FAILED: output path is required\n";
        return 1;
    }

    // The scratch path comes from the build system so the file lands in the build tree. A file left
    // by an earlier run would be appended to rather than replaced later on, so it is removed first.
    const std::string path(argv_in[1]);
    std::remove(path.c_str());

    // last_index() names the newest block, so before any block exists it has no answer. Returning 0
    // would read to the caller as "block zero exists"; asking is a usage violation the caller can
    // avoid by checking index_num() first, so the contract is std::logic_error.
    wse::gef::BINController empty;
    bool rejectedEmpty = false;
    try
    {
        (void)empty.last_index();
    }
    catch (const std::logic_error&)
    {
        rejectedEmpty = true;
    }
    expect(rejectedEmpty, "An empty binary controller has no last index");

    // A path with no file behind it is an operational failure the caller branches on, so it must
    // arrive as a failed GefStatus with the file-open code rather than as an empty controller.
    {
        wse::gef::BINController missing;
        const wse::gef::GefStatus status = missing.read(path);
        expect(
            !status.succeeded() &&
                status.error().code() == wse::gef::eGefErrorCode::FileOpenFailed,
            "A missing binary file reports FileOpenFailed");
    }

    // One block per supported element type, each carrying its type's minimum, zero, and maximum. The
    // extremes are the payload: a writer or reader that lost a byte, sign-extended, or truncated to
    // a narrower width still round-trips the middle values, and only fails on the saturating ones.
    const std::vector<std::uint8_t> u8{0U, 42U, 255U};
    const std::vector<std::int8_t> i8{-128, 0, 127};
    const std::vector<std::int16_t> i16{-32768, 0, 32767};
    const std::vector<std::int32_t> i32{-123456789, 0, 123456789};
    const std::vector<std::int64_t> i64{-1234567890123LL, 0, 1234567890123LL};
    const std::vector<wse::float32_t> f32{-2.5F, 0.0F, 1.25F};
    const std::vector<wse::float64_t> f64{-3.5, 0.0, 2.25};

    // Blocks are addressed by the order they were stored, not by their type, so seven setContents()
    // calls of seven different types must still leave seven blocks numbered 0 through 6.
    wse::gef::BINController written;
    written.setContents(u8);
    written.setContents(i8);
    written.setContents(i16);
    written.setContents(i32);
    written.setContents(i64);
    written.setContents(f32);
    written.setContents(f64);
    expect(written.index_num() == 7U, "Seven typed blocks are retained");
    expect(written.last_index() == 6U, "Last index follows insertion order");
    expect(written.write(path).succeeded(), "Writing seven blocks succeeds");

    // A controller that never saw the in-memory data must recover the same block count and, index by
    // index, the same element type and values. This is the round trip the stored file format exists
    // for; every check below it fails if the type byte or the element count is written or parsed
    // differently from the way it is read back.
    wse::gef::BINController read;
    expect(read.read(path).succeeded(), "Reading the written file succeeds");
    expect(read.index_num() == 7U, "Round-trip restores the block count");
    expectBlock(read, 0U, u8, "uint8 block round-trips");
    expectBlock(read, 1U, i8, "int8 block round-trips");
    expectBlock(read, 2U, i16, "int16 block round-trips");
    expectBlock(read, 3U, i32, "int32 block round-trips");
    expectBlock(read, 4U, i64, "int64 block round-trips");
    expectBlock(read, 5U, f32, "float32 block round-trips");
    expectBlock(read, 6U, f64, "float64 block round-trips");

    // write() with is_update_in == true appends instead of truncating, so a controller that holds
    // only one block can extend a file it never read. The appended block must become index 7 - the
    // next free slot in the file - rather than index 0, its position within the appending controller.
    const std::vector<std::uint8_t> appended{7U, 8U, 9U};
    wse::gef::BINController append;
    append.setContents(appended);
    expect(append.write(path, true).succeeded(), "Appending a block succeeds");

    wse::gef::BINController combined;
    expect(combined.read(path).succeeded(), "Reading the appended file succeeds");
    expect(combined.index_num() == 8U, "Append adds one ordered block");
    expectBlock(combined, 7U, appended, "Appended block is readable");

    // A hand-built file whose block header is structurally well formed - one category byte and a
    // 64-bit element count - but whose category is 0, the reserved eCategory::None that no writer
    // emits. Nothing but an explicit category check catches this shape: the header parses, so a
    // reader without one would fall through and read the rest of the file at an unknown element
    // width, producing plausible numbers from arbitrary bytes.
    {
        std::ofstream malformed(path, std::ios::binary | std::ios::trunc);
        const std::uint8_t unknownCategory = 0U;
        const std::uint64_t emptyCount = 0U;
        malformed.write(reinterpret_cast<const char*>(&unknownCategory), sizeof(unknownCategory));
        malformed.write(reinterpret_cast<const char*>(&emptyCount), sizeof(emptyCount));
    }
    // A read() that returned success here - the exact silent-acceptance failure this guards
    // against - fails the check; only the malformed-data code counts as the rejection.
    {
        wse::gef::BINController malformed;
        const wse::gef::GefStatus status = malformed.read(path);
        expect(
            !status.succeeded() &&
                status.error().code() == wse::gef::eGefErrorCode::MalformedData,
            "An unknown binary block category is rejected");
    }

    // GEF-BIN-01: compare against wire bytes specified independently of this writer/reader.
    // A round trip alone cannot detect a matching byte-order or count-width mistake in both.
    const std::vector< std::uint8_t > wireBytes{
        0x03U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x34U, 0x12U, 0xFEU, 0xFFU
    };
    {
        wse::gef::BINController writer;
        writer.setContents( std::vector< std::int16_t >{ 4660, -2 } );
        expect( writer.write( path ).succeeded(), "The reference Integer16 block writes" );
        std::ifstream file( path, std::ios::binary );
        const std::vector< std::uint8_t > actual{
            std::istreambuf_iterator< char >( file ), std::istreambuf_iterator< char >()
        };
        expect( actual == wireBytes, "GEF-BIN-01 pins tag, little-endian count and signed samples" );
    }
    {
        std::ofstream fixture( path, std::ios::binary | std::ios::trunc );
        fixture.write(
              reinterpret_cast< const char* >( wireBytes.data() )
            , static_cast< std::streamsize >( wireBytes.size() ) );
    }
    {
        wse::gef::BINController reader;
        expect( reader.read( path ).succeeded(), "An independently encoded block reads" );
        expect( reader.index_num() == 1U, "The wire fixture contains one block" );
        expectBlock(
              reader, 0U, std::vector< std::int16_t >{ 4660, -2 }
            , "The wire fixture decodes the signed samples" );
    }
    // A count of two with the final sample cut short must not return plausible success.
    {
        std::ofstream fixture( path, std::ios::binary | std::ios::trunc );
        fixture.write(
              reinterpret_cast< const char* >( wireBytes.data() )
            , static_cast< std::streamsize >( wireBytes.size() - 1U ) );
    }
    {
        wse::gef::BINController reader;
        const wse::gef::GefStatus status = reader.read( path );
        expect(
              !status.succeeded() && status.error().code() == wse::gef::eGefErrorCode::ReadFailed
            , "GEF-BIN-01 refuses a truncated sample" );
    }

    std::remove(path.c_str());
    return failures == 0 ? 0 : 1;
}

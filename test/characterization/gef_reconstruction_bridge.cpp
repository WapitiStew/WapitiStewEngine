// Public-API bridge for the independent codec in test/reconstruction/gef_reference.py.
// Keep binary parsing/packing out of this process: WSE and the reference must agree
// on externally produced files, not just on their own round trips.
#include <gef/stew.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void require(const bool condition_in, const char* const message_in)
    {
        if (!condition_in) throw std::runtime_error(message_in);
    }

    template<typename T>
    void block(const wse::gef::BINController& reader_in, const std::size_t index_in,
               const std::vector<T>& values_in)
    {
        require(reader_in.contents<T>(index_in) == values_in, "BIN values/index differ");
    }

    void write(const std::string& path_in)
    {
        wse::gef::BINController writer;
        writer.setContents(std::vector<std::uint8_t>{0, 42, 255});
        writer.setContents(std::vector<std::int8_t>{-128, 0, 127});
        writer.setContents(std::vector<std::int16_t>{});
        writer.setContents(std::vector<std::int16_t>{4660, -2});
        writer.setContents(std::vector<std::int32_t>{-123456789, 0, 123456789});
        writer.setContents(std::vector<std::int64_t>{-1234567890123LL, 0, 1234567890123LL});
        writer.setContents(std::vector<wse::float32_t>{-2.5F, 0.0F, 1.25F});
        writer.setContents(std::vector<wse::float64_t>{-3.5, 0.0, 2.25});
        require(writer.write(path_in).succeeded(), "BIN write failed");
        wse::gef::BINController append;
        append.setContents(std::vector<std::uint8_t>{7, 8, 9});
        require(append.write(path_in, true).succeeded(), "BIN append failed");
    }

    void verify(const std::string& path_in)
    {
        wse::gef::BINController reader;
        require(reader.read(path_in).succeeded(), "Reference BIN read failed");
        require(reader.index_num() == 9 && reader.last_index() == 8, "BIN block count differs");
        block<std::uint8_t>(reader, 0, {0, 42, 255});
        block<std::int8_t>(reader, 1, {-128, 0, 127});
        block<std::int16_t>(reader, 2, {});
        block<std::int16_t>(reader, 3, {4660, -2});
        block<std::int32_t>(reader, 4, {-123456789, 0, 123456789});
        block<std::int64_t>(reader, 5, {-1234567890123LL, 0, 1234567890123LL});
        block<wse::float32_t>(reader, 6, {-2.5F, 0.0F, 1.25F});
        block<wse::float64_t>(reader, 7, {-3.5, 0.0, 2.25});
        block<std::uint8_t>(reader, 8, {7, 8, 9});
    }

    void csv(const std::string& input_in, const std::string& output_in)
    {
        wse::gef::CSVController reader;
        require(reader.read(input_in).succeeded(), "Reference CSV read failed");
        const std::vector<std::vector<std::string>> expected{
            {"Key", "Category", "Num", "Remark", "Param0", "Param1"},
            {"entry_a", "enum", "2", " first ", "10", "20"},
            {"entry_b", "int32", "2", "", "30", "40"},
            {"entry_a", "int16", "1", "\"literal\"", "99"}};
        require(reader.contents() == expected, "CSV cell interpretation differs");
        const auto result = reader.toDataMAP2D();
        require(result.succeeded(), "CSV settings conversion failed");
        const wse::gef::datamap_2d expectedMap{
            {"entry_a", {{"Category", {"int16"}}, {"Num", {"1"}},
                         {"Remark", {"\"literal\""}}, {"Param", {"99"}}}},
            {"entry_b", {{"Category", {"int32"}}, {"Num", {"2"}},
                         {"Remark", {""}}, {"Param", {"30", "40"}}}}};
        require(result.value() == expectedMap, "CSV settings/duplicate keys differ");
        require(reader.write(output_in).succeeded(), "CSV write failed");
    }
}

int main(const int argc_in, char** const argv_in)
{
    try
    {
        require(argc_in >= 3, "Expected mode and path");
        const std::string mode(argv_in[1]);
        const std::string path(argv_in[2]);
        if (mode == "write") write(path);
        else if (mode == "verify") verify(path);
        else if (mode == "csv")
        {
            require(argc_in == 4, "Expected CSV output path");
            csv(path, argv_in[3]);
        }
        else if (mode == "reject-format" || mode == "reject-read")
        {
            wse::gef::BINController reader;
            const auto status = reader.read(path);
            const auto code = mode == "reject-format"
                ? wse::gef::eGefErrorCode::MalformedData : wse::gef::eGefErrorCode::ReadFailed;
            require(!status.succeeded() && status.error().code() == code,
                    "Malformed BIN did not report the expected code");
        }
        else require(false, "Unknown mode");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}

//*****************************************************************************************************************
//! 
//! @file    csvController.cpp
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Arg-30, 2025   Create New     WapitiStew.
//!
//!
//! @brief   \~japanese CSV設定Fileの読み書きとDataMap化の実装.
//! @brief   \~english  Implements CSV setting-file reading, writing, and conversion into the data map.
//!
//*****************************************************************************************************************

#include "../../../api/gef/csv/csvController.h"

#include <wse/stew.h>


// C/C++
#include <iostream>
#include <fstream>
#include <sstream>
#include "../FileOperations.h"

namespace wse
{
namespace gef
{   //--------------------------------------------------------------------------------
    // Constractor / Destructor
    //--------------------------------------------------------------------------------
    //
    // @brief  Default constructor.
    //
    CSVController::CSVController()
    {}
    //
    // @brief  Destructor.
    //
    CSVController::~CSVController()
    {}


    //--------------------------------------------------------------------------------
    // Specific Method
    //------------------------------------------------------------------------------

    // 
    // @brief  Read.
    //
    // @param [in ] path_in
    // 
    using CsvRows = std::vector<std::vector<std::string>>;

    static GefStatus readCsv(CsvRows& rows_inout, const std::string& path_in, const ReadLimits& limits_in)
    {
        std::ifstream file(path_in);
        if (!file.is_open())
            return GefStatus::failure(GefError(eGefErrorCategory::Io,
                eGefErrorCode::FileOpenFailed, "the CSV file cannot be opened"));
        uint64_t cells = 0;
        if (rows_inout.size() > limits_in.max_rows) return detail::limitFailure();
        for (const auto& row : rows_inout)
        {
            if (row.size() > limits_in.max_cells - cells) return detail::limitFailure();
            cells += row.size();
            for (const auto& cell : row)
                if (cell.size() > limits_in.max_cell_bytes) return detail::limitFailure();
        }
        // Copy only after validating the retained append state. Publication is one swap.
        CsvRows staged = rows_inout;
        std::vector<std::string> row;
        std::string cell;
        uint64_t bytes = 0;
        bool line_started = false;
        bool cell_pending = false;
        const auto append_cell = [&]() {
            if (cells == limits_in.max_cells) return false;
            row.push_back(std::move(cell));
            cell.clear();
            ++cells;
            return true;
        };
        while (file.peek() != EOF)
        {
            if (bytes == limits_in.max_input_bytes || staged.size() >= limits_in.max_rows)
                return detail::limitFailure();
            char value = 0;
            if (!file.get(value)) break;
            ++bytes;
            if (value == '\n')
            {
                if (cell_pending && !append_cell()) return detail::limitFailure();
                staged.push_back(std::move(row));
                row.clear();
                line_started = cell_pending = false;
            }
            else
            {
                line_started = true;
                if (value == ',')
                {
                    if (!append_cell()) return detail::limitFailure();
                    cell_pending = false;
                }
                else
                {
                    if (cell.size() == limits_in.max_cell_bytes) return detail::limitFailure();
                    cell.push_back(value);
                    cell_pending = true;
                }
            }
        }
        if (file.bad() || !file.eof())
            return GefStatus::failure(GefError(eGefErrorCategory::Io,
                eGefErrorCode::ReadFailed, "the CSV stream failed"));
        if (line_started)
        {
            if (cell_pending && !append_cell()) return detail::limitFailure();
            staged.push_back(std::move(row));
        }
        file.clear();
        file.close();
        if (file.fail())
            return GefStatus::failure(GefError(eGefErrorCategory::Io,
                eGefErrorCode::ReadFailed, "the CSV stream cannot be closed"));
        rows_inout.swap(staged);
        return GefStatus::success();
    }

    GefStatus CSVController::read(const std::string& path_in)
    {
        return readWithLimits(path_in, ReadLimits::unlimited());
    }

    GefStatus CSVController::readWithLimits(const std::string& path_in, const ReadLimits& limits_in)
    {
        return readCsv(m_contents, path_in, limits_in);
    }

    GefStatus CSVController::readReplace(const std::string& path_in, const ReadLimits& limits_in)
    {
        CsvRows staged;
        const auto status = readCsv(staged, path_in, limits_in);
        if (status.succeeded()) m_contents.swap(staged);
        return status;
    }

    // 
    // @brief  Read.
    //
    // @param [in ] path_in
    // 
    GefStatus CSVController::write ( const std::string& path_in )
    {
        std::ofstream ofs( path_in, std::ios::binary); // BOMを書けるようにbinary
        if (!ofs)
        {
            return GefStatus::failure( GefError(
                eGefErrorCategory::Io, eGefErrorCode::FileOpenFailed, "the CSV file cannot be opened" ) );
        }
        for ( const auto& row : this->m_contents ) 
        {
            for ( std::size_t i = 0; i < row.size(); ++i ) 
            {
                ofs << row[i] << ",";
            }
            ofs << "\n";
        }
        return detail::finishWrite(ofs);
    }
    

    // 
    // @brief  toDataFrameMap.
    // 
    // @details
    // 
    // Labal1| Labal2   |Labal3  |Labal4
    // ------+----------+--------+--------
    // Key1  | Category1|Param1 |Remark1
    // Key2  | Category2|Param2 |Remark2
    // Key3  | Category3|Param3 |Remark3
    // Key4  | Category4|Param4 |Remark4
    //
    // @param [in ] Path.
    // 
    GefResult< datamap_2d > CSVController::toDataMAP2D ( void )
    {
        if( this->m_contents.size() < 2 )
        {
            return GefResult< datamap_2d >::failure( GefError(
                eGefErrorCategory::Format, eGefErrorCode::NoData, "the CSV holds no data rows" ) );
        }
        
        datamap_2d map_2d;
        std::vector<std::string> labels = this->m_contents[ 0 ];

        if (labels.size() < SETTING_HEADER)
            return GefResult<datamap_2d>::failure(GefError(eGefErrorCategory::Format,
                eGefErrorCode::MalformedData, "the CSV label row has fewer than four cells"));

        for( size_t row = 1; row < this->m_contents.size(); row++ )
        {
            if( this->m_contents[ row ].size() <= SETTING_HEADER )
            {
                return GefResult< datamap_2d >::failure( GefError(
                    eGefErrorCategory::Format, eGefErrorCode::MalformedData,
                    "a CSV data row has fewer columns than the setting header" ) );
            }
            std::string key = this->m_contents[ row ][ 0 ];
            std::map<std::string, std::vector< std::string > > param_map;
            for( size_t col = 1; col < SETTING_HEADER; col++ )
            {
                std::vector< std::string > param_list;
                param_list.emplace_back( this->m_contents[ row ][ col ] );
                param_map[ labels[ col ] ] = param_list;
            }
            
            std::vector< std::string > param_list;
            for( size_t col = SETTING_HEADER; col < this->m_contents[ row ].size(); ++col )
            {
                param_list.emplace_back( this->m_contents[ row ][ col ] );
            }
            param_map[ SETTING_PARAM ] = param_list;

            map_2d[ key ] = param_map;
        }
        return GefResult< datamap_2d >::success( map_2d );
    }

    GefStatus CSVController::writeAtomic(const std::string& path_in)
    {
        return detail::atomicWrite(path_in, [this](const std::string& temporary_in) {
            return write(temporary_in);
        });
    }

};
};


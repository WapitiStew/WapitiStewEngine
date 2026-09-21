// Copyright (C) 2026 WapitiStew. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <limits>

namespace wse { namespace gef {

//! \~japanese GEF読込の包含上限。0は該当内容を拒否する。メモリ使用量そのものの上限ではない。
//! \~english Inclusive GEF read budgets. Zero rejects that content; these are not heap-byte limits.
struct ReadLimits
{
    std::uint64_t max_input_bytes = 64U * 1024U * 1024U; //!< \~japanese 読込StreamのByte数。 \~english Bytes consumed from the input stream.
    std::uint64_t max_blocks = 1024U * 1024U; //!< \~japanese BIN Block数。 \~english BIN blocks.
    std::uint64_t max_elements = 4U * 1024U * 1024U; //!< \~japanese BIN総要素数。 \~english Total BIN elements.
    std::uint64_t max_rows = 1024U * 1024U; //!< \~japanese 結果CSV行数。 \~english Resulting CSV rows.
    std::uint64_t max_cells = 4U * 1024U * 1024U; //!< \~japanese 結果CSV総Cell数。 \~english Total resulting CSV cells.
    std::uint64_t max_cell_bytes = 1024U * 1024U; //!< \~japanese CSV各CellのByte数。 \~english Bytes per CSV cell.

    //! \~japanese 従来read用の無制限Budget。 \~english Unbounded budgets used by the legacy read entry point.
    static ReadLimits unlimited() noexcept
    {
        const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
        return {maximum, maximum, maximum, maximum, maximum, maximum};
    }
};

} }

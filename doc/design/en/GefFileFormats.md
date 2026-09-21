# WSE GEF File Formats

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Purpose, responsibilities and supported use

`WSE::Gef` provides synchronous, caller-confined file controllers. It depends on Core, has no
worker or callback, and exposes no OS file handle. `BINController` stores typed numeric blocks;
`CSVController` reads a simple setting table and converts it into a key-indexed map.
Neither controller interprets device-specific settings.

| Public declaration | Implementation | Stored state |
| --- | --- | --- |
| [BINController](../../../api/gef/bin/binController.h) | [BIN implementation](../../../core/gef/bin/binController.cpp) | Per-type arrays of block-index/value pairs, next block index, writer order list |
| [CSVController](../../../api/gef/csv/csvController.h) | [CSV implementation](../../../core/gef/csv/csvController.cpp) | Owned vector of rows of strings |
| [SettingFormat](../../../api/gef/format/SettingFormat.h) | Header definitions | Type tags, fixed-column count and `datamap_2d` |
| [GefError](../../../api/gef/error/GefError.h) | [Error implementation](../../../core/gef/error/GefError.cpp) | Stable category/code and diagnostic information |

The portable interchange below is scoped to supported little-endian Windows/Linux x86-64 and
ARM64 builds. It does not certify big-endian hosts. Controllers own copied values, and local
streams close when the operation ends. Mutating one controller concurrently is unsupported.

## Binary layout: GEF-BIN-01

A file is consecutive blocks without a global magic number, version, checksum or alignment padding.
End-of-file at a block boundary terminates the sequence. Block indices are their zero-based file
order; no index is stored on disk.

| Relative offset | Field | Encoding |
| --- | --- | --- |
| 0 | Category | 1 unsigned byte from the table below |
| 1 | Element count N | 8-byte unsigned little-endian integer |
| 9 | Payload | N adjacent samples, each with the category's width |
| 9 + N * width | Next block | Immediately follows payload |

| Tag | Category | Width | Sample interpretation |
| --- | --- | --- | --- |
| 1 | Enum | 1 | Unsigned 8-bit integer |
| 2 | Integer08 | 1 | Signed 8-bit integer |
| 3 | Integer16 | 2 | Signed 16-bit little endian |
| 4 | Integer32 | 4 | Signed 32-bit little endian |
| 5 | Integer64 | 8 | Signed 64-bit little endian |
| 6 | Float32 | 4 | IEEE-754 binary32, little endian |
| 7 | Float64 | 8 | IEEE-754 binary64, little endian |

Signed multi-byte values use the supported host's two's-complement representation. Tag 0 and
unrecognized tags return `Format/MalformedData`. A truncated count or payload returns
`Io/ReadFailed`. Counts describe elements, not bytes.

The complete encoding of one Integer16 block holding [4660, -2] is:

```text
03 | 02 00 00 00 00 00 00 00 | 34 12 FE FF
tag          count=2             samples
```

This literal is shared with the [binary contract](../../../test/characterization/gef_bin_contract.cpp),
which checks writer bytes independently of a reader and reads a hand-authored fixture.
A reader can reconstruct the stream by reading 1 byte, 8 bytes, then N * width bytes, repeating
until a block boundary reaches EOF. Use `readWithLimits` to bound input bytes, block count and
total elements before decoding payloads; the subtraction/division checks avoid count multiplication overflow.

## Binary state and operation sequence: GEF-BIN-02

```text
writer: new controller -> setContents(block A) -> setContents(block B) -> write(path)
reader: new controller -> read(path) -> check result -> contents<T>(block index)
append: new writer -> setContents(new blocks) -> write(path, true)
verify: new reader -> read(appended file) -> inspect old and appended block indices
```

`setContents` copies each block into a type-specific store and appends its type/count to the
writer order list. Each call consumes one index, including an empty block. `write(path,false)`
truncates the destination; `write(path,true)` appends the writer's blocks without rewriting old
blocks. The next reader assigns indices over the whole file.

`read` and `readWithLimits` decode into a temporary controller, including the complete writer
order list. Only a successful read swaps that state into the receiver. Existing values are replaced;
any returned error or allocation exception preserves all previous values, indices and writer order.
An empty successful read clears the controller. Reading, adding blocks and writing again is supported.
`setContents` reserves both stores before publishing a block; allocation failure leaves logical
contents unchanged. Moving a BIN controller leaves its source empty and reusable.

`contents<T>(index)` returns an owned vector, converting stored values to T; it is not a checked
type-identity query. An empty result cannot distinguish a missing index from an empty block.
`last_index()` requires at least one block; otherwise it throws `std::logic_error`.

Direct `write` truncates or appends in place, but checks write/flush/close failures. A failed
direct write can leave a partial destination. Use `writeAtomic` for replacement with failure recovery,
as specified in GEF-IO-04 below. Appending is not transactional.

## Setting CSV grammar: GEF-CSV-01

The supported portable input is a BOM-free, LF-delimited table of comma-separated byte strings.
Use UTF-8 for non-ASCII text. The controller does not validate or transcode the encoding.
Quotes have no special meaning: quoted commas, multiline cells and escape sequences are not
implemented. The reader does not trim whitespace. Platform text-mode CRLF handling differs, so
cross-platform fixtures use LF.

The first row supplies labels. Data column 0 is a row key; columns 1, 2 and 3 become single-string
values under their corresponding labels. Columns 4 onward become an ordered vector under
`SETTING_PARAM` (`"Param"`), irrespective of their individual header labels.

```text
Key,Category,Num,Remark,Param0,Param1
camera_a,enum,2,primary,10,20
```

The resulting `datamap_2d` entry is:

```text
"camera_a" -> {
    "Category": ["enum"],
    "Num":      ["2"],
    "Remark":   ["primary"],
    "Param":   ["10", "20"]
}
```

`Category` and `Num` remain strings; conversion does not enforce a numeric type or compare Num
with the parameter count. `convertCategory` is a separate exact, case-sensitive lookup for
`enum`, `int08`, `int16`, `int32`, `int64`, `float32` and `float64`; unknown text maps to None.
The map owns its strings and does not borrow the CSV table.

## CSV state and validation: GEF-CSV-02

`read` retains its existing append semantics, including any incoming header row. `readWithLimits`
also appends, but with explicit budgets; `readReplace` replaces the entire table, using bounded
`ReadLimits{}` defaults unless supplied otherwise. Each operation stages changes and swaps only after
successful input and close checks. A failed read or allocation exception leaves the table unchanged.
An empty append changes no rows; an empty replacement clears the table. Append does not merge or
skip headers. For a new settings table, use `readReplace`.

`contents()` is a const reference valid while its owner remains alive and unmodified.
`toDataMAP2D` needs a label row and at least one data row; otherwise it returns `Format/NoData`.
It validates the label row has at least four cells before indexing labels, then requires at least
five cells in every data row. A short label or data row returns `Format/MalformedData`.
Raw reading does not impose this application-table shape; conversion does.

- Adjacent delimiters produce empty cells; a trailing delimiter does not create an additional
  final empty cell. Blank lines become empty rows, and an unterminated final line is retained.
- Duplicate row keys overwrite the earlier map entry. Duplicate fixed-column labels collide;
  callers should use distinct labels and reserve `Param` for the parameter vector.
- `write` emits every cell followed by a comma and each row followed by LF in binary mode, on
  both Windows and Linux. It performs no quoting or escaping. Input uses native text-mode
  translation; LF fixtures are portable.
- General quoted CSV, schema validation and arbitrary encoding validation remain unsupported.

## Bounded input and failure recovery: GEF-IO-04

[ReadLimits](../../../api/gef/format/ReadLimits.h) contains inclusive budgets:

<div class="wse-gef-budgets" style="max-width:100%; overflow-x:auto;"><div style="min-width:42rem;">

| Field | Default | Scope |
| --- | --- | --- |
| <code style="white-space:nowrap;">max_input_bytes</code> | <span style="white-space:nowrap;">67,108,864</span> | Bytes extracted from the current input stream, including delimiters and newlines |
| <code style="white-space:nowrap;">max_blocks</code> | <span style="white-space:nowrap;">1,048,576</span> | BIN blocks, including empty blocks |
| <code style="white-space:nowrap;">max_elements</code> | <span style="white-space:nowrap;">4,194,304</span> | BIN total elements across all types |
| <code style="white-space:nowrap;">max_rows</code> | <span style="white-space:nowrap;">1,048,576</span> | Resulting CSV rows, including header and empty rows |
| <code style="white-space:nowrap;">max_cells</code> | <span style="white-space:nowrap;">4,194,304</span> | Resulting CSV total cells |
| <code style="white-space:nowrap;">max_cell_bytes</code> | <span style="white-space:nowrap;">1,048,576</span> | Bytes in each CSV cell |

</div></div>

Zero admits no corresponding content. Irrelevant fields are ignored. CSV append budgets include
retained rows/cells and validate retained cell lengths; input bytes count only the incoming stream.
CSV text translation occurs before this byte count, so it is not a physical-file-size bound on Windows.
Budgets bound logical data, not total heap use: vector/string overhead, spare capacity, the old state
and staged copies coexist. Choose smaller limits for constrained applications. Input streams may read
ahead internally. No elapsed-time or cancellation deadline is provided, and concurrently modified inputs
are not snapshots. Legacy `read(path)` uses `ReadLimits::unlimited()` for compatibility; select the
bounded APIs for external input. A budget rejection returns `Resource/LimitExceeded` and preserves state.
BIN checks the announced count against remaining bytes/elements before allocating payload values.
A budget rejection may precede a truncation error if the announced payload already exceeds a budget.

Both controllers provide `writeAtomic(path)` for ordinary local files in trusted, caller-controlled
directories. The implementation creates an exclusively reserved sibling staging directory, writes its
`data` file, checks flush and close, and replaces the destination with the same-filesystem native rename
operation (Windows `MoveFileExW` with replacement; Linux rename). It never deletes the original first.
A failure or exception before successful replacement preserves the prior destination and controller;
retry the same controller after resolving the cause. New destinations are also supported. Owned staging
paths are removed on success, failure and exception; cleanup is best effort if storage access itself fails.

This is replacement only: there is no atomic append, filesystem synchronization, power-loss durability,
network-filesystem guarantee or protection against a hostile directory owner. Replacement creates a new
file identity; existing permissions, hard links and other metadata are not preserved. Concurrent writers
are not serialized; the last successful replacement wins. Keep the directory stable during the call.
The native call must support same-filesystem replacement; unsupported or denied replacement returns an error.

Failed open returns `Io/FileOpenFailed`, input/close failure `Io/ReadFailed`, and output/flush/close/rename
failure `Io/WriteFailed`. Staging-directory creation failure also uses `Io/WriteFailed`.
Allocation/length failures remain standard exceptions, including allocation while constructing an error;
GEF does not claim an allocation-free error path. Read state and the destination of `writeAtomic` remain
unchanged on these exceptions before publication; direct writes have no such file rollback guarantee.
See the [error-handling recipe](../../en/ErrorHandlingCookbook.md) for a bounded reload/save example.

## Errors, verification and extension boundary

GEF operational results use [ResultContract](ResultContract.md). Inspect `error()` only on a
failed strict result. A usage precondition such as empty `last_index()` uses a standard exception.
GEF does not add timeout, cancellation, retries or language bindings.

<div class="wse-gef-evidence" style="max-width:100%; overflow-x:auto;"><div style="min-width:48rem;">

| Contract | Executable evidence | Limit |
| --- | --- | --- |
| GEF-BIN-01 | `wse.gef.bin_contract`: literal wire vector, independent input, unknown tag and truncated payload | Does not certify arbitrary malformed counts or big-endian hosts |
| GEF-BIN-02 | Same test: all seven types, block order, append, missing file and empty index | Read/write recovery is covered separately by GEF-IO-04 |
| GEF-CSV-01 | [`wse.gef.csv_contract`](../../../test/characterization/gef_csv_contract.cpp): cells and DataMap grouping | No quoted CSV support |
| GEF-CSV-02 | Same test: missing file, empty data and parsed-cell round trip | Not a complete parser-fuzz or storage-failure gate |
| GEF-IO-04 | [`wse.gef.recovery_contract`](../../../test/characterization/gef_recovery_contract.cpp): exact/over/zero budgets, maximum count, read/append rollback, short headers, rewrite/move, failed staging/rename/flush and retry; Linux also uses `/dev/full` and directory-read failure | Controlled fault injection is not a real full-disk or power-loss test on Windows; no exhaustive OOM/fuzz/metadata/concurrent-writer certification |
| GEF-RECON-03 | `wse.gef.reconstruction`: independent codec exchanges BIN/CSV files with WSE | Requires Python interpreter and GEF; bounded fixtures, not blind developer acceptance |

</div></div>

## Independent format exercise: GEF-RECON-03

[gef_reference.py](../../../test/reconstruction/gef_reference.py) uses only the Python standard
library: `struct` with explicit little-endian widths for BIN, and simple delimiter processing for
CSV. It imports no WSE binding or implementation. The separate
[native bridge](../../../test/characterization/gef_reconstruction_bridge.cpp) uses only public GEF
APIs and does not implement wire parsing. CTest registers `wse.gef.reconstruction` when GEF and a
Python interpreter are available; it does not require the WSE Python binding.

1. Check the reference encoder and decoder against the literal Integer16 vector above.
2. Write seven type tags, an empty block and a repeated-tag appended block with the reference codec;
   read them using a fresh WSE controller and compare typed values and indices.
3. Write/append the same sequence with fresh WSE writers; decode with the reference and compare
   all values, block order and exact BIN bytes with the independent encoding.
4. Give both readers an unknown tag, truncated count and truncated payload. Verify reference
   rejection and WSE's specified error code; this does not exercise unbounded hostile counts.
5. Exchange CSV with preserved spaces, adjacent empty cells, literal quotes, differing parameter
   counts and duplicate row keys. Compare cells and the last-wins settings map. Normalize native
   output CRLF to LF defensively for this comparison; WSE writes LF, and generic CRLF input portability is not guaranteed.

This executable exercise is evidence of independent-code interoperability. Its author has reviewed
the existing implementation, so it does not establish that a first-time developer can reconstruct
the format without questions. That separate review is defined in [Design Verification](DesignVerification.md).

Do not add an unmarked version field or change tags in this unversioned format. A future format
needs an explicit identification/migration plan. Application-specific schemas remain outside GEF.
See [Design Verification](DesignVerification.md) for execution and reconstruction acceptance.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |

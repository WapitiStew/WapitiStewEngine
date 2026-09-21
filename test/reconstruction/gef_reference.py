"""GEF format interoperability using only the documented layout and Python stdlib.

No WSE binding or implementation is imported. The native bridge is a separate
process. This is an independent codec, not a claim of blind third-party review.
"""

import argparse
import pathlib
import struct
import subprocess
import tempfile


# GefFileFormats.md: tag, uint64 LE element count, then packed numeric elements.
FORMATS = {1: "B", 2: "b", 3: "h", 4: "i", 5: "q", 6: "f", 7: "d"}
BLOCKS = [
    (1, [0, 42, 255]),
    (2, [-128, 0, 127]),
    (3, []),
    (3, [4660, -2]),
    (4, [-123456789, 0, 123456789]),
    (5, [-1234567890123, 0, 1234567890123]),
    (6, [-2.5, 0.0, 1.25]),
    (7, [-3.5, 0.0, 2.25]),
]
APPENDED = (1, [7, 8, 9])
ROWS = [
    ["Key", "Category", "Num", "Remark", "Param0", "Param1"],
    ["entry_a", "enum", "2", " first ", "10", "20"],
    ["entry_b", "int32", "2", "", "30", "40"],
    ["entry_a", "int16", "1", '"literal"', "99"],
]


def encode_bin(blocks):
    encoded = bytearray()
    for tag, values in blocks:
        encoded += struct.pack("<BQ", tag, len(values))
        encoded += struct.pack("<" + FORMATS[tag] * len(values), *values)
    return bytes(encoded)


def decode_bin(encoded):
    blocks = []
    offset = 0
    while offset < len(encoded):
        if len(encoded) - offset < 9:
            raise ValueError("truncated block header")
        tag, count = struct.unpack_from("<BQ", encoded, offset)
        offset += 9
        if tag not in FORMATS:
            raise ValueError("unknown block tag")
        width = struct.calcsize("<" + FORMATS[tag])
        # A bounded reference reader; this is not a WSE allocation-limit promise.
        if count > (len(encoded) - offset) // width:
            raise ValueError("truncated block payload")
        values = list(struct.unpack_from("<" + FORMATS[tag] * count, encoded, offset))
        offset += count * width
        blocks.append((tag, values))
    return blocks


def encode_csv(rows):
    # WSE writes a delimiter after every cell. Input uses portable LF bytes.
    return "".join(",".join(row) + ",\n" for row in rows).encode("utf-8")


def decode_csv(encoded):
    # Normalize native text-mode output only, not general CSV input semantics.
    lines = encoded.decode("utf-8").replace("\r\n", "\n").split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    rows = []
    for line in lines:
        cells = line.split(",") if line else []
        if line.endswith(","):
            cells.pop()  # getline does not append a final empty cell.
        rows.append(cells)
    return rows


def settings(rows):
    if not rows or len(rows[0]) < 4:
        raise ValueError("missing setting header")
    result = {}
    for row in rows[1:]:
        if len(row) < 5:
            raise ValueError("missing setting parameters")
        entry = {rows[0][column]: [row[column]] for column in range(1, 4)}
        entry["Param"] = row[4:]
        result[row[0]] = entry
    return result


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def run(bridge, scratch):
    def native(*args):
        subprocess.run([str(bridge), *map(str, args)], check=True, timeout=15)

    literal = bytes.fromhex("03 02 00 00 00 00 00 00 00 34 12 FE FF")
    require(encode_bin([(3, [4660, -2])]) == literal, "Integer16 literal encode")
    require(decode_bin(literal) == [(3, [4660, -2])], "Integer16 literal decode")
    expected = BLOCKS + [APPENDED]
    reference_path = scratch / "reference.bin"
    native_path = scratch / "native.bin"
    reference_path.write_bytes(encode_bin(expected))
    native("verify", reference_path)
    native("write", native_path)
    require(decode_bin(native_path.read_bytes()) == expected, "native blocks decoded")
    require(native_path.read_bytes() == reference_path.read_bytes(), "native wire bytes")

    malformed = [
        (bytes.fromhex("00 00 00 00 00 00 00 00 00"), "reject-format"),
        (literal[:5], "reject-read"),
        (literal[:-1], "reject-read"),
    ]
    for index, (encoded, mode) in enumerate(malformed):
        try:
            decode_bin(encoded)
        except ValueError:
            pass
        else:
            raise AssertionError("reference accepted malformed BIN")
        path = scratch / ("malformed-%d.bin" % index)
        path.write_bytes(encoded)
        native(mode, path)

    reference_csv = scratch / "reference.csv"
    native_csv = scratch / "native.csv"
    reference_csv.write_bytes(encode_csv(ROWS))
    require(decode_csv(reference_csv.read_bytes()) == ROWS, "reference CSV cells")
    native("csv", reference_csv, native_csv)
    actual_rows = decode_csv(native_csv.read_bytes())
    require(actual_rows == ROWS, "native CSV cells after newline normalization")
    require(settings(actual_rows) == {
        "entry_a": {"Category": ["int16"], "Num": ["1"],
                    "Remark": ['"literal"'], "Param": ["99"]},
        "entry_b": {"Category": ["int32"], "Num": ["2"],
                    "Remark": [""], "Param": ["30", "40"]},
    }, "CSV settings and last duplicate key wins")
    print("GEF reconstruction: seven tags, empty block, append, malformed BIN, CSV cells/map PASS")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--bridge", type=pathlib.Path, required=True)
    parser.add_argument("--scratch", type=pathlib.Path, required=True)
    arguments = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="gef-reference-", dir=arguments.scratch) as directory:
        run(arguments.bridge.resolve(), pathlib.Path(directory))

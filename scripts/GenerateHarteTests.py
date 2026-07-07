#!/usr/bin/env python3
"""
GenerateHarteTests.py — Download Tom Harte SingleStepTests JSON and convert
to compact binary test data files for the Casso unit test runner.

Source: https://github.com/SingleStepTests/65x02
Output: UnitTest/<cpu>/XX.bin  (one file per opcode, lowercase hex)

Binary format per file:
  Header (4 bytes):
    uint16_le  vector_count
    uint8      opcode
    uint8      reserved (0)

  Per vector:
    uint8      name_length
    char[]     name (ASCII, no null terminator)
    -- initial state --
    uint16_le  pc
    uint8      s, a, x, y, p
    uint8      ram_count
    per ram: uint16_le address, uint8 value
    -- final state --
    (same layout as initial)
"""

import argparse
import json
import os
import struct
import sys
import urllib.request
import tempfile
import shutil


# All 151 legal NMOS 6502 opcodes
LEGAL_6502_OPCODES = [
    0x00, 0x01, 0x05, 0x06, 0x08, 0x09, 0x0A, 0x0D, 0x0E,
    0x10, 0x11, 0x15, 0x16, 0x18, 0x19, 0x1D, 0x1E,
    0x20, 0x21, 0x24, 0x25, 0x26, 0x28, 0x29, 0x2A, 0x2C, 0x2D, 0x2E,
    0x30, 0x31, 0x35, 0x36, 0x38, 0x39, 0x3D, 0x3E,
    0x40, 0x41, 0x45, 0x46, 0x48, 0x49, 0x4A, 0x4C, 0x4D, 0x4E,
    0x50, 0x51, 0x55, 0x56, 0x58, 0x59, 0x5D, 0x5E,
    0x60, 0x61, 0x65, 0x66, 0x68, 0x69, 0x6A, 0x6C, 0x6D, 0x6E,
    0x70, 0x71, 0x75, 0x76, 0x78, 0x79, 0x7D, 0x7E,
    0x81, 0x84, 0x85, 0x86, 0x88, 0x8A, 0x8C, 0x8D, 0x8E,
    0x90, 0x91, 0x94, 0x95, 0x96, 0x98, 0x99, 0x9A, 0x9D,
    0xA0, 0xA1, 0xA2, 0xA4, 0xA5, 0xA6, 0xA8, 0xA9, 0xAA, 0xAC, 0xAD, 0xAE,
    0xB0, 0xB1, 0xB4, 0xB5, 0xB6, 0xB8, 0xB9, 0xBA, 0xBC, 0xBD, 0xBE,
    0xC0, 0xC1, 0xC4, 0xC5, 0xC6, 0xC8, 0xC9, 0xCA, 0xCC, 0xCD, 0xCE,
    0xD0, 0xD1, 0xD5, 0xD6, 0xD8, 0xD9, 0xDD, 0xDE,
    0xE0, 0xE1, 0xE4, 0xE5, 0xE6, 0xE8, 0xE9, 0xEA, 0xEC, 0xED, 0xEE,
    0xF0, 0xF1, 0xF5, 0xF6, 0xF8, 0xF9, 0xFD, 0xFE,
]

# Undocumented NMOS 6502 opcodes that Casso actually implements (in
# CassoCore Cpu::InitializeUndocumented()). Harte's upstream data covers
# all 256 opcode bytes, but exercising an illegal opcode the CPU does not
# model would correctly fail — so this is the deliberately-small mirror of
# what Casso supports, and it grows as the CPU gains illegal opcodes.
#
# Keep in sync with BOTH:
#   - CassoCore/Cpu.cpp             Cpu::InitializeUndocumented()  (the impl)
#   - UnitTest/HarteTestRunner.cpp  the "Undocumented opcodes" TEST_METHODs
#
#   0x04  DOP zp   — double-NOP: read a zero-page byte and discard. 2B, 3c.
#   0xCF  DCP abs  — DEC memory then CMP A with the result.         3B, 6c.
IMPLEMENTED_ILLEGAL_6502_OPCODES = [
    0x04,
    0xCF,
]

BASE_URL = "https://raw.githubusercontent.com/SingleStepTests/65x02/main"


def download_json(cpu_type, opcode, temp_dir):
    """Download a single opcode JSON file. Returns the local path."""
    hex_name = f"{opcode:02x}"
    url = f"{BASE_URL}/{cpu_type}/v1/{hex_name}.json"
    local_path = os.path.join(temp_dir, f"{hex_name}.json")

    print(f"  Downloading {cpu_type}/v1/{hex_name}.json ...", end=" ", flush=True)
    urllib.request.urlretrieve(url, local_path)
    size_kb = os.path.getsize(local_path) / 1024
    print(f"{size_kb:.0f} KB")

    return local_path


def pack_cpu_state(state):
    """Pack a CPU state (initial or final) into binary bytes."""
    data = struct.pack("<H", state["pc"])
    data += struct.pack("B", state["s"])
    data += struct.pack("B", state["a"])
    data += struct.pack("B", state["x"])
    data += struct.pack("B", state["y"])
    data += struct.pack("B", state["p"])

    ram = state["ram"]
    data += struct.pack("B", len(ram))

    for addr, val in ram:
        data += struct.pack("<H", addr)
        data += struct.pack("B", val)

    return data


def pack_vector(vector):
    """Pack a single test vector into binary bytes."""
    name = vector["name"].encode("ascii")

    if len(name) > 15:
        name = name[:15]

    data = struct.pack("B", len(name))
    data += name
    data += pack_cpu_state(vector["initial"])
    data += pack_cpu_state(vector["final"])

    return data


def convert_opcode(cpu_type, opcode, temp_dir, output_dir, max_vectors):
    """Download JSON for one opcode and write the binary file."""
    json_path = download_json(cpu_type, opcode, temp_dir)

    with open(json_path, "r") as f:
        vectors = json.load(f)

    # Remove downloaded JSON immediately to save disk space
    os.remove(json_path)

    if max_vectors and max_vectors < len(vectors):
        vectors = vectors[:max_vectors]

    hex_name = f"{opcode:02x}"
    bin_path = os.path.join(output_dir, f"{hex_name}.bin")

    count = len(vectors)

    # Validate RAM entry counts
    for v in vectors:
        if len(v["initial"]["ram"]) > 32 or len(v["final"]["ram"]) > 32:
            print(f"  WARNING: {v['name']} has >32 RAM entries, skipping")
            vectors = [x for x in vectors if
                       len(x["initial"]["ram"]) <= 32 and
                       len(x["final"]["ram"]) <= 32]
            count = len(vectors)
            break

    # Header
    header = struct.pack("<H", count)
    header += struct.pack("B", opcode)
    header += struct.pack("B", 0)  # reserved

    with open(bin_path, "wb") as f:
        f.write(header)

        for v in vectors:
            f.write(pack_vector(v))

    size_kb = os.path.getsize(bin_path) / 1024
    print(f"  -> {hex_name}.bin: {count} vectors, {size_kb:.0f} KB")

    return count


def main():
    parser = argparse.ArgumentParser(
        description="Generate binary test data from Tom Harte SingleStepTests")
    parser.add_argument("--cpu", default="6502",
                        help="CPU type folder name (default: 6502)")
    parser.add_argument("--opcode", type=str, default=None,
                        help="Single opcode to generate (hex, e.g. A9)")
    parser.add_argument("--legal-only", action="store_true",
                        help="Generate only the 151 legal opcodes, "
                             "excluding implemented undocumented opcodes")
    parser.add_argument("--max-vectors", type=int, default=None,
                        help="Max vectors per opcode (default: all)")
    parser.add_argument("--output-dir", type=str, default=None,
                        help="Output directory (default: UnitTest/<cpu>/)")

    args = parser.parse_args()

    # Determine output directory
    if args.output_dir:
        output_dir = args.output_dir
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        repo_root = os.path.dirname(script_dir)
        output_dir = os.path.join(repo_root, "UnitTest", args.cpu)

    os.makedirs(output_dir, exist_ok=True)

    # Determine which opcodes to process. The default set is every opcode
    # Casso implements: the 151 legal opcodes plus the undocumented ones
    # wired into Cpu::InitializeUndocumented(). --legal-only drops the
    # illegals for a documented-only regression run; --opcode targets a
    # single byte (legal or not) for focused debugging.
    if args.opcode:
        opcodes = [int(args.opcode, 16)]
    elif args.legal_only:
        opcodes = list(LEGAL_6502_OPCODES)
    else:
        opcodes = sorted(LEGAL_6502_OPCODES + IMPLEMENTED_ILLEGAL_6502_OPCODES)

    print(f"CPU type:    {args.cpu}")
    print(f"Output dir:  {output_dir}")
    illegal_count = sum(
        1 for op in opcodes if op in IMPLEMENTED_ILLEGAL_6502_OPCODES)
    print(f"Opcodes:     {len(opcodes)} "
          f"({len(opcodes) - illegal_count} legal, "
          f"{illegal_count} undocumented)")

    if args.max_vectors:
        print(f"Max vectors: {args.max_vectors}")

    print()

    # Use a temp directory for downloads
    temp_dir = tempfile.mkdtemp(prefix="harte_")

    try:
        total_vectors = 0

        for opcode in opcodes:
            count = convert_opcode(
                args.cpu, opcode, temp_dir, output_dir, args.max_vectors)
            total_vectors += count

        print(f"\nDone: {len(opcodes)} opcodes, {total_vectors:,} total vectors")

    finally:
        # Clean up temp directory
        shutil.rmtree(temp_dir, ignore_errors=True)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
EmbedHarteVectors.py -- Write the Harte vectors the unit tests run, as C++
data compiled into the test DLL, so a unit test run reads no files.

The source is the checked-in reduced set under UnitTest/HarteVectors/. The
first N vectors of each opcode are kept (10 by default); see
ReduceHarteVectors.py for why the first N are a fair sample. The output is
UnitTest/HarteVectorSubset/, which is generated: rerun this script rather
than editing it.

The full set runs only on request, through scripts/RunHarteTests.ps1, when a
change could reasonably break the CPU or memory. See docs/testing.md.

Usage:
    EmbedHarteVectors.py [--vectors 10]
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from ReduceHarteVectors import FORMAT_VERSION, HEADER, skip_vector  # noqa: E402


CPUS = ('6502', 'rockwell65c02')


def cut(data, keep):
    """The file's bytes with only its first `keep` vectors."""
    count, opcode, version = HEADER.unpack_from(data, 0)

    if version != FORMAT_VERSION:
        raise ValueError('format version %d, expected %d' % (version, FORMAT_VERSION))

    written = min(count, keep)
    offset  = HEADER.size

    for _ in range(written):
        offset = skip_vector(data, offset)

    return opcode, HEADER.pack(written, opcode, FORMAT_VERSION) + data[HEADER.size:offset]


def format_bytes(data):
    """The bytes as C++ initializer lines."""
    lines = []

    for start in range(0, len(data), 24):
        chunk = data[start:start + 24]
        lines.append('    ' + ', '.join('0x%02X' % b for b in chunk) + ',')

    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--vectors', type=int, default=10,
                        help='vectors to keep per opcode (default: 10)')
    args = parser.parse_args()

    root    = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src_dir = os.path.join(root, 'UnitTest', 'HarteVectors')
    out_dir = os.path.join(root, 'UnitTest', 'HarteVectorSubset')
    parts   = []
    total   = 0

    os.makedirs(out_dir, exist_ok=True)

    # Each CPU's opcodes are split into PARTS_PER_CPU files of 256 / PARTS_PER_CPU
    # opcodes each, so every generated file stays under the 2,000-line limit for
    # a .cpp. The set of files is fixed, so the project lists them once.
    for cpu in CPUS:
        tag   = 'Nmos' if cpu == '6502' else 'Cmos'
        files = {}

        for name in sorted(n for n in os.listdir(os.path.join(src_dir, cpu)) if n.endswith('.bin')):
            with open(os.path.join(src_dir, cpu, name), 'rb') as f:
                opcode, data = cut(f.read(), args.vectors)

            files.setdefault(opcode // (256 // PARTS_PER_CPU), []).append((opcode, data))

        for part in range(PARTS_PER_CPU):
            function = 'GetHarteSubset%s%d' % (tag, part)
            arrays   = []
            entries  = []

            for opcode, data in files.get(part, []):
                symbol = 's_k%s_%02X' % (tag, opcode)
                arrays.append('static const Byte  %s[] =\n{\n%s\n};\n' % (symbol, format_bytes(data)))
                entries.append('    { "%s", 0x%02X, %s, sizeof (%s) },' % (cpu, opcode, symbol, symbol))

            total += len(entries)
            parts.append(function)

            text = [HEADER_TEXT % args.vectors]

            if arrays:
                text.append('\n\n\n\n\n'.join(arrays))
                text.append('\n\n\n\n\n')
                text.append('static const HarteSubsetEntry  s_kEntries[] =\n{\n%s\n};\n\n\n\n\n\n' % '\n'.join(entries))

            text.append(BANNER % function)
            text.append('std::span<const HarteSubsetEntry> %s()\n{\n' % function)
            text.append('    return std::span<const HarteSubsetEntry> (%s);\n}\n' % ('s_kEntries' if arrays else ''))

            write(os.path.join(out_dir, '%s.cpp' % function), ''.join(text))

    text = [HEADER_TEXT % args.vectors]
    text.append(''.join('std::span<const HarteSubsetEntry> %s();\n' % p for p in parts))
    text.append('\n\n\n\n\n')
    text.append('using SubsetPart = std::span<const HarteSubsetEntry> (*) ();\n\n\n\n\n\n')
    text.append('static const SubsetPart  s_kParts[] =\n{\n')
    text.append(''.join('    &%s,\n' % p for p in parts))
    text.append('};\n\n\n\n\n\n')
    text.append(BANNER % 'FindHarteSubset')
    text.append('std::span<const Byte> FindHarteSubset (const char * cpuType, Byte opcode)\n{\n')
    text.append('    for (auto part : s_kParts)\n    {\n')
    text.append('        for (const HarteSubsetEntry & entry : part())\n        {\n')
    text.append('            if (entry.opcode == opcode && strcmp (entry.cpu, cpuType) == 0)\n            {\n')
    text.append('                return std::span<const Byte> (entry.bytes, entry.size);\n            }\n        }\n    }\n\n')
    text.append('    return std::span<const Byte>();\n}\n')

    write(os.path.join(out_dir, 'FindHarteSubset.cpp'), ''.join(text))

    print('UnitTest/HarteVectorSubset/: %d opcode files, %d vectors each, %d parts'
          % (total, args.vectors, len(parts)))

    return 0


def write(path, text):
    with open(path, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(text)


PARTS_PER_CPU = 8

HEADER_TEXT = ('#include "Pch.h"\n\n'
               '#include "../HarteTestRunner.h"\n\n\n\n\n'
               '//  GENERATED by scripts/EmbedHarteVectors.py from UnitTest/HarteVectors/,\n'
               '//  %d vectors per opcode. Rerun the script rather than editing this file.\n\n\n\n\n\n')

BANNER = ('////////////////////////////////////////////////////////////////////////////////\n'
          '//\n'
          '//  %s\n'
          '//\n'
          '////////////////////////////////////////////////////////////////////////////////\n\n')

if __name__ == '__main__':
    sys.exit(main())

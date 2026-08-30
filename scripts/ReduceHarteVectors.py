#!/usr/bin/env python3
"""
ReduceHarteVectors.py — Produce the checked-in reduced Harte vector set from
an existing full-depth set, without touching the network.

GenerateHarteTests.py downloads ~4.2 MB of JSON per opcode (about 1.7 GB for
the whole instruction set) and packs it down to the .bin format. That cost is
paid to obtain the vectors, not to choose how many of them to keep -- so once
a full set exists on disk, a smaller set can be cut straight out of it. This
script does that: parse, keep the first N vectors, rewrite the header.

Taking the first N is a fair sample. The vectors arrive in random order, not
sorted: across a full 10,000-vector file the initial PC is non-monotonic, and
the first 200 alone cover 60 of the 64 reachable status-flag combinations and
135 of 256 accumulator values. Truncation is therefore an unbiased subsample,
and a deterministic one -- the same input always yields the same output.

Usage:
    ReduceHarteVectors.py --src <dir> --out <dir> --cpu 6502 [--vectors 200]
"""

import argparse
import datetime
import json
import os
import struct
import sys


HEADER = struct.Struct('<HBB')          # vector_count, opcode, format_version
STATE  = struct.Struct('<HBBBBBB')      # pc, s, a, x, y, p, ram_count
RAM    = struct.Struct('<HB')           # address, value

# Must match FORMAT_VERSION in GenerateHarteTests.py. This script walks the
# per-vector layout to find the truncation point, so it can only cut a set whose
# layout it knows; a version it does not recognize is an error, not a warning.
FORMAT_VERSION = 2


class TruncationError(Exception):
    pass


class FormatVersionError(Exception):
    pass


def skip_state(data, offset):
    """Advance past one CPU state. Returns the new offset."""
    if offset + STATE.size > len(data):
        raise TruncationError('state header runs past end of file')

    ram_count = STATE.unpack_from(data, offset)[6]
    offset += STATE.size
    offset += ram_count * RAM.size

    if offset > len(data):
        raise TruncationError('ram entries run past end of file')

    return offset


def skip_vector(data, offset):
    """Advance past one test vector. Returns the new offset."""
    if offset >= len(data):
        raise TruncationError('vector runs past end of file')

    name_length = data[offset]
    offset += 1 + name_length
    offset += 1                             # cycle count
    offset = skip_state(data, offset)       # initial
    offset = skip_state(data, offset)       # final

    return offset


def reduce_file(src_path, out_path, keep):
    """Write the first `keep` vectors of src_path to out_path.

    Returns (original_count, written_count). A file that already holds fewer
    than `keep` vectors is copied whole rather than padded.
    """
    with open(src_path, 'rb') as f:
        data = f.read()

    if len(data) < HEADER.size:
        raise TruncationError('file is too short to hold a header')

    count, opcode, version = HEADER.unpack_from(data, 0)

    if version != FORMAT_VERSION:
        raise FormatVersionError(
            '%s is format version %d, expected %d -- regenerate it with '
            'GenerateHarteTests.py' % (src_path, version, FORMAT_VERSION))

    written = min(count, keep)

    offset = HEADER.size
    for _ in range(written):
        offset = skip_vector(data, offset)

    with open(out_path, 'wb') as f:
        f.write(HEADER.pack(written, opcode, FORMAT_VERSION))
        f.write(data[HEADER.size:offset])

    return count, written


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--src', required=True,
                        help='directory holding the full-depth .bin files')
    parser.add_argument('--out', required=True,
                        help='directory to write the reduced .bin files to')
    parser.add_argument('--cpu', required=True,
                        help='CPU folder name, recorded in the manifest (e.g. 6502)')
    parser.add_argument('--vectors', type=int, default=200,
                        help='vectors to keep per opcode (default: 200)')
    parser.add_argument('--upstream-ref', default='main',
                        help='upstream ref the source set was generated from')
    args = parser.parse_args()

    if not os.path.isdir(args.src):
        print('error: --src is not a directory: %s' % args.src, file=sys.stderr)
        return 1

    names = sorted(n for n in os.listdir(args.src) if n.endswith('.bin'))
    if not names:
        print('error: no .bin files in %s' % args.src, file=sys.stderr)
        return 1

    os.makedirs(args.out, exist_ok=True)

    src_bytes = 0
    out_bytes = 0
    depths = set()
    opcodes = []

    for name in names:
        src_path = os.path.join(args.src, name)
        out_path = os.path.join(args.out, name)

        original, written = reduce_file(src_path, out_path, args.vectors)

        depths.add(written)
        opcodes.append(name[:-4])
        src_bytes += os.path.getsize(src_path)
        out_bytes += os.path.getsize(out_path)

    manifest = {
        'cpu': args.cpu,
        'formatVersion': FORMAT_VERSION,
        'vectorsPerOpcode': args.vectors,
        'actualDepths': sorted(depths),
        'opcodeCount': len(opcodes),
        'source': 'https://github.com/SingleStepTests/65x02',
        'upstreamRef': args.upstream_ref,
        'reducedFrom': 'full-depth set produced by scripts/GenerateHarteTests.py',
        'generated': datetime.date.today().isoformat(),
        'note': ('Reduced set, checked in so every clone and CI run has real '
                 'opcode coverage without a 1.7 GB download. See docs/testing.md '
                 'for when the full-depth set is warranted.'),
    }

    with open(os.path.join(args.out, 'manifest.json'), 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)
        f.write('\n')

    print('%s: %d opcodes, %d vectors each' % (args.cpu, len(opcodes), args.vectors))
    print('  %.1f MB -> %.1f MB (%.1f%%)'
          % (src_bytes / 1048576.0, out_bytes / 1048576.0,
             out_bytes / src_bytes * 100 if src_bytes else 0))

    return 0


if __name__ == '__main__':
    sys.exit(main())

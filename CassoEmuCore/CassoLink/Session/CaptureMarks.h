#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureMarks
//
//  The marks of a raw read, as the capture module sends them after the
//  values: whether each byte was stored with its top bit clear, as a byte
//  followed by zero bits is. From module version kRunsVersion, a byte ahead
//  of the marks indicates their encoding: kRuns or kBits.
//
//  kBits is a bit for each byte, eight to a byte of the encoding, the first
//  byte's in the top bit, and 0 for a marked byte. kRuns is a byte for each
//  run of bytes of one kind, the first run of unmarked bytes: 0 to 254 for a
//  run of that many, with the next run of the other kind, and 255 for a run
//  of 255, with the next run of the same kind. Bytes past the read's last are
//  ignored.
//
////////////////////////////////////////////////////////////////////////////////

class CaptureMarks
{
public:
    static constexpr Byte  kRunsVersion = 13;
    static constexpr Byte  kRuns        = 0;
    static constexpr Byte  kBits        = 1;

    //  The marks of `stored`, each byte as the read loop stores it.
    static void  EncodeBits (std::span<const Byte> stored, std::vector<Byte> & ioOut);
    static void  EncodeRuns (std::span<const Byte> stored, std::vector<Byte> & ioOut);

    //  The marks at `ioAt` applied to `ioBytes`, a marked byte's top bit
    //  cleared, and `ioAt` moved past them. A module before kRunsVersion sends
    //  kBits with no byte ahead of them. False when the marks are short or
    //  their encoding is unknown.
    static bool  Apply (std::span<const Byte> encoded, Byte moduleVersion, size_t & ioAt, std::vector<Byte> & ioBytes);

private:
    static bool  ApplyBits (std::span<const Byte> encoded, size_t & ioAt, std::vector<Byte> & ioBytes);
    static bool  ApplyRuns (std::span<const Byte> encoded, size_t & ioAt, std::vector<Byte> & ioBytes);

    static constexpr Byte    s_kTopBit       = 0x80;
    static constexpr size_t  s_kMarksPerByte = 8;
    static constexpr Byte    s_kLongRun      = 255;
};

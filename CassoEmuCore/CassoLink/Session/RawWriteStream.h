#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream
//
//  A track as the raw write module writes it (RawWrite.s): a list of bytes,
//  each a value stored in the drive's data register and the count of zero
//  bits written after it. The controller shifts the register out a cell
//  every 4 cycles and shifts in zeros, so a byte stored k cells after the
//  one before is followed by k - 8 zero bits: a 10-bit sync byte is $FF
//  with 2 zeros, and a long zero run, as a weak-bit area has, is $00 bytes
//  with many.
//
//  A WOZ track's bits are split into bytes of eight bits each, and the zero
//  bits after each are its zeros, up to kMaxZeros. A NIB or NB2 track's
//  nibbles are written as they are, and $FF in a run of kSyncRun or more
//  is written as a 10-bit sync byte.
//
////////////////////////////////////////////////////////////////////////////////

class RawWriteStream
{
public:
    static constexpr size_t  kMaxZeros     = 120;
    static constexpr size_t  kSyncZeros    = 2;
    static constexpr size_t  kSyncRun      = 5;
    static constexpr size_t  kBitsPerByte  = 8;

    //  The most bytes a track can have: the module's buffers are 26 pages.
    static constexpr size_t  kMaxBytes     = 26 * 256;

    std::vector<Byte>  values;
    std::vector<Byte>  zeros;

    //  `bits`, one to a byte, most significant first, as one revolution.
    static void    FromBits    (std::span<const uint8_t> bits, RawWriteStream & outStream);

    //  A NIB or NB2 track. A track that repeats, as a NIB track read for
    //  longer than a revolution does, is cut to one revolution first.
    static void    FromNibbles (std::span<const Byte> nibbles, RawWriteStream & outStream);

    //  The length of the first revolution of a nibble track: the first
    //  length from s_kMinRevolution at which s_kMatchBytes nibbles repeat.
    //  The track's whole length when none repeats.
    static size_t  FindRevolution (std::span<const Byte> nibbles);

    //  The bit cells the stream writes.
    size_t  GetCellCount() const;

    //  The bits the stream writes, one to a byte, for a check of FromBits.
    void    ToBits (std::vector<uint8_t> & outBits) const;

private:
    static constexpr size_t  s_kMinRevolution = 5600;
    static constexpr size_t  s_kMatchBytes    = 64;
    static constexpr Byte    s_kSyncValue     = 0xFF;
};

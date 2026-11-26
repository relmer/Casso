#pragma once

#include "Pch.h"

struct RawWriteTrack;





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
//  bits after each are its zeros, up to kMaxZeros. The split starts at the
//  track's seam (FindSeam), so the end of its bits, padded to a whole byte,
//  falls in sync bytes or junk rather than in a sector. A NIB or NB2 track's
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
    static constexpr size_t  kModuleZeros  = 7;

    //  The quarter-track positions the module writes, 0 to 139.
    static constexpr int     kWritablePositions = 140;

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

    //  Whether a file is a WOZ, NIB or NB2 image, whose tracks are written
    //  as they are rather than as sectors.
    static bool     IsRawImage    (std::span<const Byte> fileBytes);

    //  A raw image's tracks to write, in order of position: each whole track
    //  with data, and each half or quarter position whose data differs from
    //  the whole tracks on either side. ERROR_INVALID_DATA for a file that
    //  doesn't load, with outTooLong the first position of a track longer
    //  than the module's buffers, or -1.
    static HRESULT  GetImageTracks (std::span<const Byte> fileBytes, std::vector<RawWriteTrack> & outTracks, int & outTooLong);

    //  The stream rotated to start at its seam (FindSeam), the middle of a
    //  sync run, with sync bytes added there, or dropped from the run, so
    //  that it is a little longer than `cells`, one turn of the drive: the
    //  end of the write then overlaps its start at the seam. False when the
    //  excess can't be dropped.
    bool    FitToTurn (size_t cells);

    //  The bits the stream writes, one to a byte, for a check of FromBits.
    void    ToBits (std::vector<uint8_t> & outBits) const;

    //  The stream as the raw write module takes it: kMaxBytes values, then
    //  kMaxBytes counts of zeros, each 0 to kModuleZeros. A byte with more
    //  zeros is written as the byte with the remainder of eight, then a $00
    //  byte with no zeros for each eight. False when that is more than
    //  kMaxBytes bytes.
    bool    BuildModuleStream (std::vector<Byte> & outStream, size_t & outCount) const;

private:
    static constexpr size_t  s_kMinRevolution = 5600;
    static constexpr size_t  s_kMatchBytes    = 64;
    static constexpr Byte    s_kSyncValue     = 0xFF;
    static constexpr size_t  s_kMinSyncRun    = 16;
    static constexpr size_t  s_kOverlapCells  = 40;
    static constexpr Byte    s_kPrologFirst   = 0xD5;
    static constexpr Byte    s_kPrologSecond  = 0xAA;
    static constexpr size_t  s_kGroupBytes    = 200;      // the stretch a group of prologs is counted over
    static constexpr size_t  s_kSeamLead      = 10;       // the bytes between the seam and the group's first prolog

    //  `bits` split into bytes of eight bits each and the zero bits after,
    //  with outStarts each byte's first bit.
    static void    Split         (std::span<const uint8_t> bits, RawWriteStream & outStream, std::vector<size_t> & outStarts);

    //  The first bit of a byte as a drive frames the track, reading around it
    //  once first, since the track's first bit can be inside a byte.
    static size_t  FindByteStart (std::span<const uint8_t> bits);

    //  The longest run of sync bytes, which may wrap past the end: its first
    //  byte and length. False for none.
    bool  FindSyncRun (size_t & outAt, size_t & outLength) const;

    //  Where the write's seam goes: the longest run of sync bytes when it has
    //  kSyncRun or more, with outSync set and outLength its length. A track
    //  packed too tightly for one, as Karateka's are, has the junk its master
    //  left in front of its densest group of D5 AA AA prologs, so the seam
    //  goes s_kSeamLead bytes before that group's first prolog, with
    //  outLength 0.
    //  False for an empty stream.
    bool  FindSeam (size_t & outAt, size_t & outLength, bool & outSync) const;

    bool  IsSyncByte (size_t at) const { return values[at] == s_kSyncValue && zeros[at] >= 1 && zeros[at] <= kSyncZeros; }
};





//  A track of a raw image to write, at its quarter-track position.
struct RawWriteTrack
{
    int             position = 0;
    RawWriteStream  stream;
};

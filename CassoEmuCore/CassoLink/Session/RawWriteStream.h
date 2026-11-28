#pragma once

#include "Pch.h"

struct RawWriteTrack;



//  The changes FitToTurn makes to a stream, for the log.
struct RawWriteFit
{
    size_t  cells      = 0;        // the stream's cells before
    size_t  written    = 0;        // and after
    size_t  turn       = 0;        // the turn's cells
    size_t  longest    = 0;        // the most cells the write could have and fit
    size_t  addedSyncs = 0;        // sync bytes added ahead of the seam
    size_t  tailSyncs  = 0;        // sync bytes dropped from the run at the end
    size_t  weakCells  = 0;        // zero bits dropped from weak-bit runs
    size_t  gapSyncs   = 0;        // sync bytes dropped from runs between sectors
    bool    fits       = false;
};





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
    //  the whole tracks on either side and isn't them read off center
    //  (DropOffCenterReads), with outSkipped the positions left out.
    //  ERROR_INVALID_DATA for a file that doesn't load, with outTooLong the
    //  first position of a track longer than the module's buffers, or -1.
    static HRESULT  GetImageTracks (std::span<const Byte> fileBytes, std::vector<RawWriteTrack> & outTracks, std::vector<int> & outSkipped, int & outTooLong);

    //  Whether a quarter-track position's stream is only the whole tracks on
    //  either side read off center, as a capture stores it: each D5 AA in it
    //  begins s_kSectorBytes bytes found in one of them. A half track with a
    //  sector of its own, as copy protection reads, isn't. Writing an
    //  off-center read would overwrite the track beside it. Either neighbor
    //  can be null.
    static bool     IsOffCenterRead (const RawWriteStream & quarter, const RawWriteStream * below, const RawWriteStream * above);

    //  Drops each quarter position that is IsOffCenterRead. A whole track
    //  beside a half track that is kept, with no sector of its own and at
    //  least s_kBleedShare of it in that half track, becomes an erase of sync
    //  bytes moved ahead of the half track. outSkipped has the positions
    //  dropped, other than those erased.
    static void     DropOffCenterReads (std::vector<RawWriteTrack> & ioTracks, std::vector<int> & outSkipped);

    //  A track for the activity log: its bits, the most that fit the drive's
    //  turn, and the sync bytes and weak bits added or dropped to fit. The
    //  positions not written get one line, or none when there are none.
    static std::string  DescribeTrack   (const RawWriteTrack & track, const RawWriteFit & fit);
    static std::string  DescribeSkipped (const std::vector<int> & skipped);

    //  The stream rotated to start at its seam (FindSeam), the middle of a
    //  sync run, with at least s_kSeamSyncs sync bytes ahead of the seam, so
    //  that the write, with the cells the module writes around it, is longer
    //  than `cells`, one turn of the drive: its end then overlaps its start,
    //  on those sync bytes only. For a stream too long for that, sync bytes
    //  are dropped from the run at its end, then bits from its longest runs
    //  of zero bits, which are weak-bit stretches, to no fewer than
    //  s_kWeakZeros, then sync bytes from runs between sectors. False when
    //  the excess can't be dropped. outFit has what was added and dropped.
    bool    FitToTurn (size_t cells, RawWriteFit & outFit);

    //  The bits the stream writes, one to a byte, for a check of FromBits.
    void    ToBits (std::vector<uint8_t> & outBits) const;

    //  The bits the module writes, as BuildModuleStream has them: a weak-bit
    //  run has its s_kWeakFlux bytes. Empty when the stream doesn't fit.
    void    ToWrittenBits (std::vector<uint8_t> & outBits) const;

    //  The stream as the raw write module takes it: kMaxBytes values, then
    //  kMaxBytes counts of zeros, each 0 to kModuleZeros. A byte with more
    //  zeros is written as the byte with the remainder of eight, then a $00
    //  byte with no zeros for each eight, or for a weak-bit run of
    //  s_kWeakZeros or more, an s_kWeakFlux byte. False when that is more
    //  than kMaxBytes bytes.
    bool    BuildModuleStream (std::vector<Byte> & outStream, size_t & outCount) const;

private:
    static constexpr size_t  s_kMinRevolution = 5600;
    static constexpr size_t  s_kMatchBytes    = 64;
    static constexpr Byte    s_kSyncValue     = 0xFF;
    static constexpr size_t  s_kOverlapCells  = 40;
    static constexpr Byte    s_kPrologFirst   = 0xD5;
    static constexpr Byte    s_kPrologSecond  = 0xAA;
    static constexpr size_t  s_kGroupBytes    = 200;      // the stretch a group of prologs is counted over
    static constexpr size_t  s_kWeakZeros     = 16;       // the fewest zero bits FitToTurn leaves in a weak-bit run it shortens, and the shortest run written as weak
    static constexpr Byte    s_kWeakFlux      = 0x80;     // a weak-bit run's filler byte: one flux change, then seven cells with none
    static constexpr size_t  s_kSectorBytes   = 16;       // the bytes from a D5 AA that IsOffCenterRead looks for in the tracks beside
    static constexpr size_t  s_kChunkBytes    = 12;       // the chunk GetSharedPart looks for in the tracks beside
    static constexpr double  s_kBleedShare    = 0.75;     // the part of a whole track found in the half track beside that makes it that half track's edge
    static constexpr Byte    s_kEraseEnd      = 0x96;     // the byte that ends each of an erase's sync runs
    static constexpr size_t  s_kEraseRun      = 16;       // an erase's sync run and the byte that ends it
    static constexpr size_t  s_kErasePercent  = 97;       // an erase's length, as a percentage of the track it replaces

    //  The cells the module writes around the stream: ahead of it, the
    //  self-sync byte write mode starts with (rplay in ThirteenSector.s);
    //  after it, the program's two self-sync bytes and the zero bits the drive
    //  writes until write mode is turned off (pdone in Program.s).
    static constexpr size_t  s_kLeadInCells   = kBitsPerByte + kSyncZeros;
    static constexpr size_t  s_kLeadOutCells  = 38;

    //  The fewest sync bytes ahead of the seam: enough to take the most the
    //  end of the write can overlap the stream, and s_kResyncSyncs more for a
    //  drive to frame bytes again after the zero bits it ends with.
    static constexpr size_t  s_kResyncSyncs   = 4;
    static constexpr size_t  s_kSeamSyncs     = (2 * s_kOverlapCells - s_kLeadInCells + kBitsPerByte + kSyncZeros - 1) / (kBitsPerByte + kSyncZeros) + s_kResyncSyncs;

    //  The fewest sync bytes at the end of the stream after FitToTurn, ahead
    //  of the self-sync bytes the module writes after it.
    static constexpr size_t  s_kTailSyncs     = 4;

    //  The fewest sync bytes in a run between sectors after FitToTurn.
    static constexpr size_t  s_kGapSyncs      = 6;

    //  The most cells a stream can have, for a turn of `cells`, when its sync
    //  bytes ahead of the seam are `headCells`.
    static size_t  GetLongestFit (size_t cells, size_t headCells);

    //  Drops one sync byte from the longest run of more than s_kGapSyncs in
    //  the bytes from `first` up to `end`, and its cells from ioTotal. False
    //  when no run is that long.
    bool  DropGapSync (size_t first, size_t end, size_t & ioTotal);

    //  `bits` split into bytes of eight bits each and the zero bits after,
    //  with outStarts each byte's first bit.
    static void    Split         (std::span<const uint8_t> bits, RawWriteStream & outStream, std::vector<size_t> & outStarts);

    //  The first bit of a byte as a drive frames the track, reading around it
    //  once first, since the track's first bit can be inside a byte.
    static size_t  FindByteStart (std::span<const uint8_t> bits);

    //  The longest run of sync bytes, which may wrap past the end: its first
    //  byte and length. False for none.
    bool  FindSyncRun (size_t & outAt, size_t & outLength) const;

    //  A quarter-track position as a track number: 3, 2.25 or 2.5.
    static std::string  FormatPosition (int position);

    //  The part of a stream's chunks found in either neighbor, 0 to 1.
    static double  GetSharedPart      (const RawWriteStream & stream, const RawWriteStream * below, const RawWriteStream * above);

    //  Where the write's seam goes: the longest run of sync bytes when it has
    //  kSyncRun or more, with outSync set and outLength its length. A track
    //  packed too tightly for one, as Karateka's are, can have sector data
    //  right up to a prolog, so the seam goes at the first prolog of its
    //  densest group of D5 AA AA prologs, a sector boundary, with outLength 0.
    //  False for an empty stream.
    bool  FindSeam (size_t & outAt, size_t & outLength, bool & outSync) const;

    bool  IsSyncByte (size_t at) const { return values[at] == s_kSyncValue && zeros[at] >= 1 && zeros[at] <= kSyncZeros; }
};





//  A track of a raw image to write, at its quarter-track position.
struct RawWriteTrack
{
    int             position = 0;
    RawWriteStream  stream;
    bool            isErase  = false;    // sync bytes in place of a whole track beside a half track (DropOffCenterReads)
};

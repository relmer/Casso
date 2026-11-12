#pragma once

#include "Pch.h"

class CassoLinkHost;
class DiskImage;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture
//
//  A raw capture of a 5.25" disk over the link, as a WOZ 2 image, by the
//  capture module. Each whole track is read kWholeReads times. Each other
//  quarter-track position is read once: when its revolution is the whole
//  track's below or above it, its TMAP entry is that track's stream; when it has a
//  separate stream, as a half track of a protected disk has, it is read
//  kWholeReads times too; and when it doesn't repeat, it has no stream.
//
//  The reads of a stream are aligned by rotation, and each byte that isn't
//  the same in all of them is written as eight zero bits, which a WOZ reader
//  reads as random bits. A stream whose reads differ in length is written as
//  the first read is and listed as unstable.
//
//  A whole track's reads mark each byte followed by zero bits at 8 cycles,
//  12 and 8 again (Capture.s). A mark at 8 is on a byte followed by no zero
//  bit about one time in a hundred, by one about four times in seven, and
//  by two or more always; at 12, on a byte followed by two about four times
//  in seven, and by three always. Sync bytes come in runs of FF; a byte with
//  kSureSamples marks at 8 cycles is counted from them, and with fewer, a
//  run's marks are pooled, as a run of sync bytes has one count, and a byte
//  outside a run has zero bits after it only when nearly every one of its
//  marks is set. When an FF byte has some of its marks set and too few to
//  count from, as a 13-sector disk's 9-bit sync bytes have, the track is
//  read up to kExtraReads more times at 8 cycles.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkCapture
{
public:
    static constexpr int     kWholeReads       = 3;
    static constexpr int     kExtraReads       = 4;
    static constexpr int     kSureSamples      = 8;
    static constexpr int     kTracks           = 35;
    static constexpr int     kLastPosition     = (kTracks - 1) * 4;
    static constexpr size_t  kAlignBytes       = 64;
    static constexpr size_t  kSameShare        = 100;

    //  The reads of a quarter-track position.
    struct Position
    {
        int                             position = 0;
        std::vector<std::vector<Byte>>  reads;
        std::vector<Byte>               delays;       // each read's: 0 for 8 cycles, 1 for 12
    };

    //  The delays of a whole track's reads.
    static constexpr Byte    kWholeDelays[kWholeReads] = { 0, 1, 0 };

    struct Result
    {
        std::vector<Byte>  woz;
        std::vector<int>   unstable;     // positions whose reads differ in length
        std::vector<int>   weak;         // positions with bytes that differ between reads
        std::vector<int>   streams;      // positions with a stream, in order
    };

    using ProgressFn = std::function<void (int done, int total)>;

    //  Every position from 0 to the last whole track's, `lastTrack` for a
    //  shorter capture.
    static HRESULT  Capture  (CassoLinkHost & host, Byte drive, std::span<const Byte> module, const std::string & imageDate,
                              const ProgressFn & progress, Result & outResult, int lastTrack = kTracks - 1);

    static HRESULT  BuildWoz (const std::vector<Position> & positions, const std::string & imageDate, Byte drive, Result & outResult);

private:
    struct Stream
    {
        int                   position = 0;
        std::vector<Byte>     bytes;          // one revolution, as the first read has it
        std::vector<bool>     weak;
        std::vector<uint8_t>  zeros;          // the zero bits after each byte
        std::vector<uint8_t>  bits;
    };

    //  The marks of each byte, at each delay: the count set and the count read.
    struct Marks
    {
        std::vector<int>  set[2];
        std::vector<int>  read[2];
    };

    static HRESULT ReadPosition   (CassoLinkHost & host, Byte drive, std::span<const Byte> module, int position, int reads, Position & outPosition);
    static HRESULT ReadMore       (CassoLinkHost & host, Byte drive, std::span<const Byte> module, Position & ioPosition);
    static bool    CollectMarks   (const Position & position, Stream & outStream, Marks & outMarks, bool & outUnstable);
    static bool    IsUncertain    (const Stream & stream, const Marks & marks);
    static void    CountZeros     (const std::vector<Byte> & bytes, const Marks & marks, std::vector<uint8_t> & outZeros);
    static double  GetMarkScore   (int zeros, const Marks & marks, size_t at);
    static bool    MakeStream     (const Position & position, Stream & outStream, bool & outUnstable);
    static int     FindWholeTrack (const std::map<int, Stream> & streams, int position);
    static bool    FindRotation   (std::span<const Byte> revolution, std::span<const Byte> read, size_t & outOffset);
    static bool    IsSameStream   (std::span<const Byte> a, std::span<const Byte> b);
    static void    ToBits         (const Stream & stream, std::vector<uint8_t> & outBits);

    static constexpr Byte    s_kSyncByte = 0xFF;

    //  The chance of a mark at 8 and at 12 cycles on a byte with 0 to 3 zero
    //  bits after it, as the capture tests measure them.
    static constexpr double  s_kMarkChance[2][4] = { { 0.01, 0.45, 0.97, 0.99 }, { 0.005, 0.01, 0.45, 0.97 } };

    //  The counts before any mark: an FF byte is a sync byte as often as
    //  not, and any other byte almost never; and a change between neighbors.
    static constexpr double  s_kRunPrior[4]  = { 0.3, 0.3, 0.39, 0.01 };
    static constexpr double  s_kLonePrior[4] = { 1.0 - 3e-7, 1e-7, 1e-7, 1e-7 };
    static constexpr double  s_kChange       = 0.002;
};

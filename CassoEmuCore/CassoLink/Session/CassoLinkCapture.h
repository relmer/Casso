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
////////////////////////////////////////////////////////////////////////////////

class CassoLinkCapture
{
public:
    static constexpr int     kWholeReads       = 3;
    static constexpr int     kTracks           = 35;
    static constexpr int     kLastPosition     = (kTracks - 1) * 4;
    static constexpr size_t  kAlignBytes       = 64;
    static constexpr size_t  kSameShare        = 100;

    //  The reads of a quarter-track position.
    struct Position
    {
        int                             position = 0;
        std::vector<std::vector<Byte>>  reads;
    };

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
        std::vector<bool>     sync;
        std::vector<uint8_t>  bits;
    };

    static HRESULT ReadPosition   (CassoLinkHost & host, Byte drive, std::span<const Byte> module, int position, int reads, Position & outPosition);
    static bool    MakeStream     (const Position & position, Stream & outStream, bool & outUnstable);
    static int     FindWholeTrack (const std::map<int, Stream> & streams, int position);
    static bool    FindRotation   (std::span<const Byte> revolution, std::span<const Byte> read, size_t & outOffset);
    static bool    IsSameStream   (std::span<const Byte> a, std::span<const Byte> b);
    static void    ToBits         (const Stream & stream, std::vector<uint8_t> & outBits);
};

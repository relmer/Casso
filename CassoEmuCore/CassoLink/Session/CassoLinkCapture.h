#pragma once

#include "Pch.h"

class CassoLinkHost;
class DiskImage;
struct CaptureTiming;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture
//
//  A raw capture of a 5.25" disk over the link, as a WOZ 2 image, by the
//  capture module. Each whole track is read kWholeReads times. The other
//  quarter-track positions between two whole tracks are read only when one
//  of the two isn't a standard track (IsStandardTrack): a head between two
//  standard tracks reads one of them, and data written between them would
//  have damaged both. A position not read has the TMAP entry a standard disk
//  has: a quarter track its nearer whole track's stream, a half track none.
//
//  A position that is read is read once: when its revolution is the whole
//  track's below or above it, its TMAP entry is that track's stream; when it
//  has a separate stream, as a half track of a protected disk has, it is
//  read kWholeReads times too; and when it doesn't repeat, it has no stream.
//
//  A stream is the first read's revolution, and every other revolution of
//  every read is aligned to it with bytes more or fewer allowed, as a write
//  splice's partial bytes are framed differently on each pass. A byte that
//  two alignments put in a longer stretch of differences is written as
//  eight zero bits, which a WOZ reader reads as random bits. A stream with
//  a read none of whose revolutions aligns is written as the first read is
//  and listed as unstable.
//
//  A whole track's reads mark each byte followed by zero bits at 8 cycles,
//  12 and 8 again (Capture.s). How often a mark is set depends on the
//  controller: a Disk II sets one at 8 cycles on a byte with no zero bits
//  after it about one time in a hundred, and a //c's IWM about one time in
//  four. So the track's bytes that aren't FF set the baseline, and each run
//  of FF, which a run of sync bytes is, is counted from its marks pooled
//  against it (CountZeros).
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkCapture
{
public:
    static constexpr int     kWholeReads       = 3;
    static constexpr int     kTracks           = 35;
    static constexpr int     kLastPosition     = (kTracks - 1) * 4;
    static constexpr size_t  kAlignBytes       = 64;
    static constexpr size_t  kSameShare        = 33;

    //  The reads of a quarter-track position.
    //  The reads of a quarter-track position. With syncFrom a position, not
    //  -1, the first read started s_kSyncCycles after syncMarker passed there;
    //  with syncFrom the position itself, the second read started
    //  s_kSelfSyncCycles after syncMarker, from the first read, passed.
    struct Position
    {
        int                             position       = 0;
        std::vector<std::vector<Byte>>  reads;
        std::vector<Byte>               delays;   // each read's: 0 for 8 cycles, 1 for 12
        int                             syncFrom       = -1;
        std::vector<Byte>               syncMarker;
        bool                            writeProtected = false;   // a read found the disk write-protected
    };

    //  The delays of a whole track's reads.
    static constexpr Byte    kWholeDelays[kWholeReads] = { 0, 1, 0 };

    struct Result
    {
        std::vector<Byte>      woz;
        std::vector<int>       unstable;          // positions with a read that doesn't align
        std::vector<int>       weak;              // positions with bytes that differ between reads
        std::vector<int>       streams;           // positions with a stream, in order
        int                    betweenRead  = 0;  // positions between whole tracks that were read
        int                    betweenTotal = 0;  // and all of them
        int                    reads        = 0;
        double                 diskMs       = 0.0;
        double                 lineMs       = 0.0;
        size_t                 sentBytes    = 0;  // the encoded bytes over the line
        size_t                 storedBytes  = 0;  // and the bytes they decoded to
        std::vector<Position>  positions;         // every read, for SerializeReads
        bool                   synchronized = false;  // every whole track placed relative to track 0

        //  The drive's revolution as TimeRevolution timed it, 0 when it wasn't,
        //  and whether the disk is write-protected, which the WOZ's INFO has.
        double                 revolutionCycles = 0.0;
        bool                   writeProtected   = false;

        //  Each sector taken whole from another read (RepairProlog), as its
        //  position and sector number.
        std::vector<std::pair<int, int>>  replaced;
    };

    //  Where a capture is: the position of its last read, that read's number
    //  and the reads the position has, the pass (1 for the whole tracks, 2
    //  for the positions between them), the share of the reads done, and the
    //  seconds left at the pace so far, -1 before there is a pace.
    struct Progress
    {
        int     position    = 0;
        int     read        = 0;
        int     reads       = 0;
        int     pass        = 1;
        int     percent     = 0;
        double  secondsLeft = -1.0;
    };

    using ProgressFn = std::function<void (const Progress & progress)>;

    //  Every position from 0 to the last whole track's, `lastTrack` for a
    //  shorter capture.
    static HRESULT  Capture  (CassoLinkHost & host, Byte drive, std::span<const Byte> module, const std::string & imageDate,
                              const ProgressFn & progress, Result & outResult, int lastTrack = kTracks - 1);

    static HRESULT  BuildWoz (const std::vector<Position> & positions, int lastPosition, const std::string & imageDate, Byte drive, Result & outResult);

    //  A capture's reads as a file, so a WOZ can be built from them again:
    //  "CRAW", a version, then each position, its reads and each read's delay
    //  and bytes, little-endian. Parsing fails with ERROR_INVALID_DATA for
    //  any other file.
    static void     SerializeReads (const std::vector<Position> & positions, std::vector<Byte> & outFile);
    static HRESULT  ParseReads     (std::span<const Byte> file, std::vector<Position> & outPositions);

    //  What a capture marked, for a manifest row: weak 3, 11; replaced 1.3;
    //  differs 32, with a track as its number and a replaced sector as its
    //  track and sector. Empty with nothing marked.
    static std::string  FormatMarks (const Result & result);

    //  Whether a whole track's reads are a standard 16-sector or 13-sector
    //  track: a stream with every sector's address field for the track, its
    //  checksum right.
    static bool     IsStandardTrack (const Position & position);

private:
    struct Stream
    {
        int                   position = 0;
        size_t                start    = 0;   // the first read's byte the revolution starts at
        std::vector<Byte>     bytes;          // one revolution, as the first read has it
        std::vector<bool>     weak;
        std::vector<uint8_t>  zeros;          // the zero bits after each byte
        std::vector<uint8_t>  bits;
        std::vector<size_t>   starts;         // each byte's first bit in bits
        std::vector<int>      replaced;       // the sectors taken whole from another read
    };

    //  The marks of each byte, at each delay: the count set and the count read.
    struct Marks
    {
        std::vector<int>  set[2];
        std::vector<int>  read[2];
    };

    //  A revolution of a read aligned to a stream (Align).
    struct Alignment
    {
        size_t             read   = 0;
        size_t             from   = 0;
        size_t             differ = 0;
        std::vector<int>   map;
        std::vector<bool>  stretch;
    };

    //  A capture's state between its reads.
    struct Run
    {
        CassoLinkHost          & host;
        Byte                     drive;
        std::span<const Byte>    module;
        const ProgressFn       & progress;
        Result                 & result;
        ULONGLONG                started        = 0;
        int                      done           = 0;
        int                      planned        = 0;
        int                      pass           = 1;
        int                      percent        = 0;
        bool                     writeProtected = false;
    };

    static HRESULT              ReadOnce       (Run & run, int position, Byte delay, int read, int reads, Byte syncPosition, std::span<const Byte> marker, std::vector<Byte> & outBytes, bool & outMissed);
    static HRESULT              ReadPosition   (Run & run, int position, int reads, Position & outPosition);
    static HRESULT              Analyze        (Run & run, const std::function<void()> & work);
    static void                 FindMarker     (const std::vector<Byte> & revolution, std::vector<Byte> & outMarker);
    static void                 PlaceTracks    (const std::vector<Position> & positions, const std::map<int, Stream> & streams, double revolution, std::map<int, size_t> & outRotations, bool & outSynchronized);
    static double               TimeRevolution (const std::vector<Position> & positions, const std::map<int, Stream> & streams);
    static size_t               GetBitOffset   (const Stream & stream, size_t byte);
    static void                 Write32        (std::vector<Byte> & bytes, size_t at, uint32_t value);
    static bool                 CollectMarks   (const Position & position, Stream & outStream, Marks & outMarks, bool & outUnstable);
    static void                 AlignAll       (const Position & position, const std::vector<Byte> & revolution, std::vector<Alignment> & outAlignments, bool & outUnstable);
    static void                 MakeConsensus  (const Position & position, const std::vector<Byte> & revolution, const std::vector<Alignment> & alignments, std::vector<Byte> & outAgreed);
    static bool                 FindReference  (std::span<const Byte> read, size_t & outStart, size_t & outLength);
    static size_t               FindFramed     (std::span<const Byte> read, size_t limit);
    static size_t               FindLength     (std::span<const Byte> read);
    static size_t               FindRepeat     (std::span<const Byte> read);
    static size_t               CountDiffer    (std::span<const Byte> a, size_t aAt, std::span<const Byte> b, size_t bAt, size_t limit);
    static bool                 IsUnique       (std::span<const Byte> read, size_t start, size_t length);
    static std::vector<size_t>  FindAnchors    (std::span<const Byte> revolution, std::span<const Byte> read);
    static bool                 Align          (std::span<const Byte> revolution, std::span<const Byte> read, size_t from,
                                                std::vector<int> & outMap, size_t & outDiffer, std::vector<bool> & outStretch);
    static int                  CountSectors   (std::span<const Byte> revolution, int track, bool & outThirteen, int & outAddressed);
    static bool                 IsCrosstalk    (std::span<const Byte> revolution, int position);
    static void                 CountZeros     (const std::vector<Byte> & bytes, const Marks & marks, std::vector<uint8_t> & outZeros);
    static bool                 IsAboveBase    (double set, double read, double base);
    static bool                 MakeStream     (const Position & position, Stream & outStream, bool & outUnstable);
    static int                  FindWholeTrack (const std::map<int, Stream> & streams, int position);
    static bool                 IsSameStream   (std::span<const Byte> a, std::span<const Byte> b);
    static size_t               FindDataField  (std::span<const Byte> bytes, size_t at);
    static bool                 RepairFields   (const Position & position, std::vector<Byte> & ioRevolution);
    static bool                 RepairProlog   (const Position & position, std::vector<Byte> & ioRevolution, std::vector<int> & outSectors);
    static std::string          DescribeResult (const Result & result, Byte drive);
    static bool                 IsAddressAt    (std::span<const Byte> bytes, size_t at);
    static size_t               FindNextField  (std::span<const Byte> bytes, size_t at);
    static void                 UniteSyncZeros (std::map<int, Stream> & ioStreams);
    static bool                 FindGap        (const Stream & stream, size_t & outFirst, size_t & outEnd);
    static void                 FitGap         (Stream & ioStream, double bits);
    static void                 FindGapMarker  (const std::vector<Byte> & revolution, std::vector<Byte> & outMarker);
    static void                 ToBits         (const Stream & stream, std::vector<uint8_t> & outBits, std::vector<size_t> & outStarts);

    static constexpr Byte    s_kSyncByte = 0xFF;
    static constexpr size_t  s_kMaxZeros = 2;

    //  The zero bits a sync byte may have, which a controller reads alike up
    //  to three, and the sync bytes in a row that make a track's gap.
    static constexpr uint8_t  s_kMaxSyncZeros = 3;
    static constexpr size_t   s_kGapSyncs     = 32;
    static constexpr size_t   s_kResyncBytes  = 8;

    //  The range of revolution lengths for FindLength, and its sample; the
    //  step between FindReference's starts, the values a start's bytes
    //  must have, so a run of sync bytes, which repeats at any length, isn't
    //  one, and the bytes of a start or a window that may differ where it
    //  repeats; and the step between FindAnchors' windows, and the drift a
    //  write splice between two places can add.
    static constexpr size_t  s_kShortestRevolution = 4800;
    static constexpr size_t  s_kLongestRevolution  = 7000;
    static constexpr size_t  s_kSampleBytes        = 4096;
    static constexpr size_t  s_kSampleStep         = 3;
    static constexpr size_t  s_kSampleDrift        = 2;
    static constexpr size_t  s_kNoiseShare         = 4;
    static constexpr size_t  s_kStartStep          = 13;    // prime and short, to fall on every part of a sector in turn
    static constexpr size_t  s_kAnchorValues       = 8;
    static constexpr size_t  s_kAnchorMismatch     = 2;
    static constexpr size_t  s_kWindowStep         = 997;
    static constexpr size_t  s_kWindowDrift        = 8;
    static constexpr size_t  s_kFramingSyncs       = 4;     // the sync bytes in a row that frame a read's bytes as on the disk

    //  Align's allowance of bytes more or fewer, the share of a revolution
    //  that may differ, and the longest stretch of differences a write splice
    //  has; and the alignments that must put a byte in a longer one for it to
    //  be weak.
    static constexpr size_t  s_kBand               = 24;
    static constexpr size_t  s_kAlignShare         = 12;
    static constexpr size_t  s_kSpliceBytes        = 16;
    static constexpr int     s_kWeakReads          = 2;
    static constexpr size_t  s_kConsensusGap       = 4;
    static constexpr Byte    s_kStepPair           = 1;
    static constexpr Byte    s_kStepFewer          = 2;
    static constexpr Byte    s_kStepMore           = 3;

    //  CountZeros: the shortest run of FF counted as sync bytes, the share of
    //  marks at 12 cycles for three zero bits, and how far above the baseline
    //  a run's share must be.
    static constexpr size_t  s_kShortestRun        = 2;
    static constexpr double  s_kThreeZeroShare     = 0.8;
    static constexpr double  s_kMinimumGain        = 0.05;
    static constexpr double  s_kDeviations         = 3.0;
    static constexpr double  s_kMinimumReads       = 4.0;

    //  A field's marks: D5 AA, then 96 for a 16-sector address field, B5 for
    //  a 13-sector one and AD for a data field. RDOS begins its address
    //  fields D4 AA B7.
    static constexpr Byte    s_kProlog0       = 0xD5;
    static constexpr Byte    s_kProlog1       = 0xAA;
    static constexpr Byte    s_kAddress16     = 0x96;
    static constexpr Byte    s_kAddress13     = 0xB5;
    static constexpr Byte    s_kData          = 0xAD;
    static constexpr Byte    s_kRdosProlog0   = 0xD4;
    static constexpr Byte    s_kRdosAddress   = 0xB7;
    static constexpr Byte    s_kOtherProlog1  = 0xBB;      // a data field's, as Archon's D5 BB CF
    static constexpr size_t  s_kAddressValues = 8;      // volume, track, sector and checksum, two bytes each

    //  The bytes after an address field's prolog that its data field's prolog
    //  must start within; and the reads a pace is estimated after.
    static constexpr size_t  s_kDataWithin   = 48;
    static constexpr size_t  s_kNextWithin   = 600;     // the bytes after an address field its sector and the gap after it end within
    static constexpr size_t  s_kPrologBytes  = 3;
    static constexpr size_t  s_kNotedSectors = 8;       // the sectors taken from another read the notes list one by one
    static constexpr int     s_kPaceReads    = 3;

    static constexpr Byte    s_kReadsMagic[] = { 'C', 'R', 'A', 'W' };
    static constexpr Word    s_kReadsVersion = 3;

    //  The cycles from a marker's last byte to the first byte of a read that
    //  waited on it (Capture.s), as measured on Casso's //c; the cycles a bit
    //  takes; and the marker: an address field's prolog and its eight bytes.
    static constexpr double  s_kSyncCycles   = 50400.0;
    static constexpr double  s_kBitCycles    = 4.0;
    static constexpr size_t  s_kMarkerBytes  = 11;
    static constexpr int     s_kReadAttempts = 3;
    static constexpr DWORD   s_kBusyPollMs   = 200;     // Analyze's wait for a Ready, between checks that the work is done

    //  The cycles from a marker's last byte to the first byte of a read of
    //  the same track that waited on it, with no step, as measured on Casso's
    //  //c (TimeRevolution), and how far a measured revolution may be from
    //  the track's bits at s_kBitCycles.
    static constexpr double  s_kSelfSyncCycles   = 25680.0;
    static constexpr double  s_kRevolutionSpread = 0.1;

    //  A version 2 WOZ's header, its CRC's place in it, and the INFO chunk's
    //  synchronized flag's place in the file.
    static constexpr size_t  s_kWozHeaderBytes   = 12;
    static constexpr size_t  s_kWozCrcOffset     = 8;
    static constexpr size_t  s_kInfoSynchronized = 23;
    static constexpr size_t  s_kInfoProtected    = 22;
};

#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecord
//
//  One record the scanner found: where its leader starts, where its data
//  starts and ends, in fractional samples at the recording's rate, how many
//  bytes it holds (the checksum byte included), and whether that checksum
//  matches.
//
////////////////////////////////////////////////////////////////////////////////

struct TapeRecord
{
    double  leaderStart = 0.0;
    double  dataStart   = 0.0;
    double  dataEnd     = 0.0;
    size_t  byteCount   = 0;
    bool    isValid     = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordScanner
//
//  Finds the standard Apple II records in a list of level transitions: a
//  leader of about 770 Hz, a short sync cycle, then the bytes most
//  significant bit first -- a 1 bit one cycle of about 1 kHz, a 0 bit one of
//  about 2 kHz -- ending in an XOR checksum seeded with $FF. The Monitor's
//  READ, and BASIC's LOAD through it, write and read every record this way.
//
//  THE ONLY PLACE OUTSIDE THE ROM THAT KNOWS THE FORMAT. The decoder uses it
//  to judge its own work -- a record whose checksum fails is decoded again
//  with other settings -- and never to supply what the guest reads, which is
//  still only the transitions.
//
////////////////////////////////////////////////////////////////////////////////

class TapeRecordScanner
{
public:
    static void Scan (const std::vector<double> & transitions, uint32_t sampleRate, std::vector<TapeRecord> & records);

private:
    static bool IsLeaderHalf (double us) { return us > kLeaderMinUs && us < kLeaderMaxUs; }

    static constexpr double  kLeaderMinUs    = 550.0;    // a leader half-cycle is about 650 us
    static constexpr double  kLeaderMaxUs    = 800.0;
    static constexpr size_t  kLeaderMinRun   = 500;      // about a third of a second of leader
    static constexpr size_t  kLeaderMaxBurst = 3;        // odd half-cycles in a row a leader may hold
    static constexpr double  kOneMinCycleUs  = 750.0;    // a full cycle longer than this is a 1 bit
    static constexpr double  kGapCycleUs     = 1200.0;   // and longer than this, the record has ended
    static constexpr Byte    kChecksumSeed   = 0xFF;
};

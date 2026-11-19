#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteCodec
//
//  Casso's audio encoding for the link over the cassette ports (R45). A bit
//  is one full cycle: 500 microseconds for a 0 and 1,000 for a 1, most
//  significant bit first. A cycle is measured whole, so a comparator whose
//  threshold isn't centered, which makes a cycle's halves unequal, and an
//  inverted cable change no bit.
//
//  A burst is kToneHalves half cycles of kToneHalfUs, then one 0 bit as its
//  mark, then the count of bytes, low byte first, and its complement, then
//  the bytes, then one 0 bit after them, so the last bit ends at an edge
//  that another edge follows.
//
////////////////////////////////////////////////////////////////////////////////

class CassetteCodec
{
public:
    static constexpr double  kZeroHalfUs    = 250.0;
    static constexpr double  kOneHalfUs     = 500.0;
    static constexpr double  kToneHalfUs    = 1000.0;
    static constexpr int     kToneHalves    = 64;
    static constexpr size_t  kHeaderBytes   = 4;
    static constexpr size_t  kMaxBurst      = 0x1200;

    //  The decoder's limits. A tone half is from kToneLowUs to kToneHighUs,
    //  and kToneMinimum of them in a row come before the mark, whose first
    //  half is shorter than kMarkLimitUs. A cycle shorter than kBitLimitUs is
    //  a 0, and one shorter than kCycleLimitUs a 1; a longer one ends the
    //  burst as damaged.
    static constexpr double  kToneLowUs     = 750.0;
    static constexpr double  kToneHighUs    = 1500.0;
    static constexpr int     kToneMinimum   = 16;
    static constexpr double  kMarkLimitUs   = 625.0;
    static constexpr double  kBitLimitUs    = 750.0;
    static constexpr double  kCycleLimitUs  = 1500.0;

    //  The half cycles of a burst of `bytes`, at most kMaxBurst of them,
    //  appended to inOutHalvesUs, with toneHalves half cycles of tone.
    static void  AppendBurst (std::span<const Byte> bytes, std::vector<double> & inOutHalvesUs, int toneHalves = kToneHalves);

    //  Half cycles as a square wave of alternating sign at `sampleRate`, each
    //  sample the wave's mean over its interval, so an edge between two
    //  samples is between them in the samples too.
    static void  Render      (std::span<const double> halvesUs, uint32_t sampleRate, float amplitude, std::vector<float> & outSamples);

    //  The microseconds a burst of `count` bytes lasts.
    static double  GetBurstUs (size_t count);
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteDecoder
//
//  Bursts from audio as it arrives, a block of samples at a time. The
//  samples are filtered of DC and hum, an edge is where the filtered signal
//  crosses zero once it is past a threshold a fraction of its peak, placed
//  between samples, and the half cycles between edges go to PushHalf.
//
////////////////////////////////////////////////////////////////////////////////

class CassetteDecoder
{
public:
    explicit CassetteDecoder (uint32_t sampleRate);

    void   Push      (std::span<const float> samples);
    void   PushHalf  (double halfUs);
    bool   PopBurst  (std::vector<Byte> & outBytes);
    void   Reset     ();

    //  The input's peak level, 0 to 1, for a level meter.
    float  GetLevel  () const { return (float) m_peak; }

    //  Whether a burst is being received now.
    bool   IsInBurst () const { return m_state != State::Hunt; }

private:
    enum class State
    {
        Hunt,
        Mark,
        Bits,
    };

    void  EndBurst  ();
    void  TakeCycle (double cycleUs);

    static constexpr double  s_kHighPassHz    = 150.0;
    static constexpr double  s_kThreshold     = 0.15;
    static constexpr double  s_kFloor         = 0.005;
    static constexpr double  s_kReleaseSecond = 0.25;

    double                         m_sampleRate = 0.0;
    double                         m_highPassR  = 0.0;
    double                         m_release    = 0.0;
    double                         m_lastIn     = 0.0;
    double                         m_filtered   = 0.0;
    double                         m_peak       = 0.0;
    double                         m_crossing   = -1.0;
    double                         m_lastEdge   = -1.0;
    uint64_t                       m_sample     = 0;
    int                            m_sign       = 0;

    State                          m_state      = State::Hunt;
    int                            m_tones      = 0;
    double                         m_firstHalf  = -1.0;
    Byte                           m_byte       = 0;
    int                            m_bits       = 0;
    size_t                         m_count      = 0;
    std::vector<Byte>              m_bytes;
    std::deque<std::vector<Byte>>  m_bursts;
};

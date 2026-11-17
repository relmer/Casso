#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioCodec
//
//  ADTPro's audio encoding, from its audio client's cassette output: the
//  monitor's tape format, measured in cycles of the Apple's 1,020,484 Hz
//  clock. A burst is a tone of 652-cycle half cycles, a sync cycle of a
//  200-cycle half and a 250-cycle half, then the bytes, most significant
//  bit first, a 0 bit as two 250-cycle halves and a 1 bit as two 500-cycle
//  halves. A burst contains one packet in ADTPro's serial envelope.
//
//  After the first edge on its input, the client stops reading the input
//  for about 350 milliseconds and then reads it for the sync, so a burst
//  sent to it has a tone of kLeaderHalves, longer than that. One 0 bit
//  after the bytes ends the last bit at an edge.
//
////////////////////////////////////////////////////////////////////////////////

class AdtProAudioCodec
{
public:
    static constexpr double  kClockHz       = 1020484.0;
    static constexpr double  kUsPerCycle    = 1000000.0 / kClockHz;
    static constexpr double  kLeaderHalfUs  = 652.0 * kUsPerCycle;
    static constexpr double  kSyncFirstUs   = 200.0 * kUsPerCycle;
    static constexpr double  kSyncSecondUs  = 250.0 * kUsPerCycle;
    static constexpr double  kZeroHalfUs    = 250.0 * kUsPerCycle;
    static constexpr double  kOneHalfUs     = 500.0 * kUsPerCycle;
    static constexpr int     kLeaderHalves  = 940;

    //  The decoder's limits. A tone half is from kToneLowUs to kToneHighUs,
    //  which excludes the 500-microsecond halves of a 1 in Casso's encoding,
    //  and kToneMinimum of them in a row come before the sync, whose halves
    //  are each shorter than kSyncLimitUs. A cycle shorter than kBitLimitUs
    //  is a 0, and one shorter than kCycleLimitUs a 1; a longer one ends the
    //  burst.
    static constexpr double  kToneLowUs     = 560.0;
    static constexpr double  kToneHighUs    = 780.0;
    static constexpr int     kToneMinimum   = 64;
    static constexpr double  kSyncLimitUs   = 400.0;
    static constexpr double  kBitLimitUs    = 735.0;
    static constexpr double  kCycleLimitUs  = 1470.0;

    //  The client's bursts are a packet, then one byte more, whose value
    //  varies from burst to burst, then a check byte: $FF
    //  exclusive-ORed with every byte before it, as the monitor's tape
    //  write ends a record.
    static constexpr size_t  kTrailerBytes  = 2;

    static void                   AppendBurst (std::span<const Byte> bytes, std::vector<double> & inOutHalvesUs);
    static double                 GetBurstUs  (size_t count);
    static bool                   HasTrailer  (std::span<const Byte> burst);
    static std::span<const Byte>  GetPacket   (std::span<const Byte> burst);
};





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioDecoder
//
//  Bursts from audio as it arrives, a block of samples at a time. The
//  samples are filtered of DC and hum, an edge is where the filtered signal
//  crosses zero once it is past a threshold a fraction of its peak, placed
//  between samples, and the half cycles between edges go to PushHalf. A
//  burst ends at a cycle too long for a bit, or when no edge comes for two
//  of them; its whole bytes are the burst.
//
////////////////////////////////////////////////////////////////////////////////

class AdtProAudioDecoder
{
public:
    explicit AdtProAudioDecoder (uint32_t sampleRate);

    void   Push      (std::span<const float> samples);
    void   PushHalf  (double halfUs);
    bool   PopBurst  (std::vector<Byte> & outBytes);
    void   Reset     ();

    float  GetLevel  () const { return (float) m_peak; }
    bool   IsInBurst () const { return m_state != State::Hunt; }

private:
    enum class State
    {
        Hunt,
        Sync,
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
    std::vector<Byte>              m_bytes;
    std::deque<std::vector<Byte>>  m_bursts;
};

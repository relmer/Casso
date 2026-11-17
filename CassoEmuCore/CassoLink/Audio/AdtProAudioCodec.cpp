#include "Pch.h"

#include "CassoLink/Audio/AdtProAudioCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioCodec::AppendBurst
//
////////////////////////////////////////////////////////////////////////////////

void AdtProAudioCodec::AppendBurst (std::span<const Byte> bytes, std::vector<double> & inOutHalvesUs)
{
    constexpr int   kBitsPerByte = 8;
    constexpr Byte  kHighBit     = 0x80;



    inOutHalvesUs.insert (inOutHalvesUs.end(), (size_t) kLeaderHalves, kLeaderHalfUs);
    inOutHalvesUs.push_back (kSyncFirstUs);
    inOutHalvesUs.push_back (kSyncSecondUs);

    for (Byte value : bytes)
    {
        for (int bit = 0; bit < kBitsPerByte; bit++)
        {
            double  half = (value & (kHighBit >> bit)) != 0 ? kOneHalfUs : kZeroHalfUs;

            inOutHalvesUs.push_back (half);
            inOutHalvesUs.push_back (half);
        }
    }

    inOutHalvesUs.push_back (kZeroHalfUs);
    inOutHalvesUs.push_back (kZeroHalfUs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioCodec::GetBurstUs
//
//  The longest a burst of `count` bytes can be, every bit a 1.
//
////////////////////////////////////////////////////////////////////////////////

double AdtProAudioCodec::GetBurstUs (size_t count)
{
    constexpr double  kBitsPerByte = 8.0;



    return kLeaderHalves * kLeaderHalfUs + kSyncFirstUs + kSyncSecondUs + 2.0 * kZeroHalfUs + (double) count * kBitsPerByte * 2.0 * kOneHalfUs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioCodec::GetPacket
//
//  The packet in a burst from the client: the burst without its trailer
//  when the check byte matches, or the burst as it came otherwise.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> AdtProAudioCodec::GetPacket (std::span<const Byte> burst)
{
    constexpr Byte  kCheckSeed = 0xFF;
    Byte            check      = kCheckSeed;
    size_t          length     = burst.size();
    bool            trailed    = false;



    for (size_t at = 0; at + 1 < length; at++)
    {
        check ^= burst[at];
    }

    trailed = length > kTrailerBytes && check == burst[length - 1];

    return trailed ? burst.first (length - kTrailerBytes) : burst;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioDecoder::AdtProAudioDecoder
//
////////////////////////////////////////////////////////////////////////////////

AdtProAudioDecoder::AdtProAudioDecoder (uint32_t sampleRate) :
    m_sampleRate (sampleRate),
    m_highPassR  (exp (-2.0 * std::numbers::pi * s_kHighPassHz / (double) sampleRate)),
    m_release    (exp (-1.0 / (s_kReleaseSecond * (double) sampleRate)))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioDecoder::Push
//
//  An edge is placed at the zero crossing before the signal passed the
//  threshold, between the two samples on either side of it.
//
////////////////////////////////////////////////////////////////////////////////

void AdtProAudioDecoder::Push (std::span<const float> samples)
{
    constexpr double  kUsPerSecond = 1000000.0;
    double            gapSamples   = AdtProAudioCodec::kCycleLimitUs * 2.0 * m_sampleRate / kUsPerSecond;



    for (float sample : samples)
    {
        double  in        = (double) sample;
        double  prior     = m_filtered;
        double  filtered  = in - m_lastIn + m_highPassR * m_filtered;
        double  threshold = 0.0;
        double  now       = (double) m_sample;
        int     sign      = 0;

        m_lastIn   = in;
        m_filtered = filtered;
        m_peak     = std::max (fabs (filtered), m_peak * m_release);
        threshold  = std::max (s_kFloor, s_kThreshold * m_peak);

        if ((prior < 0.0) != (filtered < 0.0) && prior != filtered)
        {
            m_crossing = now - 1.0 + prior / (prior - filtered);
        }

        if (filtered > threshold)
        {
            sign = 1;
        }
        else if (filtered < -threshold)
        {
            sign = -1;
        }

        if (sign != 0 && sign != m_sign)
        {
            double  edge = (m_crossing >= 0.0) ? m_crossing : now;

            if (m_sign != 0 && m_lastEdge >= 0.0)
            {
                PushHalf ((edge - m_lastEdge) * kUsPerSecond / m_sampleRate);
            }

            m_sign     = sign;
            m_lastEdge = edge;
        }

        if (m_state != State::Hunt && m_lastEdge >= 0.0 && now - m_lastEdge > gapSamples)
        {
            EndBurst();
        }

        m_sample++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioDecoder::PushHalf
//
////////////////////////////////////////////////////////////////////////////////

void AdtProAudioDecoder::PushHalf (double halfUs)
{
    bool  tone    = halfUs >= AdtProAudioCodec::kToneLowUs && halfUs <= AdtProAudioCodec::kToneHighUs;
    bool  isShort = halfUs < AdtProAudioCodec::kSyncLimitUs;



    switch (m_state)
    {
        case State::Hunt:
            if (tone)
            {
                m_tones++;
            }
            else if (isShort && m_tones >= AdtProAudioCodec::kToneMinimum)
            {
                m_state = State::Sync;
            }
            else
            {
                m_tones = 0;
            }

            break;

        case State::Sync:
            if (isShort)
            {
                m_state     = State::Bits;
                m_firstHalf = -1.0;
                m_byte      = 0;
                m_bits      = 0;
                m_bytes.clear();
            }
            else
            {
                EndBurst();
            }

            break;

        case State::Bits:
            if (m_firstHalf < 0.0)
            {
                m_firstHalf = halfUs;
            }
            else
            {
                double  cycle = m_firstHalf + halfUs;

                m_firstHalf = -1.0;
                TakeCycle (cycle);
            }

            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioDecoder::TakeCycle
//
////////////////////////////////////////////////////////////////////////////////

void AdtProAudioDecoder::TakeCycle (double cycleUs)
{
    constexpr int  kBitsPerByte = 8;
    Byte           bit          = cycleUs >= AdtProAudioCodec::kBitLimitUs ? 1 : 0;



    if (cycleUs >= AdtProAudioCodec::kCycleLimitUs)
    {
        EndBurst();
    }
    else if (++m_bits == kBitsPerByte)
    {
        m_bytes.push_back ((Byte) ((m_byte << 1) | bit));
        m_byte = 0;
        m_bits = 0;
    }
    else
    {
        m_byte = (Byte) ((m_byte << 1) | bit);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioDecoder::EndBurst
//
//  A burst's whole bytes, if any, go to the list PopBurst reads.
//
////////////////////////////////////////////////////////////////////////////////

void AdtProAudioDecoder::EndBurst()
{
    if (m_state == State::Bits && !m_bytes.empty())
    {
        m_bursts.push_back (std::move (m_bytes));
    }

    m_state     = State::Hunt;
    m_tones     = 0;
    m_firstHalf = -1.0;
    m_byte      = 0;
    m_bits      = 0;
    m_bytes.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioDecoder::PopBurst
//
////////////////////////////////////////////////////////////////////////////////

bool AdtProAudioDecoder::PopBurst (std::vector<Byte> & outBytes)
{
    if (m_bursts.empty())
    {
        return false;
    }

    outBytes = std::move (m_bursts.front());
    m_bursts.pop_front();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioDecoder::Reset
//
////////////////////////////////////////////////////////////////////////////////

void AdtProAudioDecoder::Reset()
{
    m_state = State::Hunt;
    EndBurst();
    m_bursts.clear();
    m_lastIn   = 0.0;
    m_filtered = 0.0;
    m_peak     = 0.0;
    m_crossing = -1.0;
    m_lastEdge = -1.0;
    m_sign     = 0;
}

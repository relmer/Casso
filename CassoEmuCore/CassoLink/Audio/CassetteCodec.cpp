#include "Pch.h"

#include "CassoLink/Audio/CassetteCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteCodec::AppendBurst
//
////////////////////////////////////////////////////////////////////////////////

void CassetteCodec::AppendBurst (std::span<const Byte> bytes, std::vector<double> & inOutHalvesUs, int toneHalves)
{
    constexpr int      kBitsPerByte = 8;
    constexpr Byte     kHighBit     = 0x80;
    size_t             count        = std::min (bytes.size(), kMaxBurst);
    std::vector<Byte>  framed       = { (Byte) (count & 0xFF), (Byte) (count >> 8), (Byte) ~(count & 0xFF), (Byte) ~(count >> 8) };



    framed.insert (framed.end(), bytes.begin(), bytes.begin() + (ptrdiff_t) count);

    inOutHalvesUs.insert (inOutHalvesUs.end(), (size_t) std::max (toneHalves, kToneMinimum), kToneHalfUs);
    inOutHalvesUs.push_back (kZeroHalfUs);
    inOutHalvesUs.push_back (kZeroHalfUs);

    for (Byte value : framed)
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
//  CassetteCodec::Render
//
////////////////////////////////////////////////////////////////////////////////

void CassetteCodec::Render (std::span<const double> halvesUs, uint32_t sampleRate, float amplitude, std::vector<float> & outSamples)
{
    constexpr double  kUsPerSecond = 1000000.0;
    double            sampleUs     = kUsPerSecond / (double) sampleRate;
    double            totalUs      = 0.0;
    double            startUs      = 0.0;
    double            sign         = 1.0;
    size_t            total        = 0;



    for (double half : halvesUs)
    {
        totalUs += half;
    }

    total = (size_t) ceil (totalUs / sampleUs);
    outSamples.assign (total, 0.0f);

    for (double half : halvesUs)
    {
        double  endUs = startUs + half;
        size_t  first = (size_t) (startUs / sampleUs);
        size_t  last  = std::min ((size_t) (endUs / sampleUs), total - 1);

        for (size_t n = first; n <= last; n++)
        {
            double  overlap = std::min (endUs, (double) (n + 1) * sampleUs) - std::max (startUs, (double) n * sampleUs);

            if (overlap > 0.0)
            {
                outSamples[n] += (float) (sign * amplitude * overlap / sampleUs);
            }
        }

        startUs = endUs;
        sign    = -sign;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteCodec::GetBurstUs
//
//  The longest a burst of `count` bytes can be, every bit a 1.
//
////////////////////////////////////////////////////////////////////////////////

double CassetteCodec::GetBurstUs (size_t count)
{
    constexpr double  kBitsPerByte = 8.0;



    return kToneHalves * kToneHalfUs + 4.0 * kZeroHalfUs + (double) (kHeaderBytes + count) * kBitsPerByte * 2.0 * kOneHalfUs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteDecoder::CassetteDecoder
//
////////////////////////////////////////////////////////////////////////////////

CassetteDecoder::CassetteDecoder (uint32_t sampleRate) :
    m_sampleRate (sampleRate),
    m_highPassR  (exp (-2.0 * std::numbers::pi * s_kHighPassHz / (double) sampleRate)),
    m_release    (exp (-1.0 / (s_kReleaseSecond * (double) sampleRate)))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteDecoder::Push
//
//  An edge is placed at the zero crossing before the signal passed the
//  threshold, between the two samples on either side of it.
//
////////////////////////////////////////////////////////////////////////////////

void CassetteDecoder::Push (std::span<const float> samples)
{
    constexpr double  kUsPerSecond = 1000000.0;
    double            gapSamples   = CassetteCodec::kCycleLimitUs * 2.0 * m_sampleRate / kUsPerSecond;



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
//  CassetteDecoder::PushHalf
//
////////////////////////////////////////////////////////////////////////////////

void CassetteDecoder::PushHalf (double halfUs)
{
    bool  tone = halfUs >= CassetteCodec::kToneLowUs && halfUs <= CassetteCodec::kToneHighUs;



    switch (m_state)
    {
        case State::Hunt:
            if (tone)
            {
                m_tones++;
            }
            else if (halfUs < CassetteCodec::kMarkLimitUs && m_tones >= CassetteCodec::kToneMinimum)
            {
                m_state     = State::Mark;
                m_firstHalf = halfUs;
            }
            else
            {
                m_tones = 0;
            }

            break;

        case State::Mark:
            if (m_firstHalf + halfUs < CassetteCodec::kBitLimitUs)
            {
                m_state     = State::Bits;
                m_firstHalf = -1.0;
                m_byte      = 0;
                m_bits      = 0;
                m_count     = 0;
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
//  CassetteDecoder::TakeCycle
//
//  The header's count must match its complement, and be from 1 to
//  kMaxBurst, or the burst is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void CassetteDecoder::TakeCycle (double cycleUs)
{
    constexpr int  kBitsPerByte = 8;
    size_t         count        = 0;
    bool           valid        = false;



    if (cycleUs >= CassetteCodec::kCycleLimitUs)
    {
        EndBurst();
    }
    else if (++m_bits == kBitsPerByte)
    {
        m_bytes.push_back ((Byte) ((m_byte << 1) | (cycleUs >= CassetteCodec::kBitLimitUs ? 1 : 0)));
        m_byte = 0;
        m_bits = 0;
    }
    else
    {
        m_byte = (Byte) ((m_byte << 1) | (cycleUs >= CassetteCodec::kBitLimitUs ? 1 : 0));
    }

    if (m_state == State::Bits && m_bits == 0 && m_count == 0 && m_bytes.size() == CassetteCodec::kHeaderBytes)
    {
        count = (size_t) m_bytes[0] | ((size_t) m_bytes[1] << 8);
        valid = m_bytes[2] == (Byte) ~m_bytes[0] && m_bytes[3] == (Byte) ~m_bytes[1] && count > 0 && count <= CassetteCodec::kMaxBurst;

        if (valid)
        {
            m_count = count;
            m_bytes.clear();
        }
        else
        {
            EndBurst();
        }
    }
    else if (m_state == State::Bits && m_count != 0 && m_bytes.size() == m_count)
    {
        m_bursts.push_back (std::move (m_bytes));
        EndBurst();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteDecoder::EndBurst
//
////////////////////////////////////////////////////////////////////////////////

void CassetteDecoder::EndBurst()
{
    m_state     = State::Hunt;
    m_tones     = 0;
    m_firstHalf = -1.0;
    m_byte      = 0;
    m_bits      = 0;
    m_count     = 0;
    m_bytes.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteDecoder::PopBurst
//
////////////////////////////////////////////////////////////////////////////////

bool CassetteDecoder::PopBurst (std::vector<Byte> & outBytes)
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
//  CassetteDecoder::Reset
//
////////////////////////////////////////////////////////////////////////////////

void CassetteDecoder::Reset()
{
    EndBurst();
    m_bursts.clear();
    m_lastIn   = 0.0;
    m_filtered = 0.0;
    m_peak     = 0.0;
    m_crossing = -1.0;
    m_lastEdge = -1.0;
    m_sign     = 0;
}

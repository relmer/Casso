#include "Pch.h"

#include "Audio/SpeakerFilter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetSampleRate
//
//  Computes both stages for the device rate and clears their state.
//
//  The DC blocker is a one-pole high-pass at kDcBlockHz. The speaker is driven
//  through a coupling capacitor, so a cone parked at either level drifts back
//  to rest, and a constant level between sounds is silent rather than an offset in
//  the mix.
//
//  The low-pass is a Butterworth biquad at kLowPassHz, the bilinear-transform
//  coefficients from Robert Bristow-Johnson's audio EQ cookbook. The cone has
//  little output that high, and this stage removes most of a PWM carrier
//  (14.2 kHz for Archon) that the averaging in AudioGenerator passes. The
//  cutoff is capped below the Nyquist frequency for low device rates.
//
////////////////////////////////////////////////////////////////////////////////

void SpeakerFilter::SetSampleRate (uint32_t sampleRate)
{
    double  rate   = static_cast<double> (sampleRate);
    double  cutoff = std::min (kLowPassHz, rate * kMaxCutoffFraction);
    double  omega  = 0.0;
    double  alpha  = 0.0;
    double  cosine = 0.0;
    double  a0     = 0.0;



    if (sampleRate == 0)
    {
        return;
    }

    m_dcPole = std::exp (-2.0 * std::numbers::pi * kDcBlockHz / rate);

    omega  = 2.0 * std::numbers::pi * cutoff / rate;
    alpha  = std::sin (omega) / (2.0 * kButterworthQ);
    cosine = std::cos (omega);
    a0     = 1.0 + alpha;

    m_b0 = (1.0 - cosine) / 2.0 / a0;
    m_b1 = (1.0 - cosine) / a0;
    m_b2 = m_b0;
    m_a1 = -2.0 * cosine / a0;
    m_a2 = (1.0 - alpha) / a0;

    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  Clears the filter state. The next sample primes the DC blocker, so a
//  speaker that starts at a non-zero level does not produce a thump.
//
////////////////////////////////////////////////////////////////////////////////

void SpeakerFilter::Reset()
{
    m_dcPrevIn  = 0.0;
    m_dcPrevOut = 0.0;
    m_isPrimed  = false;
    m_z1        = 0.0;
    m_z2        = 0.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Process
//
//  Filters the samples in place, the DC blocker first and then the low-pass,
//  continuing from the state the previous call left.
//
////////////////////////////////////////////////////////////////////////////////

void SpeakerFilter::Process (float * samples, uint32_t numSamples)
{
    double  in      = 0.0;
    double  blocked = 0.0;
    double  out     = 0.0;



    if (samples == nullptr)
    {
        return;
    }

    for (uint32_t i = 0; i < numSamples; i++)
    {
        in = samples[i];

        if (!m_isPrimed)
        {
            m_dcPrevIn = in;
            m_isPrimed = true;
        }

        blocked     = m_dcPole * (m_dcPrevOut + in - m_dcPrevIn);
        m_dcPrevIn  = in;
        m_dcPrevOut = blocked;

        out  = m_b0 * blocked + m_z1;
        m_z1 = m_b1 * blocked - m_a1 * out + m_z2;
        m_z2 = m_b2 * blocked - m_a2 * out;

        samples[i] = static_cast<float> (out);
    }
}






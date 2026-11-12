#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SpeakerFilter
//
//  The frequency response of the Apple II speaker and its driver, applied to
//  the PCM AudioGenerator produces: a DC blocker for the AC-coupled drive and
//  a second-order low-pass for the cone.
//
////////////////////////////////////////////////////////////////////////////////

class SpeakerFilter
{
public:
    void  SetSampleRate (uint32_t sampleRate);
    void  Reset         ();
    void  Process       (float * samples, uint32_t numSamples);

private:
    static constexpr double  kDcBlockHz         = 20.0;
    static constexpr double  kLowPassHz         = 10000.0;
    static constexpr double  kMaxCutoffFraction = 0.45;
    static constexpr double  kButterworthQ      = std::numbers::sqrt2 / 2.0;

    double  m_dcPole      = 1.0;
    double  m_dcPrevIn    = 0.0;
    double  m_dcPrevOut   = 0.0;
    bool    m_isPrimed    = false;

    double  m_b0          = 1.0;
    double  m_b1          = 0.0;
    double  m_b2          = 0.0;
    double  m_a1          = 0.0;
    double  m_a2          = 0.0;
    double  m_z1          = 0.0;
    double  m_z2          = 0.0;
};

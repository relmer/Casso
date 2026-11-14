#include "Pch.h"

#include "Audio/HardDiskAudioSource.h"




//  The spindle at full speed: 3,600 rpm, a hum at its fundamental and a
//  whine an octave and a half up.
static constexpr double    s_kSpindleHz    = 60.0;
static constexpr double    s_kWhineHz      = 180.0;
static constexpr double    s_kTickHz       = 2200.0;
static constexpr float     s_kSpindleLevel = 0.04f;
static constexpr double    s_kTwoPi        = 6.283185307179586;
static constexpr uint32_t  s_kMsPerSecond  = 1000;





////////////////////////////////////////////////////////////////////////////////
//
//  HardDiskAudioSource
//
////////////////////////////////////////////////////////////////////////////////

HardDiskAudioSource::HardDiskAudioSource (uint32_t spinUpMs, float tickLevel) :
    m_spinUpMs  (spinUpMs),
    m_tickLevel (tickLevel)
{
    SetSampleRate (kDefaultRate);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnBlockDeviceEvent
//
////////////////////////////////////////////////////////////////////////////////

void HardDiskAudioSource::OnBlockDeviceEvent (const BlockDeviceEvent & event)
{
    if (event.op == BlockDeviceOp::Read || event.op == BlockDeviceOp::Write || event.op == BlockDeviceOp::Seek)
    {
        m_tickLeft = m_tickTotal;
    }
    else if (event.op == BlockDeviceOp::SpinUp)
    {
        PowerOn();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSampleRate
//
////////////////////////////////////////////////////////////////////////////////

void HardDiskAudioSource::SetSampleRate (uint32_t rate)
{
    m_rate      = (rate != 0) ? rate : kDefaultRate;
    m_tickTotal = m_rate * kTickMs / s_kMsPerSecond;
    m_speedStep = (m_spinUpMs == 0) ? 1.0f : 1.0f / ((float) m_rate * (float) m_spinUpMs / (float) s_kMsPerSecond);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
//  The spindle's pitch and level follow its speed; a tick decays from its
//  start.
//
////////////////////////////////////////////////////////////////////////////////

void HardDiskAudioSource::GeneratePCM (float * outMono, uint32_t numSamples)
{
    double  step = s_kTwoPi / (double) m_rate;



    for (uint32_t i = 0; i < numSamples; i++)
    {
        float  sample = 0.0f;



        m_speed  = m_spinning ? std::min (1.0f, m_speed + m_speedStep) : std::max (0.0f, m_speed - m_speedStep);
        m_phase += step * m_speed;

        if (m_speed > 0.0f)
        {
            sample += m_motorVolume * s_kSpindleLevel * m_speed * (float) (std::sin (m_phase * s_kSpindleHz) + 0.5 * std::sin (m_phase * s_kWhineHz));
        }

        if (m_tickLeft > 0)
        {
            float  envelope = (float) m_tickLeft / (float) m_tickTotal;



            sample += m_headVolume * m_tickLevel * envelope * envelope * (float) (std::sin ((double) m_tickLeft * step * s_kTickHz) * 0.5 + NextNoise() * 0.5);
            m_tickLeft--;
        }

        outMono[i] = sample;
    }

    m_phase = std::fmod (m_phase, s_kTwoPi * 1000.0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NextNoise
//
////////////////////////////////////////////////////////////////////////////////

float HardDiskAudioSource::NextNoise()
{
    constexpr float  kScale = 1.0f / 2147483648.0f;



    m_noise ^= m_noise << 13;
    m_noise ^= m_noise >> 17;
    m_noise ^= m_noise << 5;

    return (float) (int32_t) m_noise * kScale;
}

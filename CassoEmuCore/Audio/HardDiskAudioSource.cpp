#include "Pch.h"

#include "Audio/HardDiskAudioSource.h"




static constexpr double    s_kTickHz      = 2200.0;
static constexpr double    s_kTwoPi       = 6.283185307179586;
static constexpr uint32_t  s_kMsPerSecond = 1000;





////////////////////////////////////////////////////////////////////////////////
//
//  HardDiskAudioSource
//
////////////////////////////////////////////////////////////////////////////////

HardDiskAudioSource::HardDiskAudioSource (float tickLevel) :
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
//  A tick decays from its start.
//
////////////////////////////////////////////////////////////////////////////////

void HardDiskAudioSource::GeneratePCM (float * outMono, uint32_t numSamples)
{
    double  step = s_kTwoPi / (double) m_rate;



    for (uint32_t i = 0; i < numSamples; i++)
    {
        float  sample = 0.0f;



        if (m_tickLeft > 0)
        {
            float  envelope = (float) m_tickLeft / (float) m_tickTotal;



            sample += m_headVolume * m_tickLevel * envelope * envelope * (float) (std::sin ((double) m_tickLeft * step * s_kTickHz) * 0.5 + NextNoise() * 0.5);
            m_tickLeft--;
        }

        outMono[i] = sample;
    }
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

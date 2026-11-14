#include "Pch.h"

#include "Audio/Floppy35AudioSource.h"




//  Levels of each sound at full volume, chosen against the Disk II's samples.
static constexpr float   s_kMotorLevel    = 0.06f;
static constexpr float   s_kStepLevel     = 0.35f;
static constexpr float   s_kEjectLevel    = 0.25f;
static constexpr float   s_kInsertLevel   = 0.40f;

//  The spindle's hum, at the motor's speed in the outer zone (394 rpm) times
//  its poles, and the eject motor's.
static constexpr double  s_kMotorHz       = 105.0;
static constexpr double  s_kEjectHz       = 160.0;
static constexpr double  s_kStepHz        = 1400.0;
static constexpr double  s_kInsertHz      = 85.0;
static constexpr float   s_kMotorRamp     = 0.0005f;
static constexpr double  s_kTwoPi         = 6.283185307179586;
static constexpr uint32_t s_kMsPerSecond  = 1000;





////////////////////////////////////////////////////////////////////////////////
//
//  OnFloppy35Event
//
////////////////////////////////////////////////////////////////////////////////

void Floppy35AudioSource::OnFloppy35Event (const Floppy35Event & event)
{
    if (m_drive >= 0 && event.drive != m_drive)
    {
        return;
    }

    switch (event.op)
    {
        case Floppy35Op::MotorOn:   m_motorOn    = true;           break;
        case Floppy35Op::MotorOff:  m_motorOn    = false;          break;
        case Floppy35Op::Step:      m_stepLeft   = m_stepTotal;    break;
        case Floppy35Op::Eject:     m_ejectLeft  = m_ejectTotal;   break;
        case Floppy35Op::Insert:    m_insertLeft = m_insertTotal;  break;
        default:                                                   break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSampleRate
//
////////////////////////////////////////////////////////////////////////////////

void Floppy35AudioSource::SetSampleRate (uint32_t rate)
{
    m_rate        = (rate != 0) ? rate : kDefaultRate;
    m_stepTotal   = m_rate * kStepMs   / s_kMsPerSecond;
    m_ejectTotal  = m_rate * kEjectMs  / s_kMsPerSecond;
    m_insertTotal = m_rate * kInsertMs / s_kMsPerSecond;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
//  The motor's hum ramps in and out; each one-shot sound decays from its
//  start.
//
////////////////////////////////////////////////////////////////////////////////

void Floppy35AudioSource::GeneratePCM (float * outMono, uint32_t numSamples)
{
    double  step = s_kTwoPi / (double) m_rate;



    for (uint32_t i = 0; i < numSamples; i++)
    {
        float   sample = 0.0f;
        double  t      = m_phase;



        m_phase += step;

        m_motorLevel = m_motorOn ? std::min (1.0f, m_motorLevel + s_kMotorRamp) : std::max (0.0f, m_motorLevel - s_kMotorRamp);

        if (m_motorLevel > 0.0f)
        {
            sample += m_motorVolume * s_kMotorLevel * m_motorLevel * (float) (std::sin (t * s_kMotorHz) + 0.3 * NextNoise());
        }

        if (m_stepLeft > 0)
        {
            float  envelope = (float) m_stepLeft / (float) m_stepTotal;



            sample += m_headVolume * s_kStepLevel * envelope * envelope * (float) (std::sin (t * s_kStepHz) * 0.5 + NextNoise() * 0.5);
            m_stepLeft--;
        }

        if (m_ejectLeft > 0)
        {
            float  envelope = (float) m_ejectLeft / (float) m_ejectTotal;



            sample += m_doorVolume * s_kEjectLevel * envelope * (float) (std::sin (t * s_kEjectHz) * 0.6 + NextNoise() * 0.4);
            m_ejectLeft--;
        }

        if (m_insertLeft > 0)
        {
            float  envelope = (float) m_insertLeft / (float) m_insertTotal;



            sample += m_doorVolume * s_kInsertLevel * envelope * envelope * (float) std::sin (t * s_kInsertHz);
            m_insertLeft--;
        }

        outMono[i] = sample;
    }

    m_phase = std::fmod (m_phase, s_kTwoPi * 1000.0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NextNoise
//
//  White noise in -1 to 1 from a xorshift generator.
//
////////////////////////////////////////////////////////////////////////////////

float Floppy35AudioSource::NextNoise()
{
    constexpr float  kScale = 1.0f / 2147483648.0f;



    m_noise ^= m_noise << 13;
    m_noise ^= m_noise >> 17;
    m_noise ^= m_noise << 5;

    return (float) (int32_t) m_noise * kScale;
}

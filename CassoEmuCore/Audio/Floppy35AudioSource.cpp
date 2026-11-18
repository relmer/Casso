#include "Pch.h"

#include "Audio/Floppy35AudioSource.h"
#include "Audio/AudioFileDecoder.h"




//  The recordings in the order of Floppy35AudioSource::Sound.
static const wchar_t * const  s_kpszSampleFiles[] =
{
    L"35_spin_start_loaded.wav",
    L"35_spin_loaded.wav",
    L"35_spin_end.wav",
    L"35_step_1_1.wav",
    L"35_seek_12ms.wav",
};

static_assert (_countof (s_kpszSampleFiles) == (size_t) Floppy35AudioSource::Sound::Count);

static constexpr uint32_t  s_kMsPerSecond = 1000;





////////////////////////////////////////////////////////////////////////////////
//
//  LoadSamples
//
//  A missing or unreadable recording is silent, so only a failure to
//  start Media Foundation is returned.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Floppy35AudioSource::LoadSamples (const fs::path & directory, uint32_t sampleRate)
{
    HRESULT  hr         = S_OK;
    HRESULT  hrShutdown = S_OK;



    hr = MFStartup (MF_VERSION, MFSTARTUP_LITE);
    CHR (hr);

    for (size_t i = 0; i < kSoundCount; i++)
    {
        HRESULT  hrFile = AudioFileDecoder::DecodeToMono ((directory / s_kpszSampleFiles[i]).c_str(), sampleRate, m_samples[i]);



        IGNORE_RETURN_VALUE (hrFile, S_OK);
    }

    hrShutdown = MFShutdown();
    IGNORE_RETURN_VALUE (hrShutdown, S_OK);

    SetSampleRate (sampleRate);

    m_motor = {};
    m_head  = {};

    if (m_motorOn)
    {
        Play (m_motor, Sound::SpinLoop, true);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSampleForTest
//
////////////////////////////////////////////////////////////////////////////////

void Floppy35AudioSource::SetSampleForTest (Sound sound, vector<float> && samples)
{
    m_samples[(size_t) sound] = std::move (samples);
}





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
        case Floppy35Op::MotorOn:   StartMotor();  break;
        case Floppy35Op::MotorOff:  StopMotor();   break;
        case Floppy35Op::Step:      Step();        break;
        default:                                   break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSampleRate
//
////////////////////////////////////////////////////////////////////////////////

void Floppy35AudioSource::SetSampleRate (uint32_t rate)
{
    m_rate    = (rate != 0) ? rate : kDefaultRate;
    m_seekGap = m_rate * kSeekGapMs / s_kMsPerSecond;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
//  The spin-up rolls into the running loop while the motor stays on. A seek
//  ends when no step has come for the seek gap.
//
////////////////////////////////////////////////////////////////////////////////

void Floppy35AudioSource::GeneratePCM (float * outMono, uint32_t numSamples)
{
    for (uint32_t i = 0; i < numSamples; i++)
    {
        if (m_motorOn && m_motor.buffer == nullptr)
        {
            Play (m_motor, Sound::SpinLoop, true);
        }

        if (m_seeking && m_sinceStep >= m_seekGap)
        {
            m_seeking = false;
            m_head    = {};
        }

        if (m_sinceStep < UINT32_MAX)
        {
            m_sinceStep++;
        }

        outMono[i] = m_motorVolume * NextSample (m_motor) + m_headVolume * NextSample (m_head);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NextSample
//
////////////////////////////////////////////////////////////////////////////////

float Floppy35AudioSource::NextSample (Voice & voice)
{
    float  sample = 0.0f;



    if (voice.buffer == nullptr)
    {
        return 0.0f;
    }

    sample = (*voice.buffer)[voice.position++];

    if (voice.position >= voice.buffer->size())
    {
        voice.position = 0;

        if (!voice.isLoop)
        {
            voice.buffer = nullptr;
        }
    }

    return sample;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartMotor
//
////////////////////////////////////////////////////////////////////////////////

void Floppy35AudioSource::StartMotor()
{
    if (!m_motorOn)
    {
        m_motorOn = true;
        Play (m_motor, Sound::SpinStart, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopMotor
//
////////////////////////////////////////////////////////////////////////////////

void Floppy35AudioSource::StopMotor()
{
    if (m_motorOn)
    {
        m_motorOn = false;
        Play (m_motor, Sound::SpinEnd, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Step
//
//  A step soon after another is part of a seek, which loops the seek
//  recording until the steps stop.
//
////////////////////////////////////////////////////////////////////////////////

void Floppy35AudioSource::Step()
{
    bool  isQuick = m_sinceStep < m_seekGap;



    m_sinceStep = 0;

    if (!isQuick)
    {
        m_seeking = false;
        Play (m_head, Sound::Step, false);
    }
    else if (!m_seeking)
    {
        m_seeking = true;
        Play (m_head, Sound::Seek, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Play
//
////////////////////////////////////////////////////////////////////////////////

void Floppy35AudioSource::Play (Voice & voice, Sound sound, bool isLoop)
{
    const vector<float>  & samples = m_samples[(size_t) sound];



    voice.buffer   = samples.empty() ? nullptr : &samples;
    voice.position = 0;
    voice.isLoop   = isLoop;
}






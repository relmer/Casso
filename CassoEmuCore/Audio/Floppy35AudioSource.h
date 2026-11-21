#pragma once

#include "Pch.h"

#include "Audio/IDriveAudioSource.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Floppy35AudioSource
//
//  The sounds of a Sony 3.5-inch mechanism, from MAME's CC0 recordings: the
//  spindle's spin-up, run and spin-down, a click for a single head step, and
//  a seek's buzz for steps in quick succession. The mechanism steps every
//  12 ms, so a seek uses the recording made at that rate. MAME has no eject
//  recording, so eject and insert are silent.
//
//  The mechanism's events arrive on the emulation thread, which also mixes
//  the audio, so no lock is taken. With `drive` -1 the source takes every
//  drive's events, as for a card's two drives or a chain; otherwise events
//  for other drives are ignored.
//
////////////////////////////////////////////////////////////////////////////////

class Floppy35AudioSource : public IDriveAudioSource, public IFloppy35EventSink
{
public:
    enum class Sound : size_t
    {
        SpinStart,
        SpinLoop,
        SpinEnd,
        Step,
        Seek,
        Count,
    };

    //  The recordings' folder under the drive sounds directory.
    static constexpr const wchar_t  * kSampleFolder = L"Sony 3.5";

    explicit Floppy35AudioSource (int drive = -1) : m_drive (drive) {}

    HRESULT  LoadSamples      (const fs::path & directory, uint32_t sampleRate);
    void     SetSampleForTest (Sound sound, vector<float> && samples);

    // IFloppy35EventSink
    void  OnFloppy35Event (const Floppy35Event & event) override;

    // IDriveAudioSink: the Disk II's events, for the settings preview.
    void  OnMotorEngaged    () override { StartMotor(); }
    void  OnMotorDisengaged () override { StopMotor(); }
    void  OnHeadStep        (int newQt) override { (void) newQt; Step(); }
    void  OnHeadBump        () override { Step(); }
    void  OnDiskInserted    () override {}
    void  OnDiskEjected     () override {}

    // IDriveAudioSource
    void   GeneratePCM (float * outMono, uint32_t numSamples) override;
    float  GetPanLeft  () const override { return m_panLeft; }
    float  GetPanRight () const override { return m_panRight; }
    void   SetPan      (float panLeft, float panRight) override { m_panLeft = panLeft; m_panRight = panRight; }

    void  SetVolumes    (float motor, float head, float door) { m_motorVolume = motor; m_headVolume = head; (void) door; }
    void  SetSampleRate (uint32_t rate);

    bool  IsMotorOn () const { return m_motorOn; }
    bool  IsSeeking () const { return m_seeking; }
    bool  IsSilent  () const { return m_motor.buffer == nullptr && m_head.buffer == nullptr; }

private:
    struct Voice
    {
        const vector<float>  * buffer   = nullptr;
        size_t                 position = 0;
        bool                   isLoop   = false;
    };

    static constexpr uint32_t  kDefaultRate  = 48'000;
    static constexpr uint32_t  kSeekGapMs    = 40;
    static constexpr size_t    kSoundCount   = (size_t) Sound::Count;

    static float  NextSample (Voice & voice);

    void  StartMotor ();
    void  StopMotor  ();
    void  Step       ();
    void  Play       (Voice & voice, Sound sound, bool isLoop);

    int                                    m_drive         = -1;
    uint32_t                               m_rate          = kDefaultRate;
    uint32_t                               m_seekGap       = kDefaultRate * kSeekGapMs / 1000;
    uint32_t                               m_sinceStep     = UINT32_MAX;
    bool                                   m_motorOn       = false;
    bool                                   m_seeking       = false;
    std::array<vector<float>, kSoundCount> m_samples;
    Voice                                  m_motor;
    Voice                                  m_head;
    float                                  m_motorVolume   = 1.0f;
    float                                  m_headVolume    = 1.0f;
    float                                  m_panLeft       = kCenterPan;
    float                                  m_panRight      = kCenterPan;
};

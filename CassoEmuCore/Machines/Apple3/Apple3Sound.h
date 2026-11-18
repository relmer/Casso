#pragma once

#include "Pch.h"

#include "Audio/IDriveAudioSource.h"
#include "Core/IMachineState.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Sound
//
//  The Apple ///'s three sound sources, which the machine sums into one
//  amplifier: the speaker toggle at $C030, as on the Apple II; the beeper at
//  $C040, a 0.1 second 1 kHz tone from a pair of 556 timers; and the 6-bit
//  DAC on E VIA port B bits 0-5.
//
//  The speaker is an AppleSpeaker on the audio path's toggle log. The beeper
//  and the DAC are this audio source, which the sound card mixer renders:
//  each DAC change is logged with the tick it happened on, and each frame's
//  samples are spread over the ticks since the last frame, so the DAC's
//  pitch follows the program's timing at either CPU speed.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3Sound : public IDriveAudioSource, public IMachineState
{
public:
    static constexpr uint32_t  kBeepHz        = 1000;
    static constexpr uint32_t  kBeepMs        = 100;
    static constexpr int       kDacLevels     = 64;
    static constexpr Byte      kDacMask       = 0x3F;
    static constexpr float     kDacGain       = 0.40f;
    static constexpr float     kBeepGain      = 0.25f;
    static constexpr float     kDcBlockPole   = 0.995f;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'S', 'D');
    static constexpr uint16_t  kStateVersion = 1;

    Apple3Sound() = default;

    //  The CPU's cycle counter, in ticks, and the ticks per second.
    void            SetTickSource   (const uint64_t * ticks, double ticksPerSecond);

    AppleSpeaker  & GetSpeaker      () { return m_speaker; }

    //  An access to $C040-$C04D starts the tone, or restarts it.
    void            TriggerBeep     ();
    bool            IsBeeping       () const;

    //  E VIA port B: the DAC level is its low six bits. A change is logged at
    //  the current tick.
    void            SetDacPort      (Byte portB);
    Byte            GetDacLevel     () const { return m_latestLevel; }

    void            Reset           ();

    // IDriveAudioSource
    void            GeneratePCM     (float * outMono, uint32_t numSamples) override;
    float           GetPanLeft      () const override { return m_panLeft;  }
    float           GetPanRight     () const override { return m_panRight; }
    void            SetPan          (float panLeft, float panRight) override { m_panLeft = panLeft; m_panRight = panRight; }

    // IDriveAudioSink: the Apple ///'s sound has no drive events.
    void            OnMotorEngaged    () override {}
    void            OnMotorDisengaged () override {}
    void            OnHeadStep        (int newQt) override { (void) newQt; }
    void            OnHeadBump        () override {}
    void            OnDiskInserted    () override {}
    void            OnDiskEjected     () override {}

    HRESULT         SaveState       (StateWriter & writer) const override;
    HRESULT         LoadState       (StateReader & reader) override;

private:
    struct DacChange
    {
        uint64_t  tick  = 0;
        Byte      level = 0;
    };

    uint64_t        GetNow          () const { return (m_ticks != nullptr) ? *m_ticks : 0; }
    float           RenderSample    (uint64_t tick);

    AppleSpeaker             m_speaker;
    const uint64_t         * m_ticks          = nullptr;
    double                   m_ticksPerSecond = 1.0;
    std::vector<DacChange>   m_changes;
    Byte                     m_renderedLevel  = 0;
    Byte                     m_latestLevel    = 0;
    uint64_t                 m_renderedTo     = 0;
    uint64_t                 m_beepStart      = 0;
    uint64_t                 m_beepEnd        = 0;
    float                    m_dcPrevIn       = 0.0f;
    float                    m_dcPrevOut      = 0.0f;
    float                    m_panLeft        = IDriveAudioSource::kCenterPan;
    float                    m_panRight       = IDriveAudioSource::kCenterPan;
};

#pragma once

#include "Audio/IDriveAudioSource.h"

class TapeDeck;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeAudioSource
//
//  The sound of the tape playing, as a recorder's own speaker would give it.
//  It is synthesized from the decoded transitions rather than played from the
//  recording: the signal is nothing but tones, so a square wave at the same
//  edges sounds the same and nothing large has to stay in memory. Silent
//  unless the tape is playing. During a fast load the audio path keeps only
//  slices of it, at their true pitch, so this source needs no knowledge of
//  it.
//
////////////////////////////////////////////////////////////////////////////////

class TapeAudioSource : public IDriveAudioSource
{
public:
    using NowFn = std::function<uint64_t ()>;

    void  Attach      (const TapeDeck * deck, NowFn now);
    void  SetVolume   (float gain) { m_volume.store (gain, std::memory_order_relaxed); }
    float GetVolume   () const     { return m_volume.load (std::memory_order_relaxed); }

    void  GeneratePCM (float * outMono, uint32_t numSamples) override;
    float GetPanLeft  () const override { return m_panLeft; }
    float GetPanRight () const override { return m_panRight; }
    void  SetPan      (float panLeft, float panRight) override { m_panLeft = panLeft; m_panRight = panRight; }

    void  OnMotorEngaged    () override {}
    void  OnMotorDisengaged () override {}
    void  OnHeadStep        (int newQt) override { UNREFERENCED_PARAMETER (newQt); }
    void  OnHeadBump        () override {}
    void  OnDiskInserted    () override {}
    void  OnDiskEjected     () override {}

    static constexpr float  kAmplitude = 0.12f;

private:
    const TapeDeck  * m_deck       = nullptr;
    NowFn             m_now;
    uint64_t          m_lastCycle  = 0;
    bool              m_hasLast    = false;
    std::atomic<float>  m_volume   { 1.0f };
    float             m_panLeft    = kCenterPan;
    float             m_panRight   = kCenterPan;
};

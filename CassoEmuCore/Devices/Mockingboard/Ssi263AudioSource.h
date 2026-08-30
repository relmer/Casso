#pragma once

#include "Pch.h"
#include "Audio/IDriveAudioSource.h"

class Ssi263;





////////////////////////////////////////////////////////////////////////////////
//
//  Ssi263AudioSource
//
//  Adapts the voice chip to the DriveAudioMixer's IDriveAudioSource
//  contract, parallel to MockingboardAudioSource rather than a change to
//  it. The board's speech output is a single mono signal belonging to
//  neither PSG channel, so this source is panned center by default where
//  the PSG sources sit hard left and hard right.
//
//  An idle chip reports itself silent and the render loop skips synthesis
//  entirely, so a speech-equipped card whose chip is never programmed
//  costs nothing here.
//
//  The IDriveAudioSink notification methods are inherited from the disk
//  audio abstraction and are no-ops here, as they are for the PSG source.
//
////////////////////////////////////////////////////////////////////////////////

class Ssi263AudioSource : public IDriveAudioSource
{
public:
    // Headroom: speech sums into the stereo bus alongside two PSG sources
    // (0.28 each, hard-panned), the equal-power-centered speaker, and Disk
    // II audio. Speech is center-panned, so each channel receives this gain
    // times the center-pan coefficient; the card-level budget is pinned by
    // FullVolumeCardOutputLeavesHeadroom, and a 25-second full-mix capture
    // of connected speech peaked at 0.18.
    static constexpr float    kMasterGain = 0.45f;

    // One-pole DC-blocker pole. y[n] = x[n] - x[n-1] + R*y[n-1].
    static constexpr float    kDcBlockPole = 0.995f;

    Ssi263AudioSource () = default;

    void   SetSpeech (Ssi263 * speech) { m_speech = speech; }

    // IDriveAudioSource
    void   GeneratePCM (float * outMono, uint32_t numSamples) override;
    float  GetPanLeft  () const override { return m_panLeft;  }
    float  GetPanRight () const override { return m_panRight; }
    void   SetPan      (float panLeft, float panRight) override { m_panLeft = panLeft; m_panRight = panRight; }

    // IDriveAudioSink -- unused by a sound card.
    void   OnMotorEngaged    () override {}
    void   OnMotorDisengaged () override {}
    void   OnHeadStep        (int newQt) override { (void) newQt; }
    void   OnHeadBump        () override {}
    void   OnDiskInserted    () override {}
    void   OnDiskEjected     () override {}

private:
    Ssi263 *   m_speech = nullptr;

    float      m_panLeft  = IDriveAudioSource::kCenterPan;
    float      m_panRight = IDriveAudioSource::kCenterPan;

    float      m_dcPrevIn  = 0.0f;
    float      m_dcPrevOut = 0.0f;
};

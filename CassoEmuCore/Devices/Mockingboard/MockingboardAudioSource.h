#pragma once

#include "Pch.h"
#include "Audio/IDriveAudioSource.h"

class Ay8910;




////////////////////////////////////////////////////////////////////////////////
//
//  MockingboardAudioSource
//
//  Adapts one AY-3-8910 PSG to the DriveAudioMixer's IDriveAudioSource
//  contract: it pulls mono samples from the chip, removes the DAC's DC
//  offset with a one-pole blocker, applies a master gain, and reports its
//  stereo pan. A Mockingboard is dual-mono -- PSG #1 is wired hard-left,
//  PSG #2 hard-right -- so each source carries a fixed pan by default.
//
//  The IDriveAudioSink notification methods are inherited from the disk
//  audio abstraction and are no-ops here; a sound card has no motor, head,
//  or door events.
//
////////////////////////////////////////////////////////////////////////////////

class MockingboardAudioSource : public IDriveAudioSource
{
public:
    // Headroom: two PSGs (three channels each) sum into the stereo bus
    // alongside the speaker and Disk II audio, so each channel is
    // attenuated to keep the pre-clamp sum civil.
    static constexpr float    kMasterGain = 0.28f;

    // One-pole DC-blocker pole. y[n] = x[n] - x[n-1] + R*y[n-1].
    static constexpr float    kDcBlockPole = 0.995f;

    MockingboardAudioSource () = default;

    void   SetPsg (Ay8910 * psg) { m_psg = psg; }

    // IDriveAudioSource
    void   GeneratePCM (float * outMono, uint32_t numSamples) override;
    float  PanLeft     () const override { return m_panLeft;  }
    float  PanRight    () const override { return m_panRight; }
    void   SetPan      (float panLeft, float panRight) override { m_panLeft = panLeft; m_panRight = panRight; }

    // IDriveAudioSink -- unused by a sound card.
    void   OnMotorEngaged    () override {}
    void   OnMotorDisengaged () override {}
    void   OnHeadStep        (int newQt) override { (void) newQt; }
    void   OnHeadBump        () override {}
    void   OnDiskInserted    () override {}
    void   OnDiskEjected     () override {}

private:
    Ay8910 *   m_psg = nullptr;

    float      m_panLeft  = IDriveAudioSource::kCenterPan;
    float      m_panRight = IDriveAudioSource::kCenterPan;

    float      m_dcPrevIn  = 0.0f;
    float      m_dcPrevOut = 0.0f;
};

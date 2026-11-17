#pragma once

#include "Pch.h"

#include "Audio/IDriveAudioSource.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Floppy35AudioSource
//
//  The sounds of a Sony 3.5-inch mechanism, synthesized: the spindle motor's
//  hum while it runs, a click for each head step, the eject motor's whirr
//  and the disk's thump going in. No recording of the drive under a license
//  Casso can use was found (research R13).
//
//  The mechanism's events arrive on the emulation thread, which also mixes
//  the audio, so no lock is taken. With `drive` -1 the source takes
//  every drive's events, as for a card's two drives or a chain; otherwise
//  events for other drives are ignored.
//
////////////////////////////////////////////////////////////////////////////////

class Floppy35AudioSource : public IDriveAudioSource, public IFloppy35EventSink
{
public:
    explicit Floppy35AudioSource (int drive = -1) : m_drive (drive) {}

    // IFloppy35EventSink
    void  OnFloppy35Event (const Floppy35Event & event) override;

    // IDriveAudioSink: the Disk II's events, for the settings preview.
    void  OnMotorEngaged    () override { m_motorOn = true; }
    void  OnMotorDisengaged () override { m_motorOn = false; }
    void  OnHeadStep        (int newQt) override { (void) newQt; m_stepLeft = kStepSamples; }
    void  OnHeadBump        () override { m_stepLeft = kStepSamples; }
    void  OnDiskInserted    () override { m_insertLeft = kInsertSamples; }
    void  OnDiskEjected     () override { m_ejectLeft = kEjectSamples; }

    // IDriveAudioSource
    void   GeneratePCM (float * outMono, uint32_t numSamples) override;
    float  GetPanLeft  () const override { return m_panLeft; }
    float  GetPanRight () const override { return m_panRight; }
    void   SetPan      (float panLeft, float panRight) override { m_panLeft = panLeft; m_panRight = panRight; }

    void  SetVolumes    (float motor, float head, float door) { m_motorVolume = motor; m_headVolume = head; m_doorVolume = door; }
    void  SetSampleRate (uint32_t rate);

    bool  IsMotorOn () const { return m_motorOn; }
    bool  IsSilent  () const { return !m_motorOn && m_stepLeft == 0 && m_ejectLeft == 0 && m_insertLeft == 0 && m_motorLevel <= 0.0f; }

private:
    static constexpr uint32_t  kDefaultRate     = 48'000;
    static constexpr uint32_t  kStepMs          = 10;
    static constexpr uint32_t  kEjectMs         = 450;
    static constexpr uint32_t  kInsertMs        = 70;
    static constexpr uint32_t  kStepSamples     = kDefaultRate * kStepMs / 1000;
    static constexpr uint32_t  kEjectSamples    = kDefaultRate * kEjectMs / 1000;
    static constexpr uint32_t  kInsertSamples   = kDefaultRate * kInsertMs / 1000;

    float  NextNoise ();

    int       m_drive        = -1;
    uint32_t  m_rate         = kDefaultRate;
    uint32_t  m_stepTotal    = kStepSamples;
    uint32_t  m_ejectTotal   = kEjectSamples;
    uint32_t  m_insertTotal  = kInsertSamples;
    uint32_t  m_stepLeft     = 0;
    uint32_t  m_ejectLeft    = 0;
    uint32_t  m_insertLeft   = 0;
    bool      m_motorOn      = false;
    float     m_motorLevel   = 0.0f;
    double    m_phase        = 0.0;
    uint32_t  m_noise        = 0x1234567u;
    float     m_motorVolume  = 1.0f;
    float     m_headVolume   = 1.0f;
    float     m_doorVolume   = 1.0f;
    float     m_panLeft      = kCenterPan;
    float     m_panRight     = kCenterPan;
};

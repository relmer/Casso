#pragma once

#include "Pch.h"

#include "Audio/IDriveAudioSource.h"
#include "Devices/BlockDevice/IBlockDeviceEventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HardDiskAudioSource
//
//  The sound of a hard disk, synthesized: the actuator's tick for each read,
//  write and seek. A ProFile ticks louder than an HD20SC. The spindle is
//  silent. No recording of either drive under a license Casso can use was
//  found (research R13).
//
//  Events arrive on the emulation thread, which also mixes the audio.
//
////////////////////////////////////////////////////////////////////////////////

class HardDiskAudioSource : public IDriveAudioSource, public IBlockDeviceEventSink
{
public:
    HardDiskAudioSource (float tickLevel);

    // IBlockDeviceEventSink
    void  OnBlockDeviceEvent (const BlockDeviceEvent & event) override;

    // IDriveAudioSink: unused for a hard disk.
    void  OnMotorEngaged    () override {}
    void  OnMotorDisengaged () override {}
    void  OnHeadStep        (int newQt) override { (void) newQt; }
    void  OnHeadBump        () override {}
    void  OnDiskInserted    () override {}
    void  OnDiskEjected     () override {}

    // IDriveAudioSource
    void   GeneratePCM (float * outMono, uint32_t numSamples) override;
    float  GetPanLeft  () const override { return m_panLeft; }
    float  GetPanRight () const override { return m_panRight; }
    void   SetPan      (float panLeft, float panRight) override { m_panLeft = panLeft; m_panRight = panRight; }

    void  SetVolumes    (float motor, float head, float door) { m_headVolume = head; (void) motor; (void) door; }
    void  SetSampleRate (uint32_t rate);

    bool  IsTickActive () const { return m_tickLeft > 0; }

private:
    static constexpr uint32_t  kDefaultRate = 48'000;
    static constexpr uint32_t  kTickMs      = 6;

    float  NextNoise ();

    uint32_t  m_rate       = kDefaultRate;
    float     m_tickLevel  = 0.0f;
    uint32_t  m_tickTotal  = 0;
    uint32_t  m_tickLeft   = 0;
    uint32_t  m_noise      = 0x2468ACEu;
    float     m_headVolume = 1.0f;
    float     m_panLeft    = kCenterPan;
    float     m_panRight   = kCenterPan;
};

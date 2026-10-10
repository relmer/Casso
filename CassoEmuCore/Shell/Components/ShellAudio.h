#pragma once

#include "Pch.h"

#include "Audio/DriveAudioMixer.h"
#include "Audio/PrinterAudioSource.h"
#include "Audio/TapeAudioSource.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "Shell/AudioSampleBudget.h"
#include "WasapiAudio.h"



class MockingboardCard;
struct MachineBuildServices;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio
//
//  The host audio output and everything mixed into it: the drive sounds, the
//  printer's mechanism, the Mockingboard's PSGs and the tape. The speaker is
//  the machine's; this is where its toggles become samples.
//
//  The output is opened, serviced and fed on the CPU thread, which is the
//  thread that mixes. The drive gains and pans live here rather than on the
//  sources so a machine rebuild seeds fresh sources from them.
//
////////////////////////////////////////////////////////////////////////////////

class ShellAudio
{
public:
    ShellAudio();
    ~ShellAudio();

    WasapiAudio                                      & GetOutput()            { return m_wasapiAudio; }
    DriveAudioMixer                                  & GetDriveMixer()        { return m_driveAudioMixer; }
    DriveAudioMixer                                  & GetMockingboardMixer() { return m_mockingboardAudioMixer; }
    std::vector<std::unique_ptr<Disk2AudioSource>>   & GetDiskSources()       { return m_diskAudioSources; }
    PrinterAudioSource                               & GetPrinterAudio()      { return m_printerAudio; }
    TapeAudioSource                                  & GetTapeSource()        { return m_tapeAudioSource; }

    // Points the machine builder at the sources and mixers it registers the
    // new machine's devices with, and at the gains it seeds them from.
    void  BindBuildServices (MachineBuildServices & services);

    // The tape's sound follows the recorder, timed by the bus cycle counter.
    void  AttachTape (const TapeDeck * deck, TapeAudioSource::NowFn busCycles);

    // CPU thread: bring the output up, and take it down.
    void  Start (MockingboardCard * mockingboard);
    void  Stop  ();

    // CPU thread, once a frame: reopen a lost or changed endpoint, and
    // re-decode every sound when the device's rate moved.
    void      ServiceEndpointChanges ()       { m_wasapiAudio.ServiceEndpointChanges(); }
    bool      IsOutputUp             () const { return m_wasapiAudio.IsInitialized(); }
    uint32_t  GetSampleRate          () const { return m_wasapiAudio.GetSampleRate(); }
    bool      HasDeviceRateChanged   () const { return m_wasapiAudio.GetSampleRate() != m_audioAssetSampleRate; }
    void      LoadAssetsForDeviceRate (MockingboardCard * mockingboard);

    // CPU thread, once a slice: how many samples it owes, then the slice's
    // speaker toggles mixed with every other source.
    uint32_t  TakeSamplesFor (uint32_t cycles, double cyclesPerSample) { return m_sampleBudget.SamplesFor (cycles, cyclesPerSample); }

    HRESULT   SubmitFrame (const std::vector<uint32_t> & toggleTimestamps,
                           uint32_t                      sliceCycles,
                           float                         speakerState,
                           uint32_t                      numSamples,
                           uint64_t                      totalCycles)
    {
        return m_wasapiAudio.SubmitFrame (toggleTimestamps,
                                          sliceCycles,
                                          speakerState,
                                          numSamples,
                                          &m_driveAudioMixer,
                                          totalCycles,
                                          &m_mockingboardAudioMixer,
                                          &m_tapeAudioMixer);
    }

    // CPU thread: the live drive gains and pans, and a sound auditioned on
    // demand from Settings.
    void  SetDriveVolumes    (float motor, float head, float door);
    void  SetDrivePan        (int drive, float pan);
    void  PlayDriveTestSound (int drive, int kind);

private:
    WasapiAudio                           m_wasapiAudio;

    // Drive audio. Mixer is always allocated; per-drive sources are
    // populated only when the active machine config carries a
    // Disk II controller (FR-015).
    DriveAudioMixer                       m_driveAudioMixer;
    vector<unique_ptr<Disk2AudioSource>>  m_diskAudioSources;

    // Emulated ImageWriter II mechanical audio (Option A: driven by the paced
    // on-screen carriage, not the raw guest stream). A single persistent source
    // on the shared drive-audio bus (FR-016), re-registered by MachineManager on
    // every build. Its grains load once when the output comes up.
    PrinterAudioSource                    m_printerAudio;

    // Mockingboard audio. Its own mixer so the "Mockingboard" Options
    // toggle is independent of the Drive audio toggle. The PSG audio
    // sources are owned by the MockingboardCard device; the mixer holds
    // borrowed pointers, re-registered by MachineManager on every build.
    DriveAudioMixer                       m_mockingboardAudioMixer;
    DriveAudioMixer                       m_tapeAudioMixer;
    TapeAudioSource                       m_tapeAudioSource;

    // Live per-sound drive-audio gains (0..1), seeded from $cassoUiPrefs
    // at startup and updated via SetDriveVolumes. Kept here so they survive
    // machine resets (MachineManager re-seeds fresh sources from these).
    float                                 m_driveMotorVolume = Disk2AudioSource::kMotorVolume;
    float                                 m_driveHeadVolume  = Disk2AudioSource::kHeadVolume;
    float                                 m_driveDoorVolume  = Disk2AudioSource::kDoorVolume;

    // Live per-drive stereo pan in [-1, +1] (-1 = hard left, +1 = hard
    // right), index 0 = Drive 1, 1 = Drive 2. Seeded from $cassoUiPrefs at
    // startup and updated via SetDrivePan; survives machine resets
    // (MachineManager re-seeds fresh sources from these).
    float                                 m_drivePan[2] = { DriveAudioMixer::kDefaultDriveOnePan,
                                                            DriveAudioMixer::kDefaultDriveTwoPan };

    // The fraction of a sample each slice leaves owing, carried into the next
    // so the audio never drifts from the picture. CPU-thread-only.
    AudioSampleBudget                     m_sampleBudget;

    // Host sample rate the loaded sounds were decoded at, 0 before the first
    // load. Compared against the live device rate each frame so a reopen onto
    // a device with a different mix format re-decodes them.
    uint32_t                              m_audioAssetSampleRate = 0;
};

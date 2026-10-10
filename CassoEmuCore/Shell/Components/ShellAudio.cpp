#include "Pch.h"

#include "Shell/Components/ShellAudio.h"
#include "AssetBootstrap.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Shell/MachineBuilder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio
//
////////////////////////////////////////////////////////////////////////////////

ShellAudio::ShellAudio()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~ShellAudio
//
////////////////////////////////////////////////////////////////////////////////

ShellAudio::~ShellAudio() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio::BindBuildServices
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::BindBuildServices (MachineBuildServices & services)
{
    services.diskAudioSources       = &m_diskAudioSources;
    services.driveAudioMixer        = &m_driveAudioMixer;
    services.mockingboardAudioMixer = &m_mockingboardAudioMixer;
    services.printerAudio           = &m_printerAudio;
    services.wasapiAudio            = &m_wasapiAudio;
    services.drivePan               = m_drivePan;
    services.driveMotorVolume       = &m_driveMotorVolume;
    services.driveHeadVolume        = &m_driveHeadVolume;
    services.driveDoorVolume        = &m_driveDoorVolume;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio::AttachTape
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::AttachTape (const TapeDeck * deck, TapeAudioSource::NowFn busCycles)
{
    m_tapeAudioSource.Attach (deck, std::move (busCycles));
    m_tapeAudioMixer.RegisterSource (&m_tapeAudioSource);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio::Start
//
//  CPU-thread-side initialization, run by CpuManager once the worker thread
//  is alive and COM is initialized. Brings up the WASAPI client and seeds the
//  drive-audio mixer with the per-machine sample set so subsequent
//  SetMechanism() switches reload from disk.
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::Start (MockingboardCard * mockingboard)
{
    HRESULT  hr = S_OK;



    // Initialize WASAPI audio (non-fatal if it fails)
    hr = m_wasapiAudio.Initialize();
    IGNORE_RETURN_VALUE (hr, S_OK);

    LoadAssetsForDeviceRate (mockingboard);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio::Stop
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::Stop()
{
    m_wasapiAudio.Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio::LoadAssetsForDeviceRate
//
//  Decodes every sound that has to arrive at the host device's sample rate,
//  and records the rate it decoded at.
//
//  Drive-audio sample loading (spec 005-disk-ii-audio FR-009, NFR-005,
//  FR-019, FR-006). The mixer holds the asset-load context, so any later
//  runtime mechanism switch reloads every registered source through one entry
//  point. Default mechanism is Shugart unless the per-machine registry
//  already overrode it during Initialize.
//
//  The ImageWriter mechanical sound set is the embedded CC BY 4.0 grains that
//  EnsureImageWriterSounds extracted to the asset base, decoded from MP3
//  through the same Media Foundation path as the Disk II WAVs. A missing
//  grain is silent.
//
//  The Mockingboard PSGs are seeded here because the initial machine is built
//  before WASAPI comes up; machine switches after this point pick the rate up
//  at build time in MachineManager.
//
//  ExecuteCpuSlices re-runs this whenever the device rate moves. A change of
//  the default output device can land on a device with a different mix format
//  (GH #137), and grains decoded at the old rate would play at the wrong
//  pitch.
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::LoadAssetsForDeviceRate (MockingboardCard * mockingboard)
{
    HRESULT   hr          = S_OK;
    bool      isAudioUp   = false;
    uint32_t  sampleRate  = 0;
    fs::path  baseDir;
    wstring   devicesDir;
    fs::path  soundsDir;
    HRESULT   hrLoad      = S_OK;
    HRESULT   hrSnd       = S_OK;



    isAudioUp = m_wasapiAudio.IsInitialized();
    BAIL_OUT_IF (!isAudioUp, S_OK);

    sampleRate = m_wasapiAudio.GetSampleRate();

    if (!m_diskAudioSources.empty())
    {
        // Use the same user-writable asset root that Main.cpp /
        // AssetBootstrap used when writing the WAVs so the read
        // path agrees with the write path.
        baseDir     = AssetBootstrap::GetAssetBaseDirectory();
        devicesDir  = (baseDir / L"Devices" / L"DiskII").wstring();

        m_driveAudioMixer.SetSampleLoadContext (devicesDir, sampleRate);

        hrLoad = m_driveAudioMixer.SetMechanism (m_driveAudioMixer.GetMechanism());
        IGNORE_RETURN_VALUE (hrLoad, S_OK);
    }

    soundsDir = AssetBootstrap::GetAssetBaseDirectory() / L"ImageWriter II Sounds";
    hrSnd     = m_printerAudio.LoadSounds (soundsDir.wstring().c_str(), sampleRate);
    IGNORE_RETURN_VALUE (hrSnd, S_OK);

    if (mockingboard != nullptr)
    {
        mockingboard->SetSampleRate (sampleRate);
    }

    m_audioAssetSampleRate = sampleRate;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio::SetDriveVolumes
//
//  Stores the live drive-audio gains and pushes them to every registered
//  Disk2AudioSource. Runs on the CPU thread (the mixing thread), so it is
//  safe to mutate the sources' gains here without synchronization.
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::SetDriveVolumes (float motor, float head, float door)
{
    m_driveMotorVolume = motor;
    m_driveHeadVolume  = head;
    m_driveDoorVolume  = door;

    for (auto & src : m_diskAudioSources)
    {
        src->SetVolumes (motor, head, door);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio::SetDrivePan
//
//  Stores a live per-drive stereo pan and applies it to the matching
//  Disk2AudioSource via equal-power panning. Runs on the CPU thread (the
//  mixing thread), so mutating the source's pan here needs no locking.
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::SetDrivePan (int drive, float pan)
{
    HRESULT  hr   = S_OK;
    float    panL = DriveAudioMixer::kSpeakerCenter;
    float    panR = DriveAudioMixer::kSpeakerCenter;



    BAIL_OUT_IF (drive < 0 || drive >= (int) std::size (m_drivePan), S_OK);

    m_drivePan[drive] = std::clamp (pan, -1.0f, 1.0f);

    DriveAudioMixer::PanToStereo (m_drivePan[drive], panL, panR);

    if (drive < (int) m_diskAudioSources.size())
    {
        m_diskAudioSources[(size_t) drive]->SetPan (panL, panR);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio::PlayDriveTestSound
//
//  Auditions a single drive sound on demand. Runs on the CPU thread (the
//  mixing thread), so triggering the source's test channel is lock-free.
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::PlayDriveTestSound (int drive, int kind)
{
    HRESULT                          hr       = S_OK;
    Disk2AudioSource::TestSoundKind  testKind = Disk2AudioSource::TestSoundKind::Motor;
    bool                             valid    = true;



    BAIL_OUT_IF (drive < 0 || drive >= (int) m_diskAudioSources.size(), S_OK);

    switch (kind)
    {
        case 0:  testKind = Disk2AudioSource::TestSoundKind::Motor; break;
        case 1:  testKind = Disk2AudioSource::TestSoundKind::Head;  break;
        case 2:  testKind = Disk2AudioSource::TestSoundKind::Door;  break;
        default: valid    = false;                                  break;
    }

    BAIL_OUT_IF (!valid, S_OK);

    m_diskAudioSources[(size_t) drive]->PlayTestSound (testKind);

Error:
    return;
}

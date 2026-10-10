#include "Pch.h"

#include "Shell/Components/ShellAudio.h"
#include "AssetBootstrap.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Shell/MachineBuilder.h"
#include "Core/JsonValue.h"
#include "Core/TextEncoding.h"





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
//  ShellAudio::SetDriveAudioVolumes
//
//  Stores the live drive-audio gains and pushes them to every registered
//  Disk2AudioSource. Runs on the CPU thread (the mixing thread), so it is
//  safe to mutate the sources' gains here without synchronization.
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::SetDriveAudioVolumes (float motor, float head, float door)
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
//  ShellAudio::SetDriveAudioPan
//
//  Stores a live per-drive stereo pan and applies it to the matching
//  Disk2AudioSource via equal-power panning. Runs on the CPU thread (the
//  mixing thread), so mutating the source's pan here needs no locking.
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::SetDriveAudioPan (int drive, float pan)
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





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio::SetDriveAudioEnabled
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::SetDriveAudioEnabled (bool enabled)
{
    m_driveAudioMixer.SetEnabled (enabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio::SetDriveAudioMechanism
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellAudio::SetDriveAudioMechanism (const std::wstring & mechanism)
{
    return m_driveAudioMixer.SetMechanism (mechanism);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellAudio::ApplyMachinePrefs
//
//  The drive sounds' switch and mechanism, their gains and pans, and the
//  tape's gain. The gains and pans are pre-seeded to their built-in defaults,
//  and GetNumber leaves a default in place when its key is absent, so the
//  read results are deliberately discarded: absence means keep the default.
//
////////////////////////////////////////////////////////////////////////////////

void ShellAudio::ApplyMachinePrefs (const JsonValue & uiPrefs)
{
    HRESULT      hrOpt      = S_OK;
    bool         enabled    = true;
    std::string  mechNarrow;
    double       motorV     = Disk2AudioSource::kMotorVolume;
    double       headV      = Disk2AudioSource::kHeadVolume;
    double       doorV      = Disk2AudioSource::kDoorVolume;
    double       pan0       = DriveAudioMixer::kDefaultDriveOnePan;
    double       pan1       = DriveAudioMixer::kDefaultDriveTwoPan;
    double       tapeVolume = 1.0;



    hrOpt = uiPrefs.GetBool ("floppySoundEnabled", enabled);
    if (SUCCEEDED (hrOpt))
    {
        m_driveAudioMixer.SetEnabled (enabled);
    }

    hrOpt = uiPrefs.GetString ("floppyMechanism", mechNarrow);
    if (SUCCEEDED (hrOpt) && !mechNarrow.empty())
    {
        // DriveAudioMixer matches mechanism names case-insensitively, so
        // the persisted lower-case token ("alps"/"shugart") can be handed
        // over as-is. A stale/unknown name leaves the mixer on its default
        // mechanism, which is fine -- not worth aborting startup for.
        std::wstring  mechWide = TextEncoding::Utf8ToWide (mechNarrow);

        hrOpt = m_driveAudioMixer.SetMechanism (mechWide);
        IGNORE_RETURN_VALUE (hrOpt, S_OK);
    }

    hrOpt = uiPrefs.GetNumber ("driveMotorVolume", motorV);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    hrOpt = uiPrefs.GetNumber ("driveHeadVolume",  headV);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    hrOpt = uiPrefs.GetNumber ("driveDoorVolume",  doorV);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    SetDriveAudioVolumes ((float) motorV, (float) headV, (float) doorV);

    hrOpt = uiPrefs.GetNumber ("driveOnePan", pan0);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    hrOpt = uiPrefs.GetNumber ("driveTwoPan", pan1);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    SetDriveAudioPan (0, (float) pan0);
    SetDriveAudioPan (1, (float) pan1);

    hrOpt = uiPrefs.GetNumber ("tapeVolume", tapeVolume);
    if (SUCCEEDED (hrOpt))
    {
        SetTapeVolume ((float) tapeVolume);
    }
}

#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Ui/Chrome/DriveLabelTruncation.h"
#include "Print/PrintJobStore.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Ui/PrinterPanel.h"
#include "Core/PathResolver.h"
#include "Version.h"
#include "BuildInfo.h"
#include "resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/FramePacing.h"
#include "Shell/Input/AppleKeyMapping.h"
#include "Shell/Layout/DriveRowLayout.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Core/Prng.h"
#include "Config/DiskSettings.h"
#include "Core/UnicodeSymbols.h"
#include "Core/MachineConfig.h"
#include "Core/JsonParser.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Video/PixelFormat.h"
#include "Video/MonochromeTint.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/DriveWidgetController.h"
#include "Shell/DiskMru.h"
#include "Window/DxuiHwndSource.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyPersistedAudioPrefs
//
//  Seeds the drive-audio mixer, the input mapping, and the //c default
//  pointer from the per-machine $cassoUiPrefs JSON before the audio thread
//  first calls SetEnabled / SetMechanism. Default is enabled + Shugart when
//  nothing is persisted.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyPersistedAudioPrefs()
{
    HRESULT            hr      = S_OK;
    HRESULT            hrOpt   = S_OK;
    JsonValue          doc;
    const JsonValue *  uiPrefs = nullptr;
    bool               enabled = true;
    std::string        mechNarrow;
    double             motorV  = Disk2AudioSource::kMotorVolume;
    double             headV   = Disk2AudioSource::kHeadVolume;
    double             doorV   = Disk2AudioSource::kDoorVolume;
    double             pan0    = DriveAudioMixer::kDefaultDriveOnePan;
    double             pan1    = DriveAudioMixer::kDefaultDriveTwoPan;



    LoadMachineUiPrefs (doc, uiPrefs);

    // The input mapping and the //c pointer default are settled whether or
    // not this machine has a prefs block, so they come BEFORE the bail. A
    // machine with no block is exactly the one that needs the fallback to
    // the pre-1.23 global mapping, and skipping it would leave the mapping
    // wherever the previously-booted machine had left it.
    //
    // Here rather than with the chrome prefs because both read state that is
    // seeded earlier: the mouse device exists by now, and
    // ApplyPersistedChromePrefs has already applied mouseConnected.
    AdoptInputModeForMachine (uiPrefs);
    ApplyDefaultPointerForMachine();
    SyncSelectorState();

    BAIL_OUT_IF (uiPrefs == nullptr, S_OK);

    hrOpt = uiPrefs->GetBool ("floppySoundEnabled", enabled);
    if (SUCCEEDED (hrOpt))
    {
        m_driveAudioMixer.SetEnabled (enabled);
    }

    hrOpt = uiPrefs->GetString ("floppyMechanism", mechNarrow);
    if (SUCCEEDED (hrOpt) && !mechNarrow.empty())
    {
        // DriveAudioMixer matches mechanism names case-insensitively, so
        // the persisted lower-case token ("alps"/"shugart") can be handed
        // over as-is. A stale/unknown name leaves the mixer on its default
        // mechanism, which is fine -- not worth aborting startup for.
        std::wstring  mechWide (mechNarrow.begin(), mechNarrow.end());

        hrOpt = m_driveAudioMixer.SetMechanism (mechWide);
        IGNORE_RETURN_VALUE (hrOpt, S_OK);
    }

    // Optional gains + pans: each value is pre-seeded to its built-in
    // default, and GetNumber leaves that default in place when the key is
    // absent, so the read result is intentionally discarded -- absence
    // simply means "keep the default". SetDriveAudio* then applies the
    // resolved values (default or persisted) uniformly.
    hrOpt = uiPrefs->GetNumber ("driveMotorVolume", motorV);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    hrOpt = uiPrefs->GetNumber ("driveHeadVolume",  headV);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    hrOpt = uiPrefs->GetNumber ("driveDoorVolume",  doorV);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    SetDriveAudioVolumes ((float) motorV, (float) headV, (float) doorV);

    hrOpt = uiPrefs->GetNumber ("driveOnePan", pan0);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    hrOpt = uiPrefs->GetNumber ("driveTwoPan", pan1);
    IGNORE_RETURN_VALUE (hrOpt, S_OK);
    SetDriveAudioPan (0, (float) pan0);
    SetDriveAudioPan (1, (float) pan1);

    // //c case-switch latches: restore the 80/40 and keyboard (Dvorak) switch
    // positions onto the keyboard device. Absent keys leave the hardware
    // default (both out), and only //c configs carry them, so this is a no-op
    // elsewhere. The switch strip re-reads the device on its next
    // SyncSwitchBarState.
    {
        Apple2eKeyboard *  iieKbd = m_machine.GetRefs().iieKeyboard;

        if (iieKbd != nullptr)
        {
            bool  eightyIn = false;
            bool  dvorak   = false;

            if (uiPrefs->HasBool ("eightyColumnSwitch", eightyIn))
            {
                iieKbd->SetEightyColumnSwitchIn (eightyIn);
            }

            if (uiPrefs->HasBool ("keyboardDvorak", dvorak))
            {
                iieKbd->SetKeyboardSwitchDvorak (dvorak);
            }
        }
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCpuThreadStart
//
//  CPU-thread-side initialization callback invoked by CpuManager once
//  the worker thread is alive and COM is initialized. Brings up the
//  WASAPI client and seeds the drive-audio mixer with the per-machine
//  sample set so subsequent SetMechanism() switches reload from disk.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnCpuThreadStart()
{
    HRESULT  hr = S_OK;



    // Initialize WASAPI audio (non-fatal if it fails)
    hr = m_wasapiAudio.Initialize();
    IGNORE_RETURN_VALUE (hr, S_OK);

    LoadAudioAssetsForDeviceRate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadAudioAssetsForDeviceRate
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

void EmulatorShell::LoadAudioAssetsForDeviceRate()
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

    if (m_machine.GetRefs().mockingboard != nullptr)
    {
        m_machine.GetRefs().mockingboard->SetSampleRate (sampleRate);
    }

    m_audioAssetSampleRate = sampleRate;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCpuThreadStop
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnCpuThreadStop()
{
    m_wasapiAudio.Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchCpuCommand
//
//  Invoked by CpuManager once per drained EmulatorCommand, on the CPU
//  thread, where it is safe to touch CPU, bus and device state. The command
//  ids and the payload grammar live in CpuCommandDispatcher; this shell is
//  the target it calls, and the eight small overrides below are the calls
//  that used to be inline in the switch.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::DispatchCpuCommand (const EmulatorCommand & cmd)
{
    CpuCommandDispatcher::Dispatch (cmd, *this);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepInstruction
//
//  The first of the ICpuCommandTarget overrides: each one outcome, over the
//  machine, the disk manager or a mixer.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StepInstruction()
{
    if (m_machine.GetCpu() != nullptr)
    {
        m_machine.StepOne();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemountDisks
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RemountDisks()
{
    m_diskManager->RemountSlot6Disks();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::MountDisk (int drive, const std::string & path)
{
    return m_diskManager->MountDiskInSlot6 (drive, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EjectDisk
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::EjectDisk (int drive)
{
    m_diskManager->EjectDiskInSlot6 (drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleImageWriteProtect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::ToggleImageWriteProtect (int drive)
{
    // On the CPU thread like mount and eject, so the flush never races the
    // drive engine; failures are already reported inside.
    return m_diskManager->ToggleImageWriteProtect (drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolvePendingChange
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ResolvePendingChange (int slot, int drive, int action, const std::string & savePath)
{
    m_machine.GetDiskStore().ResolvePendingChange (slot, drive, (ChangeAction) action, savePath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveAudioEnabled
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDriveAudioEnabled (bool enabled)
{
    m_driveAudioMixer.SetEnabled (enabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveAudioMechanism
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::SetDriveAudioMechanism (const std::wstring & mechanism)
{
    return m_driveAudioMixer.SetMechanism (mechanism);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HoldAppleKeysThroughReset
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::HoldAppleKeysThroughReset (bool openApple, bool closedApple)
{
    if (m_machine.GetRefs().iieKeyboard != nullptr)
    {
        m_machine.GetRefs().iieKeyboard->HoldAppleKeysThroughReset (openApple, closedApple);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestReset
//
//  UI thread. The keys are read here because GetKeyState answers for the
//  calling thread's queue; the CPU thread has no queue to ask.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RequestReset()
{
    bool         foreground  = (GetForegroundWindow() == m_hwnd);
    bool         openApple   = foreground && (GetKeyState (VK_LMENU) & 0x8000) != 0;
    bool         closedApple = foreground && (GetKeyState (VK_RMENU) & 0x8000) != 0;
    std::string  payload;



    if (openApple)
    {
        payload += "open ";
    }

    if (closedApple)
    {
        payload += "closed";
    }

    PostCommand (IDM_MACHINE_RESET, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::PersistSwitchState
//
//  Writes one //c case-switch latch into the current machine's per-machine
//  $cassoUiPrefs block so the position survives across runs. Best-effort: a
//  missing store / machine name, or a write failure, just leaves the on-disk
//  state as it was.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PersistSwitchState (const char * key, bool value)
{
    HRESULT  hr = S_OK;



    if (m_userConfigStore == nullptr || m_machine.GetCurrentMachineName().empty())
    {
        return;
    }

    hr = DiskSettings::WriteSavedUiPrefBool (*m_userConfigStore, m_uiFs, key,
                                             m_machine.GetCurrentMachineName(), value);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveAudioVolumes
//
//  Stores the live drive-audio gains and pushes them to every registered
//  Disk2AudioSource. Runs on the CPU thread (the mixing thread), so it is
//  safe to mutate the sources' gains here without synchronization.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDriveAudioVolumes (float motor, float head, float door)
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
//  SetDriveAudioPan
//
//  Stores a live per-drive stereo pan and applies it to the matching
//  Disk2AudioSource via equal-power panning. Runs on the CPU thread (the
//  mixing thread), so mutating the source's pan here needs no locking.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDriveAudioPan (int drive, float pan)
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
//  PlayDriveTestSound
//
//  Auditions a single drive sound on demand. Runs on the CPU thread (the
//  mixing thread), so triggering the source's test channel is lock-free.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PlayDriveTestSound (int drive, int kind)
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
//  RunOneFrame
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunOneFrame()
{
    ExecuteCpuSlices();
    RenderFramebuffer();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunCpuThreadFrame
//
//  The CPU thread's per-frame callback. Always advances the emulation
//  (ExecuteCpuSlices). Then, at most ~60 Hz (ShouldPublishFrame throttles
//  Maximum speed), it renders + publishes ONLY when the picture can have
//  changed: the bus raised video-dirty (a write into a display page or a
//  banking change), or the video mode / flash phase / color signature moved.
//  A steady screen re-rasterizes nothing -- just a few cheap comparisons.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunCpuThreadFrame()
{
    FrameSignature  current;
    FrameSignature  lastRendered;
    bool            needsRender = false;



    // Emulation always advances; only the publish is throttled and gated.
    ExecuteCpuSlices();

    if (ShouldPublishFrame())
    {
        current.videoDirty = m_machine.GetMemoryBus().IsVideoDirty();
        current.modeSig    = ComputeVideoModeSig();
        current.flashOn    = ComputeFlashOn();
        current.colorSig   = ComputeColorSig();

        lastRendered.modeSig  = m_lastRenderModeSig;
        lastRendered.flashOn  = m_lastRenderFlashOn;
        lastRendered.colorSig = m_lastRenderColorSig;

        needsRender = FramePacing::NeedsRender (current, lastRendered);
    }

    if (needsRender)
    {
        RenderFramebuffer();
        PublishFramebuffer();

        m_machine.GetMemoryBus().ClearVideoDirty();
        m_lastRenderModeSig  = current.modeSig;
        m_lastRenderFlashOn  = current.flashOn;
        m_lastRenderColorSig = current.colorSig;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TickKeyboardAutoRepeat
//
//  Advances the //e keyboard's auto-repeat cadence by the real time since the
//  previous CPU-thread frame.
//
//  The cadence used to be counted in guest cycles, ticked once per
//  instruction. That reads as authentic and is not: the //e's repeat is
//  generated in the keyboard encoder, off an oscillator that knows nothing
//  about the 6502, and a typist's finger rests in real seconds either way. So
//  the guest clock dragged it along -- Double repeated at twice the rate off
//  half the delay, and Maximum, which runs uncapped at tens of times real,
//  turned a held key into hundreds of characters a second and made the
//  machine impossible to type on.
//
//  Once a frame is resolution enough for a 500 ms delay and a 15 cps rate,
//  and the clock read replaces one call per instruction. The stamp is
//  advanced by the whole microseconds handed over rather than set to now, so
//  the sub-microsecond remainder is carried instead of being dropped every
//  frame -- at Maximum speed the frames are short enough for that to matter.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::TickKeyboardAutoRepeat()
{
    uint32_t  elapsed = 0;



    if (m_machine.GetRefs().keyboard == nullptr)
    {
        return;
    }

    // The first frame since the machine came up starts the interval rather
    // than reporting the whole time since the epoch, and a stall is charged
    // once at the device's initial delay: the device caps anything past it at
    // one repeat anyway. Both rules are the clock's, and tested there.
    elapsed = m_frameClock.TakeKeyRepeatElapsedUs (AppleKeyboard::kKeyRepeatDelayUs);

    if (elapsed == 0)
    {
        return;
    }

    m_machine.GetRefs().keyboard->TickAutoRepeat (static_cast<uint32_t> (elapsed));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExecuteCpuSlices
//
//  One emulated frame's worth of CPU time, cut into ~1023-cycle slices.
//
//  The slice exists for audio, not for the CPU. Samples are generated per
//  slice, so the slice length sets how finely speaker toggles are resolved:
//  one slice per frame would quantize the speaker to 60 Hz and turn Apple II
//  square-wave tones into buzzing. A prime-ish length also keeps the slice
//  boundary from landing on the same instruction every frame.
//
//  Device ticking is deliberately NOT per slice. The Disk II engine is
//  advanced by EACH instruction's cycle count, because the boot ROM sits in a
//  tight LDA $C0EC / BPL loop reading the data latch: at slice granularity it
//  would see roughly one valid nibble per thousand cycles instead of one per
//  thirty-two, never accumulate enough sync bytes to match a sector header,
//  and hang forever on a disk that reads fine.
//
//  StepOne is the whole step -- it polls the interrupt lines itself and
//  substitutes an NMI / IRQ vector for the opcode fetch when one is pending,
//  reporting the cost through GetLastInstructionCycles either way. An outer
//  interrupt poll used to wrap this and was simply a second, redundant poll
//  on every instruction.
//
//  The fractional sample the AudioSampleBudget carries is what keeps audio
//  from drifting: cycles per sample is rarely integral, and truncating it
//  every slice would lose a sample every few frames and slowly desync.
//
//  Double speed doubles the cycle target rather than shortening the frame, so
//  the audio and video cadence stay at their real rates and only the guest
//  runs faster.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ExecuteCpuSlices()
{
    static constexpr uint32_t kSliceCycles = 1023;



    HRESULT   hr              = S_OK;
    uint32_t  targetCycles    = m_cyclesPerFrame;
    SpeedMode speed           = m_cpuManager.GetSpeedMode();
    bool      audioActive     = false;
    double    cyclesPerSample = 0.0;
    uint32_t  sliceTarget     = 0;
    uint32_t  sliceActual     = 0;
    uint32_t  numSamples      = 0;



    // Service the endpoint before anything reads it. A lost endpoint and a
    // change of the default output device are both torn down and reopened
    // here, on the one thread allowed to do it. This runs ahead of the
    // audioActive gate rather than inside SubmitFrame: a teardown clears
    // IsInitialized, so the gate below would otherwise keep the reopen from
    // ever running (GH #137).
    m_wasapiAudio.ServiceEndpointChanges();

    // Real time, not the cycle budget below: the keyboard's repeat cadence is
    // the one thing in this frame that must not follow the emulated clock.
    TickKeyboardAutoRepeat();

    audioActive = (m_machine.GetRefs().speaker != nullptr && m_wasapiAudio.IsInitialized());

    // A reopen can land on a device with a different mix format, and every
    // drive, printer and PSG sound was decoded to the rate of the device that
    // is gone. Re-decode before this frame's samples are generated.
    if (audioActive && m_wasapiAudio.GetSampleRate() != m_audioAssetSampleRate)
    {
        LoadAudioAssetsForDeviceRate();
    }

    if (speed == SpeedMode::Double)
    {
        targetCycles *= 2;
    }

    if (audioActive)
    {
        cyclesPerSample = static_cast<double> (m_machine.GetConfig().clockSpeed) /
                          static_cast<double> (m_wasapiAudio.GetSampleRate());
        m_machine.GetRefs().speaker->BeginFrame();
    }

    for (uint32_t executed = 0; executed < targetCycles; )
    {
        sliceTarget = targetCycles - executed;

        if (sliceTarget > kSliceCycles)
        {
            sliceTarget = kSliceCycles;
        }

        // Feed the next paste character if available; the slice budget is
        // the guest-time currency the settle pacing is measured in.
        m_clipboardManager->DrainPasteBuffer (sliceTarget);

        sliceActual = static_cast<uint32_t> (m_machine.RunCycles (sliceTarget));

        // No CPU (a rebuild failed after teardown): nothing advances, so
        // leave rather than spin on a budget no one can consume.
        if (sliceActual == 0)
        {
            break;
        }

        executed += sliceActual;

        // The Apple keys held through a reset are held for a count of
        // emulated cycles, the firmware's own timeline.
        if (m_machine.GetRefs().iieKeyboard != nullptr)
        {
            m_machine.GetRefs().iieKeyboard->TickResetHold (sliceActual);
        }

        if (audioActive)
        {
            numSamples = m_sampleBudget.SamplesFor (sliceActual, cyclesPerSample);

            hr = m_wasapiAudio.SubmitFrame (m_machine.GetRefs().speaker->GetToggleTimestamps(),
                                            sliceActual,
                                            m_machine.GetRefs().speaker->GetFrameInitialState(),
                                            numSamples,
                                            &m_driveAudioMixer,
                                            m_machine.GetCpu()->GetTotalCycles(),
                                            &m_mockingboardAudioMixer);
            IGNORE_RETURN_VALUE (hr, S_OK);

            m_machine.GetRefs().speaker->ClearTimestamps();
            m_machine.GetRefs().speaker->BeginFrame();
        }
    }
}

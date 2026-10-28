#include "Pch.h"
#include "Shell/Components/ShellInput.h"

#include "Devices/Tape/TapeTurboGovernor.h"

#include "Shell/EmulatorShell.h"
#include "Shell/ShellDebugger.h"
#include "Shell/ClipboardManager.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/DiskManager.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Shell/TapeManager.h"
#include "Shell/Components/ShellAudio.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Debugger/CpuManagerRunDriver.h"
#include "Debugger/DebugSession.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Core/TextEncoding.h"
#include "Debugger/Channel/PipeSecurity.h"
#include "Debugger/Channel/Win32NamedPipeApi.h"
#include "Debugger/Channel/Win32PipeTransport.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Print/PrintJobStore.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Machines/Apple2/Common/SscSwitches.h"
#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "Machines/Apple2/Common/UthernetCard.h"
#include "Shell/Components/ShellChrome.h"
#include "Core/TextEncoding.h"
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
#include "Shell/FrameCycleBudget.h"
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
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Core/TextEncoding.h"
#include "Shell/DiskManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyPersistedMachinePrefs
//
//  Seeds the input mapping, the //c default pointer, the drive sounds, the
//  tape settings and the //c case switches from the per-machine
//  $cassoUiPrefs JSON, before the audio thread first opens the output.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyPersistedMachinePrefs()
{
    HRESULT            hr      = S_OK;
    JsonValue          doc;
    const JsonValue  * uiPrefs = nullptr;



    m_settings->LoadMachineUiPrefs (doc, uiPrefs);

    // The input mapping and the //c pointer default are settled whether or
    // not this machine has a prefs block, so they come BEFORE the bail. A
    // machine with no block is exactly the one that needs the fallback to
    // the pre-1.23 global mapping, and skipping it would leave the mapping
    // wherever the previously-booted machine had left it.
    //
    // Here rather than with the chrome prefs because both read state that is
    // seeded earlier: the mouse device exists by now, and
    // ApplyPersistedChromePrefs has already applied mouseConnected.
    m_input->AdoptInputModeForMachine (uiPrefs, m_machine.GetConfig().machineId);
    m_input->ApplyDefaultPointerForMachine();
    m_input->GetControllers().SyncPaddleSourceList();

    BAIL_OUT_IF (uiPrefs == nullptr, S_OK);

    m_audio->ApplyMachinePrefs (*uiPrefs);
    m_tapeDeck->ApplyMachinePrefs (*uiPrefs);

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
//  audio output, which has to open on this thread.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnCpuThreadStart()
{
    HRESULT  hr = S_OK;



    // The disk store's token moves to this thread here and back to the window
    // thread at OnCpuThreadStop. The window thread calls ReleaseDiskOwnership
    // just before the start.
    m_machine.ClaimDiskOwnership();

    m_audio->Start (m_machine.GetRefs().mockingboard);

    // Reverse execution records from here, on the thread that runs the
    // machine, which is built and power cycled by now. The settings are read
    // once, here, and hold for every machine built after.
    m_debugger->LoadReverseSettings();
    m_debugger->StartReverseRecording();

    // On the CPU thread, which is where the debugger lives from here on.
    if (m_openDebuggerAtStart)
    {
        hr = m_debugger->OpenDebugger();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCpuThreadStop
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnCpuThreadStop()
{
    // First, so the disks' held writes reach their files at shutdown.
    m_debugger->StopReverseRecording();

    m_debugger->CloseDebugger();
    m_audio->Stop();

    // Last, so the shutdown flush on the window thread can claim the disk
    // store's token once this thread has exited.
    m_machine.ReleaseDiskOwnership();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchCpuCommand
//
//  Invoked by CpuManager once per drained EmulatorCommand, on the CPU
//  thread, where it is safe to touch CPU, bus and device state. The command
//  ids and the payload grammar live in CpuCommandDispatcher. The shell takes
//  the machine-wide commands, and the drives, the drive sounds and the
//  recorder take their own.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::DispatchCpuCommand (const EmulatorCommand & cmd)
{
    InputRecord  input;



    // Journaled before it is applied, so the stamp is the boundary the
    // machine was at when the reset, power cycle or disk change landed.
    if (m_machine.GetInputJournal().IsOn() && CpuCommandDispatcher::TryGetJournalInput (cmd, input))
    {
        m_machine.RecordInput (input.kind, input.value, input.detail, input.payload);
    }

    CpuCommandDispatcher::Dispatch (cmd, { *this, *m_disks, *m_audio, *m_tapeDeck, *m_debugger });
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepInstruction
//
//  The first of the ICpuMachineCommands overrides: each one outcome, over the
//  machine.
//
//  With the debugger attached this is the debugger's own step into, so the
//  session starts it, the frame loop runs it as it runs any other step, and
//  the stop is announced and drawn like one. Without it, one instruction runs
//  here and the screen is redrawn, since a paused machine runs no frame that
//  would draw it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StepInstruction()
{
    if (m_debugger->StepSession())
    {
        return;
    }

    if (m_machine.GetCpu() == nullptr)
    {
        return;
    }

    m_machine.SampleHostInputs();
    m_machine.StepOne();

    m_renderer->RenderFramebuffer();
    m_renderer->PublishFramebuffer();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigureNetworkCard
//
//  LAN access and port forwards apply to the running card; a forward whose
//  host port cannot be opened is reported as a notice.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ConfigureNetworkCard (const NetworkCardSettings & settings)
{
    UthernetCard *  card = m_machine.GetRefs().uthernet;



    if (card == nullptr)
    {
        return;
    }

    card->ApplySettings (settings);

    for (const std::string & error : card->GetNetworkErrors())
    {
        m_chrome->PostNotice (TextEncoding::NarrowToWide (error));
    }
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
//  SetSerialCardSwitches
//
//  On the CPU thread, where the card reads its switches. A bank that does not
//  parse does not change the card's current positions.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetSerialCardSwitches (const std::string & sw1, const std::string & sw2)
{
    SuperSerialCard  * card     = m_machine.GetRefs().superSerialCard;
    SscSwitches        switches;
    bool               parsed   = false;



    if (card != nullptr)
    {
        switches = card->GetSwitches();

        parsed = switches.TryParseBankText (SscBank::Sw1, sw1);
        IGNORE_RETURN_VALUE (parsed, false);
        parsed = switches.TryParseBankText (SscBank::Sw2, sw2);
        IGNORE_RETURN_VALUE (parsed, false);

        card->SetSwitches (switches);
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
//  ApplyTapeTurbo
//
//  Runs a tape load at Maximum speed while the governor returns true, and
//  restores the user's own speed as soon as it returns false. Only a change is
//  written.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyTapeTurbo()
{
    uint64_t  now    = *m_machine.GetCpu()->GetBusCyclePtr();
    bool      isFast = TapeTurboGovernor::ShouldRunAtMaximum (m_tapeDeck->IsFastTapeLoading(),
                                                              m_machine.GetTapeDeck(),
                                                              now,
                                                              (double) m_machine.GetConfig().clockSpeed);



    // Not silenced. At Maximum speed the audio path keeps only what fits in
    // real time and drops the rest, faded at each cut, so the load plays as
    // slices of the tape at its true pitch -- the data's rasp, not a chirp.
    if (isFast != m_cpuManager.IsMaximumOverride())
    {
        m_cpuManager.SetMaximumOverride (isFast);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunOneFrame
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunOneFrame()
{
    ExecuteCpuSlices();
    m_renderer->RenderFramebuffer();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServiceCpuThread
//
//  The CPU thread's service pass, on every pass through its loop, paused or
//  running: the debug channel, the history thumbnails, and then the drive
//  status, with activity due on every pass while paused.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ServiceCpuThread()
{
    m_debugger->ServiceDebugger();
    m_debugger->ServiceHistoryThumbnails();

    (void) m_disks->GetManager()->PublishDriveStatus (m_cpuManager.IsPaused());
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
    bool  isPublishDue = false;



    // Recording pauses at a Maximum speed the user chose, and running the
    // machine from the past makes it live again.
    m_debugger->RecordHistoryFrame (m_cpuManager.IsUserMaximumSpeed());

    // Emulation always advances; only the publish is throttled and gated.
    ExecuteCpuSlices();

    isPublishDue = m_renderer->PublishFrameIfChanged();

    // The drives' activity publishes at the picture's cadence; a bay change
    // made in the frame publishes regardless.
    (void) m_disks->GetManager()->PublishDriveStatus (isPublishDue);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunWatchedSlice
//
//  One slice. Behind live, with input held back, the devices report a read
//  the held input would change for this slice only, so neither a reverse
//  command's replay nor a debugger's look at the machine between slices can
//  count as the guest reading it.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t EmulatorShell::RunWatchedSlice (uint32_t sliceTarget, const DebuggerSliceHooks & hooks)
{
    bool      isWatching = hooks.isBehindLive && hooks.heldInputWatch->Refresh();
    uint32_t  actual     = 0;



    if (isWatching)
    {
        m_machine.SetHeldInputWatch (hooks.heldInputWatch);
    }

    actual = static_cast<uint32_t> (m_machine.RunCycles (sliceTarget));

    if (isWatching)
    {
        m_machine.SetHeldInputWatch (nullptr);
    }

    return actual;
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
//  The target runs the cycle count to the next frame boundary rather than a
//  fixed frame's worth past wherever the last pass ended. FrameCycleBudget
//  says why: the last instruction's overshoot is repaid, and a pass a stop
//  cut short is finished rather than restarted.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ExecuteCpuSlices()
{
    HRESULT                   hr              = S_OK;
    uint32_t                  nominalCycles   = m_cyclesPerFrame;
    uint32_t                  targetCycles    = 0;
    uint32_t                  executed        = 0;
    SpeedMode                 speed           = m_cpuManager.GetEffectiveSpeedMode();
    bool                      audioActive     = false;
    bool                      isSilent        = false;
    double                    cyclesPerSample = 0.0;
    uint32_t                  sliceTarget     = 0;
    uint32_t                  sliceActual     = 0;
    uint32_t                  numSamples      = 0;
    Byte                      pasted          = 0;
    bool                      isPauseLanding  = false;
    bool                      hasRunEnded     = false;
    const DebuggerSliceHooks  hooks           = m_debugger->GetSliceHooks();   // the slices touch the debugger only through these
    const bool                isBehindLive    = hooks.isBehindLive;



    // Service the endpoint before anything reads it. A lost endpoint and a
    // change of the default output device are both torn down and reopened
    // here, on the one thread allowed to do it. This runs ahead of the
    // audioActive gate rather than inside SubmitFrame: a teardown clears
    // IsInitialized, so the gate below would otherwise keep the reopen from
    // ever running (GH #137).
    m_audio->ServiceEndpointChanges();

    // Real time, not the cycle budget below: the keyboard's repeat cadence is
    // the one thing in this frame that must not follow the emulated clock.
    // Behind live the keyboard holds a recorded position, whose repeats the
    // replay applies from the journal.
    if (!isBehindLive)
    {
        m_input->TickKeyboardAutoRepeat();
    }

    audioActive = (m_machine.GetRefs().speaker != nullptr && m_audio->IsOutputUp());

    // A reopen can land on a device with a different mix format, and every
    // drive, printer and PSG sound was decoded to the rate of the device that
    // is gone. Re-decode before this frame's samples are generated.
    if (audioActive && m_audio->HasDeviceRateChanged())
    {
        m_audio->LoadAssetsForDeviceRate (m_machine.GetRefs().mockingboard);
    }

    if (speed == SpeedMode::Double)
    {
        nominalCycles *= 2;
    }

    if (m_machine.GetCpu() != nullptr)
    {
        targetCycles = FrameCycleBudget::GetTarget (nominalCycles, m_machine.GetCpu()->GetTotalCycles());

        // A pending pause shortens this pass to the share of the frame that
        // matches how far through the host tick it was asked for.
        isPauseLanding = hooks.runDriver != nullptr && hooks.runDriver->IsPausePending();

        if (isPauseLanding)
        {
            targetCycles = FrameCycleBudget::GetPauseTarget (nominalCycles,
                                                             m_machine.GetCpu()->GetTotalCycles(),
                                                             hooks.runDriver->GetPauseFraction());
        }
    }

    if (audioActive)
    {
        cyclesPerSample = static_cast<double> (m_machine.GetConfig().clockSpeed) /
                          static_cast<double> (m_audio->GetSampleRate());
        m_machine.GetRefs().speaker->BeginFrame();
    }

    while (executed < targetCycles)
    {
        // A pause asked for while this pass runs ends it here, on the
        // instruction boundary RunCycles stopped the last slice at, rather
        // than at the end of the frame. The pass after the resume runs the
        // rest of the frame.
        if (m_cpuManager.IsPaused())
        {
            break;
        }

        sliceTarget = targetCycles - executed;

        if (sliceTarget > FrameCycleBudget::kSliceCycles)
        {
            sliceTarget = FrameCycleBudget::kSliceCycles;
        }

        if (hooks.runDriver != nullptr)
        {
            sliceTarget = hooks.runDriver->LimitSlice (sliceTarget);
        }

        // Feed the next paste character if available; the slice budget is
        // the guest-time currency the settle pacing is measured in.
        pasted = isBehindLive ? 0 : m_clipboardManager->DrainPasteBuffer (sliceTarget);

        if (pasted != 0)
        {
            m_machine.RecordInput (InputKind::PasteChar, pasted, 0, {});
        }

        // Recording history: journal what the UI thread wrote since the last
        // slice, here where the machine first holds it. Off, one bool test.
        m_machine.SampleHostInputs();

        sliceActual = RunWatchedSlice (sliceTarget, hooks);
        executed   += sliceActual;

        // Behind live, the slice ended after a read that input held back
        // would change: the machine stops there and the user is asked.
        if (hooks.heldInputWatch->HasHit())
        {
            m_debugger->StopForHeldInputRead();
            hasRunEnded = true;
            break;
        }

        // A debugger run ends HERE, on the thread that ran it, because this is
        // where a stop becomes observable: the hook stopped the CPU partway
        // through the slice above, which is why RunCycles came back short.
        // Carrying on with the rest of the frame would run the guest past the
        // instruction the client was told it stopped on.
        //
        // AHEAD OF THE ZERO TEST BELOW: a stop before the slice's first
        // instruction runs nothing, and a run told nothing of it never ends.
        // The machine then stays running at the breakpoint.
        //
        // A debugger step is silent: whether this slice belonged to one is
        // read before the run can end, and its speaker toggles are dropped
        // rather than played, here or with the next run's first slice.
        isSilent = hooks.runDriver != nullptr && hooks.runDriver->IsSilent();

        if (audioActive && isSilent)
        {
            m_machine.GetRefs().speaker->ClearTimestamps();
            m_machine.GetRefs().speaker->BeginFrame();
        }

        if (hooks.runDriver != nullptr && hooks.runDriver->OnSliceExecuted (sliceActual))
        {
            hasRunEnded = true;
            break;
        }

        // No CPU (a rebuild failed after teardown): nothing advances, so
        // leave rather than spin on a budget no one can consume.
        if (sliceActual == 0)
        {
            break;
        }

        // The Apple keys held through a reset are held for a count of
        // emulated cycles, the firmware's own timeline.
        if (m_machine.GetRefs().iieKeyboard != nullptr)
        {
            m_machine.GetRefs().iieKeyboard->TickResetHold (sliceActual);
        }

        // The recorder stops itself at the end of the tape and refreshes what
        // the deck shows, on emulated time like everything else here.
        m_machine.GetTapeDeck().Update (*m_machine.GetCpu()->GetBusCyclePtr());
        ApplyTapeTurbo();

        if (m_machine.GetTapeDeck().HasPendingRecording())
        {
            hr = m_tapeDeck->GetManager()->CommitPendingRecording (m_machine.GetTapeDeck());
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (audioActive && !isSilent)
        {
            numSamples = m_audio->TakeSamplesFor (sliceActual, cyclesPerSample);

            hr = m_audio->SubmitFrame (m_machine.GetRefs().speaker->GetToggleTimestamps(),
                                       sliceActual,
                                       m_machine.GetRefs().speaker->GetFrameInitialState(),
                                       numSamples,
                                       m_machine.GetCpu()->GetTotalCycles());
            IGNORE_RETURN_VALUE (hr, S_OK);

            m_machine.GetRefs().speaker->ClearTimestamps();
            m_machine.GetRefs().speaker->BeginFrame();
        }
    }

    // The shortened pass reached its landing point. A stop on the way, such
    // as a breakpoint, ended the pass and the pause along with it.
    if (isPauseLanding && !hasRunEnded)
    {
        hooks.runDriver->OnPausePointReached (executed);
    }
}

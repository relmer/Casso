#include "Pch.h"

#include "Shell/MachineHost.h"
#include "Devices/Floppy35/Apple2cPlusDiskPort.h"
#include "Devices/Floppy35/Apple2cSmartPort.h"
#include "Machines/Apple2/Apple2cPlus/Apple2cPlusAccelerator.h"
#include "Devices/Floppy35/Apple35ControllerCard.h"
#include "Devices/Floppy35/LironCard.h"
#include "CassoLink/RunningCassoRegistry.h"

#include "Core/Prng.h"
#include "Core/StateHash.h"
#include "Shell/MachineHardwareIdentity.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DebugHook.h"
#include "Debugger/Reverse/HeldInputWatch.h"
#include "Debugger/Reverse/HistoryRecorder.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/RomDevice.h"
#include "Devices/RomGeneration.h"
#include "Audio/IDriveAudioSink.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/BlockDevice/IBlockDeviceEventSink.h"
#include "Devices/BlockDevice/ProFileCard.h"
#include "Devices/BlockDevice/ScsiHardDisk.h"
#include "Shell/StorageCardWiring.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Seams/Win32DiskFileIo.h"
#include "Shell/DiskManager.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"
#include "Devices/RamDevice.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/VideoScanner.h"
#include "Devices/Acia6551.h"
#include "Devices/AciaByteSourceEndpoint.h"
#include "Devices/AciaEndpoints.h"
#include "Devices/SerialPortDevice.h"
#include "Devices/SerialTrafficLog.h"
#include "Seams/IByteStreamFactory.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Shell/MachineBuilder.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"
#include "Machines/Apple2/Common/CharacterRomData.h"
#include "Devices/IAciaEndpoint.h"
#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "Machines/Apple2/Common/UthernetCard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::MachineHost
//
////////////////////////////////////////////////////////////////////////////////

MachineHost::MachineHost() :
    m_memoryBus (std::make_unique<MemoryBus>()),
    m_charRom   (std::make_unique<CharacterRomData>()),
    m_diskStore (std::make_unique<DiskImageStore>()),
    m_config    (std::make_unique<MachineConfig>()),
    m_tapeDeck  (std::make_unique<TapeDeck>()),
    m_printerRing (std::make_unique<PrinterByteRing>()),
    m_connectors  (m_refs, *m_printerRing, *m_config),
    m_stateStream (*this, *m_diskStore, *m_config, m_currentMachineName)
{
    // A disk leaving a bay is stamped with the position, and a disk change is
    // a boundary in reverse execution's history.
    m_diskStore->SetPositionSource (&m_position);
    m_diskStore->SetMediaChangeListener ([this] () { OnMediaChanged(); });

    //  The store frees an image on an eject or a remount, so the controller
    //  is re-pointed on every bay change rather than by each caller. After a
    //  restored state, the bay has the same image and the controller has its
    //  restored state.
    m_diskStore->SetBayChangeSink ([this] (int slot, int drive, BayChange change)
    {
        if (change != BayChange::Restored)
        {
            ConnectDiskToController (slot, drive);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::OnMediaChanged
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::OnMediaChanged()
{
    if (m_historyRecorder != nullptr)
    {
        m_historyRecorder->OnMediaChanged (*this);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::NoteDebuggerEdit
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::NoteDebuggerEdit()
{
    if (m_historyRecorder != nullptr)
    {
        m_historyRecorder->OnMachineEdited (*this);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetOutputMuted
//
//  Sound and the printer together.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetOutputMuted (bool isMuted)
{
    SetOutputMuted (isMuted, isMuted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetOutputMuted
//
//  The speaker and the printer card hold their own flag. The Disk II's
//  drive sounds go out through its audio sink, which is held here and
//  detached while muted. The serial chips are muted with the printer, since
//  what they send goes to a file or another machine. Muting twice, or
//  unmuting twice, changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetOutputMuted (bool isSoundMuted, bool isPrinterMuted)
{
    for (PrinterCard * card : m_connectors.GetParallelCards())
    {
        if (card != nullptr)
        {
            card->SetMuted (isPrinterMuted);
        }
    }

    m_isPrinterMuted = isPrinterMuted;

    for (Acia6551 * acia : m_refs.serialChips)
    {
        acia->SetOutputMuted (isPrinterMuted);
    }

    if (isSoundMuted == m_isSoundMuted)
    {
        return;
    }

    m_isSoundMuted = isSoundMuted;

    if (m_refs.speaker != nullptr)
    {
        m_refs.speaker->SetMuted (isSoundMuted);
    }

    if (m_refs.diskController == nullptr)
    {
        return;
    }

    if (isSoundMuted)
    {
        m_mutedDiskAudio   = m_refs.diskController->GetAudioSink();
        m_wasMotorOnAtMute = m_refs.diskController->IsMotorOn();
        m_refs.diskController->SetAudioSink (nullptr);
        return;
    }

    m_refs.diskController->SetAudioSink (m_mutedDiskAudio);

    // The motor's hum follows the drive's state, so a motor that started or
    // stopped while muted is told to the sink now.
    if (m_mutedDiskAudio != nullptr && m_refs.diskController->IsMotorOn() != m_wasMotorOnAtMute)
    {
        if (m_wasMotorOnAtMute)
        {
            m_mutedDiskAudio->OnMotorDisengaged();
        }
        else
        {
            m_mutedDiskAudio->OnMotorEngaged();
        }
    }

    m_mutedDiskAudio = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::~MachineHost
//
//  Out of line so the header can forward-declare the types it holds by
//  unique_ptr rather than including all of them.
//
////////////////////////////////////////////////////////////////////////////////

MachineHost::~MachineHost()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetBlockMediaFileIo
//
//  Null restores the host's file system and its write-protect probe.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetBlockMediaFileIo (IDiskFileIo * fileIo, WriteProtectProbe probe)
{
    m_blockFileIo = fileIo;
    m_blockProbe  = std::move (probe);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::GetBlockMediaFileIo
//
////////////////////////////////////////////////////////////////////////////////

IDiskFileIo & MachineHost::GetBlockMediaFileIo()
{
    if (m_blockFileIo != nullptr)
    {
        return *m_blockFileIo;
    }

    if (m_hostBlockFileIo == nullptr)
    {
        m_hostBlockFileIo = std::make_unique<Win32DiskFileIo>();
    }

    return *m_hostBlockFileIo;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::ProbeBlockMedia
//
//  The host file's read-only attribute and write permission, for the host's
//  file system; nothing for an in-memory one without a probe.
//
////////////////////////////////////////////////////////////////////////////////

WriteProtectInfo MachineHost::ProbeBlockMedia (const std::string & path) const
{
    WriteProtectInfo  info;



    if (m_blockProbe)
    {
        return m_blockProbe (path);
    }

    if (m_blockFileIo == nullptr)
    {
        DiskManager::ProbeFileWritability (path, info.readOnlyFile, info.noPermission);
    }

    return info;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetCpu
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetCpu (std::unique_ptr<EmuCpu> cpu)
{
    m_cpu = std::move (cpu);

    if (m_cpu != nullptr)
    {
        m_cpu->GetCpu6502()->SetOpcodeWatch (m_watchOpcodes, m_watcher);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetPrng
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetPrng (std::unique_ptr<Prng> prng)
{
    m_prng = std::move (prng);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetMmu
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetMmu (std::unique_ptr<Apple2eMmu> mmu)
{
    m_mmu = std::move (mmu);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetApple2cRomBank
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetApple2cRomBank (std::unique_ptr<Apple2cRomBank> romBank)
{
    m_apple2cRomBank = std::move (romBank);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetMouse
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetMouse (std::unique_ptr<AppleMouse> mouse)
{
    m_mouse = std::move (mouse);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetAccelerator
//
//  The memory bus watches the soft switches for the accelerator, so the
//  bus is pointed at the new one before the old one is deleted.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetAccelerator (std::unique_ptr<Apple2cPlusAccelerator> accelerator)
{
    m_memoryBus->SetIoObserver (accelerator.get());
    m_accelerator = std::move (accelerator);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetJoyport
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetJoyport (std::unique_ptr<SiriusJoyport> joyport)
{
    m_joyport = std::move (joyport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetVideoTiming
//
//  Before the machine is built: the build points the video scanner, the
//  keyboard, the soft switches and the mouse at this timing, so one set
//  after the build frees the timing they read.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetVideoTiming (std::unique_ptr<VideoTiming> videoTiming)
{
    m_videoTiming = std::move (videoTiming);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::GetPendingPrintDir
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path MachineHost::GetPendingPrintDir() const
{
    std::filesystem::path  dir = std::filesystem::path (m_assetBaseDir) / L"Machines" /
                                 std::filesystem::path (m_currentMachineName) / L"PendingPrint";



    return (dir);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::StepOne
//
//  One instruction, and the devices that measure time in instructions.
//
//  The Disk ][ nibble engine is pumped per instruction rather than per slice
//  because the boot ROM sits in a tight LDA $C0EC / BPL loop reading the data
//  latch: advance the engine only at slice boundaries and the CPU sees one
//  valid nibble per ~1000 cycles instead of ~32, and never accumulates enough
//  sync bytes to find a sector header.
//
//  AddCycles is what moves the machine's clock, and through it the //e video
//  timing and the //c mouse. It belongs here, with the instruction that spent
//  the cycles, rather than at each call site -- a single-step that ticked the
//  disk controller but left the clock standing was the shape of the defect
//  this consolidates away.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineHost::StepOne()
{
    if (m_cpu == nullptr)
    {
        return (0);
    }

    if (m_debugHook != nullptr)
    {
        return StepOneWithHook();
    }

    // StepOne polls the interrupt lines itself and dispatches a pending
    // NMI/IRQ vector in place of the opcode fetch, reporting the cost through
    // GetLastInstructionCycles either way -- so a bare StepOne is the whole
    // step, and a separate interrupt poll would be a second, redundant one.
    if (m_historyRecorder != nullptr)
    {
        m_historyRecorder->OnInstructionStart (*m_cpu->GetCpu6502());
    }

    m_cpu->StepOne();

    return FinishStep();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::StepOneWithHook
//
//  StepOne while a debug hook is installed. The hook is asked before the
//  instructions on the pages its filter marks (StepOneAsked); the rest run
//  without it. This runs once per instruction, so the test is one load.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineHost::StepOneWithHook()
{
    const DebugHookFilter  & filter = m_debugHook->GetFilter();
    Word                     pc     = m_cpu->GetPC();



    if (filter.pages[pc >> 8])
    {
        return StepOneAsked (filter, pc);
    }

    if (m_historyRecorder != nullptr)
    {
        m_historyRecorder->OnInstructionStart (*m_cpu->GetCpu6502());
    }

    m_cpu->StepOne();
    return FinishStep();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::StepOneAsked
//
//  An instruction on a page the filter marks, which the hook may stop before.
//  While only an opcode can stop the machine, only an instruction with that
//  opcode, or with one that cannot be read without side effects, is asked
//  about. The opcode is read from the shadow read page, which is what the CPU
//  is about to fetch; I/O and a device's ROM have none there.
//
////////////////////////////////////////////////////////////////////////////////

__declspec (noinline) Byte MachineHost::StepOneAsked (const DebugHookFilter & filter, Word pc)
{
    static constexpr Word  kPageMask = 0xFF;
    const Byte           * page      = m_memoryBus->GetShadowReadPage (pc);
    bool                   isAsked   = !filter.opcodesStop || filter.everyInstruction || page == nullptr || filter.opcodes[page[pc & kPageMask]];



    if (isAsked && m_debugHook->ShouldStopBefore (pc))
    {
        return (0);
    }

    if (m_historyRecorder != nullptr)
    {
        m_historyRecorder->OnInstructionStart (*m_cpu->GetCpu6502());
    }

    m_cpu->StepOne();
    return FinishStep();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::RecordInput
//
//  Stamps a host input with the CPU's cycle count. Called on the CPU thread
//  at an instruction boundary, before or in the same breath as the input is
//  applied, so the stamp is the point the guest first sees it. A machine with
//  no CPU yet stamps zero.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::RecordInput (
    InputKind         kind,
    Byte              value,
    uint16_t          detail,
    std::string_view  payload)
{
    uint64_t  cycle = 0;



    if (!m_inputJournal.IsOn())
    {
        return;
    }

    if (m_cpu != nullptr)
    {
        cycle = m_cpu->GetTotalCycles();
    }

    m_inputJournal.Record (cycle, kind, value, detail, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetInputJournalOn
//
//  Turns the journal on or off and points the devices at it, or at nothing,
//  so a device whose reads see host input pays one null test while it is
//  off. CPU thread, between instructions.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetInputJournalOn (bool isOn)
{
    m_inputJournal.SetOn (isOn);
    AttachInputJournal();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::AttachInputJournal
//
//  Hands the journal to every device that records the host input its reads
//  see, or null when the journal is off. A machine switch builds new
//  devices, so the builder calls this again once they are wired. The serial
//  chips also get the host input gate, checked before an attachment's bytes
//  are taken, so that behind live they wait.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::AttachInputJournal()
{
    InputJournal  * journal = m_inputJournal.IsOn() ? &m_inputJournal : nullptr;
    size_t          i       = 0;



    m_inputJournal.SetCycleSource    ((m_cpu != nullptr) ? m_cpu->GetCycleCounterPtr() : nullptr);
    m_inputJournal.SetCycleBase      ((m_cpu != nullptr) ? m_cpu->GetCycleBasePtr() : nullptr);
    m_inputJournal.SetPositionSource (&m_position);

    if (m_refs.keyboard != nullptr)
    {
        m_refs.keyboard->SetInputJournal (journal);
    }

    if (m_refs.gamePort != nullptr)
    {
        m_refs.gamePort->SetInputJournal (journal);
    }

    if (m_refs.iieSoftSwitches != nullptr)
    {
        m_refs.iieSoftSwitches->SetInputJournal (journal);
    }

    if (m_mouse != nullptr)
    {
        m_mouse->SetInputJournal (journal);
    }

    if (m_joyport != nullptr)
    {
        m_joyport->SetInputJournal (journal);
    }

    for (i = 0; i < m_refs.serialChips.size(); i++)
    {
        m_refs.serialChips[i]->SetInputJournal  (journal, (uint16_t) i);
        m_refs.serialChips[i]->SetHostInputGate (&m_hostInputGate);
    }

    if (m_refs.uthernet != nullptr)
    {
        m_refs.uthernet->SetInputJournal  (journal);
        m_refs.uthernet->SetHostInputGate (&m_hostInputGate);
    }

    AttachCommEventSink (m_commSink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::ApplyDeviceInput
//
//  Replay: hands one journal record a device made at a read back to that
//  device, which stores the value so the same read returns the same result.
//  The caller applies each record just before the instruction that begins at
//  its cycle. Returns false for a record no device here holds -- a reset,
//  power cycle or disk change, which the replayer dispatches as a command.
//
////////////////////////////////////////////////////////////////////////////////

bool MachineHost::ApplyDeviceInput (const InputRecord & record)
{
    bool  isApplied = false;



    if (m_refs.keyboard != nullptr)
    {
        isApplied = m_refs.keyboard->ApplyInput (record);
    }

    if (!isApplied && m_refs.iieSoftSwitches != nullptr)
    {
        isApplied = m_refs.iieSoftSwitches->ApplyInput (record);
    }

    if (!isApplied && m_refs.gamePort != nullptr)
    {
        isApplied = m_refs.gamePort->ApplyInput (record);
    }

    if (!isApplied && m_mouse != nullptr)
    {
        isApplied = m_mouse->ApplyInput (record);
    }

    if (!isApplied && m_joyport != nullptr)
    {
        isApplied = m_joyport->ApplyInput (record);
    }

    if (!isApplied && record.kind == InputKind::AciaByte && record.detail < m_refs.serialChips.size())
    {
        isApplied = m_refs.serialChips[record.detail]->ApplyInput (record);
    }

    if (!isApplied && m_refs.uthernet != nullptr)
    {
        isApplied = m_refs.uthernet->ApplyInput (record);
    }

    return isApplied;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::DropHostConnections
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::DropHostConnections()
{
    if (m_refs.uthernet != nullptr)
    {
        m_refs.uthernet->DropHostConnections();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::ResyncHostConnections
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::ResyncHostConnections (size_t cursor)
{
    size_t                          index = 0;
    std::vector<std::vector<Byte>>  bytes (m_refs.serialChips.size());



    for (index = cursor; index < m_inputJournal.GetEndIndex(); index++)
    {
        const InputRecord  & record = m_inputJournal.GetRecord (index);

        if (record.kind == InputKind::AciaByte && record.detail < bytes.size())
        {
            bytes[record.detail].push_back (record.value);
        }
    }

    for (index = 0; index < bytes.size(); index++)
    {
        m_refs.serialChips[index]->RequeueReceivedBytes (bytes[index]);
    }

    if (m_refs.uthernet != nullptr)
    {
        m_refs.uthernet->ResyncHostConnections (&m_inputJournal, cursor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetHeldInputWatch
//
//  The devices whose reads see host input, pointed at the watch or at
//  nothing, with the watch told where the machine's position is kept.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetHeldInputWatch (HeldInputWatch * watch)
{
    m_heldInputWatch = watch;

    if (watch != nullptr)
    {
        watch->SetPositionSource (&m_position);
    }

    if (m_refs.keyboard != nullptr)
    {
        m_refs.keyboard->SetHeldInputWatch (watch);
    }

    if (m_refs.gamePort != nullptr)
    {
        m_refs.gamePort->SetHeldInputWatch (watch);
    }

    if (m_refs.iieSoftSwitches != nullptr)
    {
        m_refs.iieSoftSwitches->SetHeldInputWatch (watch);
    }

    if (m_mouse != nullptr)
    {
        m_mouse->SetHeldInputWatch (watch);
    }

    if (m_joyport != nullptr)
    {
        m_joyport->SetHeldInputWatch (watch);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SampleHostInputs
//
//  A key pressed or a paddle moved between two guest reads is in the live
//  machine from the moment the host wrote it, but a read records it only
//  when the guest looks. Sampling at each slice boundary puts the change in
//  the journal where the CPU thread first held it, as a boundary record, so
//  every position after it replays with it. Within a slice a change still
//  waits for the next read or the next sample, whichever comes first.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SampleHostInputs()
{
    if (!m_inputJournal.IsOn())
    {
        return;
    }

    m_inputJournal.SetSampling (true);

    if (m_refs.keyboard != nullptr)
    {
        m_refs.keyboard->SampleHostInputs();
    }

    if (m_refs.gamePort != nullptr)
    {
        m_refs.gamePort->SampleHostInputs();
    }

    if (m_refs.iieSoftSwitches != nullptr)
    {
        m_refs.iieSoftSwitches->SampleHostInputs();
    }

    if (m_mouse != nullptr)
    {
        m_mouse->SampleHostInputs();
    }

    if (m_joyport != nullptr)
    {
        m_joyport->SampleHostInputs();
    }

    m_inputJournal.SetSampling (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SaveHostInputState
//
//  Each device's own section, back to back, in GetHostInputParts order.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::SaveHostInputState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    const_cast<MachineHost &> (*this).GetHostInputParts (m_hostInputParts);

    for (const IMachineState * part : m_hostInputParts)
    {
        hr = part->SaveState (writer);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::LoadHostInputState
//
//  A blob saved by this machine's devices loads back into the same devices;
//  one from another machine fails on the first section tag that differs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::LoadHostInputState (std::string_view blob)
{
    HRESULT      hr     = S_OK;
    StateReader  reader (reinterpret_cast<const Byte *> (blob.data()), blob.size());



    GetHostInputParts (m_hostInputParts);

    for (IMachineState * part : m_hostInputParts)
    {
        hr = part->LoadState (reader);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::GetHostInputParts
//
//  The devices whose state the UI and controller threads write, each once:
//  the keyboard through its most derived type, as GetStateParts saves it.
//  Filled into the caller's list, which keeps its capacity from call to call.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::GetHostInputParts (std::vector<IMachineState *> & outParts)
{
    IMachineState  * candidates[] =
    {
        dynamic_cast<IMachineState *> (m_refs.keyboard),
        m_refs.iieSoftSwitches,
        m_refs.gamePort,
        m_mouse.get(),
        m_joyport.get(),
    };



    outParts.clear();

    for (IMachineState * part : candidates)
    {
        if (part != nullptr)
        {
            outParts.push_back (part);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetOpcodeWatch
//
//  Kept here as well as in the CPU, so a CPU the machine is rebuilt with
//  watches the same opcodes.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetOpcodeWatch (const bool * opcodes, IOpcodeWatcher * watcher)
{
    m_watchOpcodes = opcodes;
    m_watcher      = watcher;

    if (m_cpu != nullptr)
    {
        m_cpu->GetCpu6502()->SetOpcodeWatch (opcodes, watcher);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::FinishStep
//
//  What the instruction or interrupt just executed cost, added to the clock
//  and to the devices that count cycles.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineHost::FinishStep()
{
    Byte      cycles        = m_cpu->GetLastInstructionCycles();
    uint64_t  historyCycles = 0;



    //  The //c Plus's accelerator runs the CPU faster than the machine's
    //  clock, so the instruction takes fewer of the machine's cycles.
    if (m_accelerator != nullptr)
    {
        cycles = m_accelerator->Scale (cycles);
    }

    m_cpu->AddCycles (cycles);

    historyCycles = m_cpu->GetHistoryCycles();

    if ((historyCycles >> kCatchUpShift) != ((historyCycles - cycles) >> kCatchUpShift))
    {
        m_cpu->SyncBusCycle();
        CatchUpCards();
    }

    if (m_refs.diskController != nullptr)
    {
        m_refs.diskController->Tick (cycles);
    }

    if (m_refs.mockingboard != nullptr)
    {
        m_refs.mockingboard->Tick (cycles);
    }

    for (Acia6551 * acia : m_refs.serialChips)
    {
        acia->Tick (cycles);
    }

    m_connectors.Tick (cycles);

    if (m_refs.uthernet != nullptr)
    {
        m_refs.uthernet->Tick (cycles);
    }

    m_position++;

    if (m_historyRecorder != nullptr)
    {
        m_historyRecorder->OnInstructionEnd();
    }

    return (cycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::CatchUpCards
//
//  Cards with separate processors run up to the Apple's time, so their
//  firmware's timers run while the guest doesn't access them. FinishStep
//  calls it each time the history's cycle count crosses a multiple of
//  2^kCatchUpShift, so the state at a position is the same however the run is
//  cut into slices.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::CatchUpCards()
{
    for (LironCard * lironCard : m_refs.lironCards)
    {
        lironCard->CatchUp (CatchUpKind::BetweenFrames);
    }

    for (Apple35ControllerCard * apple35Card : m_refs.apple35Cards)
    {
        apple35Card->CatchUp (CatchUpKind::BetweenFrames);
    }

    if (m_refs.apple2cSmartPort != nullptr)
    {
        m_refs.apple2cSmartPort->GetPort().CatchUp (CatchUpKind::BetweenFrames);
    }

    if (m_refs.apple2cPlusDiskPort != nullptr)
    {
        m_refs.apple2cPlusDiskPort->GetPort().CatchUp (CatchUpKind::BetweenFrames);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::RunCycles
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineHost::RunCycles (uint64_t cycleBudget)
{
    uint64_t  spent  = 0;
    Byte      cycles = 0;



    if (m_cpu == nullptr)
    {
        return (0);
    }

    while (spent < cycleBudget)
    {
        // A pause raised on another thread ends the slice here, between two
        // instructions. Relaxed, since the stop only has to be seen: the CPU
        // thread takes the pause mutex before it parks.
        if (m_stopFlag != nullptr && m_stopFlag->load (std::memory_order_relaxed))
        {
            break;
        }

        cycles  = StepOne();
        spent  += cycles;

        // A hook stop returns a short slice: StepOne declined to execute, or
        // the instruction it just ran raised a stop for the next boundary.
        if (m_debugHook != nullptr && (cycles == 0 || (m_debugHook->GetFilter().everyInstruction && m_debugHook->HasPendingStop())))
        {
            break;
        }

        // The instruction just run read a line the input held back behind
        // live would change: the caller puts the machine back before it.
        if (m_heldInputWatch != nullptr && m_heldInputWatch->HasHit())
        {
            break;
        }
    }

    return (spent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SoftReset
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SoftReset()
{
    // Flush dirty disks through the store before the devices reset, so a
    // reset doesn't lose the guest's writes and every safeguard on a write
    // applies. Not while reverse execution holds the disks, nor during a
    // replay of a recorded reset.
    m_diskStore->SoftReset();

    // A reset stops the recorder, as pressing reset mid-load would leave a real
    // one running into a guest that is no longer listening. The tape stays in.
    StopTape();

    m_memoryBus->SoftResetAll();

    if (m_accelerator != nullptr)
    {
        m_accelerator->Reset();
    }

    if (m_mmu != nullptr)
    {
        m_mmu->OnSoftReset();
    }

    m_interruptController.SoftReset();

    // //c IOU mouse: /RESET clears the interrupt latches + enables and
    // shuts the IOU access gate (matches power-on state).
    if (m_mouse != nullptr)
    {
        m_mouse->Reset();
    }

    if (m_videoTiming != nullptr)
    {
        m_videoTiming->SoftReset();
    }

    if (m_cpu != nullptr)
    {
        m_cpu->SoftReset();
    }

    // Last, so the window is stamped from the CPU's post-reset counter.
    if (m_joyport != nullptr)
    {
        m_joyport->OnMachineReset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::PowerCycle
//
//  Refills every DRAM-owning device with the power-on pattern, then runs the
//  reset sequence.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::PowerCycle()
{
    HRESULT  hrFlush = S_OK;



    if (m_prng == nullptr)
    {
        return;
    }

    StopTape();

    // Auto-flush dirty disks before reseeding device state so writes don't
    // get lost across a power cycle. Mounts persist (matching
    // DiskImageStore::SoftReset semantics -- see the comment block on
    // DiskImageStore::PowerCycle, which is the unmount-everything variant
    // tests can opt into directly). Not while reverse execution holds the
    // disks, nor during a replay of a recorded power cycle.
    hrFlush = m_diskStore->FlushAllUnlessHeld();
    IGNORE_RETURN_VALUE (hrFlush, S_OK);

    m_memoryBus->PowerCycleAll (*m_prng);

    // The Disk II's power cycle points each drive at its empty internal
    // disk, but the mounts persist: the drives go back to the disks in the
    // bays, which a replayed power cycle must do the same way.
    BindDiskDrives();

    if (m_accelerator != nullptr)
    {
        m_accelerator->Reset();
    }

    if (m_mmu != nullptr)
    {
        m_mmu->OnPowerCycle (*m_prng);
    }

    m_interruptController.PowerCycle();

    // //c IOU mouse: power-on state (latches clear, interrupts masked, IOU
    // access gate shut).
    if (m_mouse != nullptr)
    {
        m_mouse->Reset();
    }

    if (m_videoTiming != nullptr)
    {
        m_videoTiming->PowerCycle (*m_prng);
    }

    if (m_cpu != nullptr)
    {
        m_cpu->PowerCycle (*m_prng);
    }

    // After the CPU's power cycle, which is what fills main RAM.
    ApplyPowerOnOverrides();

    // Last: the CPU's power cycle zeroes the cycle counter, and the window
    // is measured from that zero.
    if (m_joyport != nullptr)
    {
        m_joyport->OnMachineReset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::GetStateParts
//
//  The fixed save and load order. The CPU comes first, because devices
//  compare their saved cycle stamps against its counter. Mounted disks come
//  before the slot cards, so the Disk II controller loads against restored
//  media. The keyboard is one owned device, so it is saved once, through its
//  most derived type. Device wiring is not here: the machine builder made it.
//  Filled into the caller's list, which keeps its capacity from call to call,
//  so a save allocates nothing for it.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::GetStateParts (std::vector<IMachineState *> & outParts)
{
    IMachineState  * optional[] =
    {
        m_mmu.get(),
        m_apple2cRomBank.get(),
        m_videoTiming.get(),
        m_mouse.get(),
        m_joyport.get(),
        m_accelerator.get(),
    };
    IMachineState  * device     = nullptr;
    DiskImage      * image      = nullptr;
    int              slot       = 0;
    int              drive      = 0;



    outParts.clear();

    if (m_cpu != nullptr)
    {
        outParts.push_back (m_cpu.get());
    }

    outParts.push_back (&m_interruptController);
    outParts.push_back (m_memoryBus.get());

    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            image = m_diskStore->IsMounted (slot, drive) ? m_diskStore->GetImage (slot, drive) : nullptr;

            if (image != nullptr)
            {
                outParts.push_back (image);
            }
        }
    }

    for (IMachineState * part : optional)
    {
        if (part != nullptr)
        {
            outParts.push_back (part);
        }
    }

    for (const std::unique_ptr<MemoryDevice> & owned : m_ownedDevices)
    {
        device = dynamic_cast<IMachineState *> (owned.get());

        if (device != nullptr)
        {
            outParts.push_back (device);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::GetRomIdentity
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineHost::GetRomIdentity() const
{
    const CxxxRomRouter  * router = (m_mmu != nullptr) ? m_mmu->GetCxxxRouter() : nullptr;



    return m_romIdentity.Get (m_ownedDevices, router, m_apple2cRomBank.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::BindDiskDrives
//
//  Each drive of the Disk II the refs hold reads the disk in its bay, or its
//  own empty disk when the bay is empty.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::BindDiskDrives()
{
    int  drive = 0;



    if (m_refs.diskController == nullptr)
    {
        return;
    }

    for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
    {
        m_refs.diskController->SetExternalDisk (drive, m_diskStore->GetImage (kDiskControllerSlot, drive));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::ApplyPowerOnOverrides
//
//  Bytes whose power-on contents software reads before writing, and which the
//  fill must not decide.
//
//  $03F2-$03F4: the autostart ROM treats a reset as warm, and jumps through
//  the soft-entry vector at $03F2, when the power-up byte at $03F4 equals the
//  vector's high byte XOR $A5. Zeroing all three fails that check, so a power
//  cycle always cold-boots. Without it, a power-on fill that happened to pass
//  sent the ROM into garbage with the screen never cleared (GH #157).
//
//  $4E/$4F: the monitor's random seed, counted up while the ROM waits for a
//  key. A disk that autostarts never waits, so a program that seeds from it
//  sees the fill, and the pattern puts 00 00 here. Pooyan loops forever on a
//  zero seed, so each byte is forced nonzero, the rest of it from the Prng.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::ApplyPowerOnOverrides()
{
    constexpr Word  kSoftEntryLo  = 0x03F2;
    constexpr Word  kPowerUpByte  = 0x03F4;
    constexpr Word  kRandomSeedLo = 0x004E;
    constexpr Word  kRandomSeedHi = 0x004F;
    constexpr Byte  kNonzeroBit   = 0x20;



    Word  address = 0;



    for (address = kSoftEntryLo; address <= kPowerUpByte; address++)
    {
        m_memoryBus->WriteByte (address, 0);
    }

    m_memoryBus->WriteByte (kRandomSeedLo, static_cast<Byte> (kNonzeroBit | m_prng->NextByte()));
    m_memoryBus->WriteByte (kRandomSeedHi, static_cast<Byte> (kNonzeroBit | m_prng->NextByte()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::AttachObservers
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::AttachObservers (const MachineObservers & observers, ObserverSlot slot)
{
    if (m_refs.diskController != nullptr)
    {
        m_refs.diskController->SetEventSink (slot, observers.disk);
    }

    if (m_refs.keyboard != nullptr)
    {
        m_refs.keyboard->SetInputEventSink (slot, observers.input);
    }

    // The //e soft-switch bank reports the switches the keyboard does not
    // own (80COL, 80STORE, ALTCHARSET); null on a ][ or ][+.
    if (m_refs.iieSoftSwitches != nullptr)
    {
        m_refs.iieSoftSwitches->SetInputEventSink (slot, observers.input);
    }

    if (m_refs.gamePort != nullptr)
    {
        m_refs.gamePort->SetInputEventSink (slot, observers.input);
    }

    if (m_memoryBus != nullptr)
    {
        m_memoryBus->SetIoAccessSink (slot, (observers.ioFilter != nullptr) ? observers.io : nullptr, (observers.ioFilter != nullptr) ? *observers.ioFilter : IoAccessFilter());
    }

    if (slot == ObserverSlot::History)
    {
        AttachCommEventSink (observers.comm);
    }

    //  The block devices and 3.5-inch drives have one sink each, whichever
    //  slot attached it last.
    for (IBlockDeviceEventSource * source : m_refs.blockDeviceSources)
    {
        source->SetBlockDeviceEventSink (observers.blockDevice);
    }

    for (IFloppy35EventSource * source : m_refs.floppy35Sources)
    {
        source->SetFloppy35EventSink (observers.floppy35);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::AttachCommEventSink
//
//  Each serial chip, numbered as the input journal numbers it, and the
//  network card. AttachInputJournal calls this again, for the devices of a
//  machine the builder switched to.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::AttachCommEventSink (ICommEventSink * sink)
{
    m_commSink = sink;

    for (Acia6551 * acia : m_refs.serialChips)
    {
        acia->SetCommEventSink (sink);
    }

    if (m_refs.uthernet != nullptr)
    {
        m_refs.uthernet->SetCommEventSink (sink);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::GetDiagnosticsProviders
//
////////////////////////////////////////////////////////////////////////////////

std::vector<const IDiagnosticsProvider *> MachineHost::GetDiagnosticsProviders() const
{
    std::vector<const IDiagnosticsProvider *>  providers = { this };
    const IDiagnosticsProvider               * devices[] =
    {
        m_refs.keyboard,
        m_refs.softSwitches,
        m_mmu.get(),
        m_refs.diskController,
        m_refs.mockingboard,
        m_refs.superSerialCard,
        m_refs.uthernet,
    };



    for (const IDiagnosticsProvider * device : devices)
    {
        if (device != nullptr)
        {
            providers.push_back (device);
        }
    }

    for (const PrinterCard * card : m_connectors.GetParallelCards())
    {
        if (card != nullptr)
        {
            providers.push_back (card);
        }
    }

    //  A 6551 that is a port by itself, as each of the //c's is; a card's
    //  is in the card's panel.
    for (const Acia6551 * acia : m_refs.serialChips)
    {
        if (!acia->GetDiagnosticsId().empty())
        {
            providers.push_back (acia);
        }
    }

    if (m_refs.cassettePort != nullptr)
    {
        providers.push_back (&m_cassetteDiagnostics);
    }

    return providers;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::GetDiagnostics
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::GetDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    static constexpr const char * kSpeeds[] = { "authentic", "double", "maximum" };
    DiagnosticsGroup              cpu       { "CPU", {} };
    DiagnosticsGroup              video     { "Video", {} };



    cpu.rows.push_back (MakeTextRow ("Cycles", std::format ("{}", (m_cpu != nullptr) ? m_cpu->GetTotalCycles() : 0)));
    cpu.rows.push_back (MakeTextRow ("Clock",  std::format ("{} Hz", m_config->clockSpeed)));
    cpu.rows.push_back (MakeTextRow ("Speed",  kSpeeds[(size_t) m_speedMode]));
    snapshot.groups.push_back (std::move (cpu));

    if (m_videoTiming != nullptr)
    {
        video.rows.push_back (MakeTextRow ("Scanline",       std::format ("{}", m_videoTiming->GetCurrentScanline())));
        video.rows.push_back (MakeTextRow ("Cycle in line",  std::format ("{}", m_videoTiming->GetHorizontalPos())));
        video.rows.push_back (MakeTextRow ("Cycle in frame", std::format ("{}", m_videoTiming->GetCycleInFrame())));
        snapshot.groups.push_back (std::move (video));
    }

}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::ConnectDiskToController
//
//  Points the slot 6 controller's drive at the store's image for that bay. A
//  null image, on an eject, selects the controller's own empty disk. Only
//  slot 6 has a controller in the machine references.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::ConnectDiskToController (int slot, int drive)
{
    Disk2Controller  *  controller = m_refs.diskController;



    if (slot != 6 || controller == nullptr)
    {
        return;
    }

    controller->SetExternalDisk (drive, m_diskStore->GetImage (slot, drive));
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::StopTape
//
//  Stops the recorder at the current bus cycle; the tape stays inserted.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::StopTape()
{
    uint64_t  now = m_cpu != nullptr ? *m_cpu->GetBusCyclePtr() : 0;



    m_tapeDeck->Stop (now);
    m_tapeResetCount.fetch_add (1, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::FindFloppy35
//
////////////////////////////////////////////////////////////////////////////////

Sony35Drive * MachineHost::FindFloppy35 (int slot, int drive)
{
    Sony35Drive  * found = nullptr;
    size_t         index = (size_t) drive;



    // The //c Plus's drive 0 is its internal drive, then the chain in order.
    if (slot == 0 && m_refs.apple2cPlusDiskPort != nullptr)
    {
        Apple2cPlusDiskPort  & port     = *m_refs.apple2cPlusDiskPort;
        size_t                 external = (size_t) port.GetExternalCount();



        if (drive == 0)
        {
            found = &port.GetInternalDrive();
        }
        else if (index <= external)
        {
            found = &port.GetExternalDrive (drive - 1);
        }
        else if (index - 1 - external < port.GetPort().GetDriveCount())
        {
            found = &port.GetPort().GetDrive (index - 1 - external).GetMechanism();
        }
    }
    else if (slot == 0 && m_refs.apple2cSmartPort != nullptr && index < m_refs.apple2cSmartPort->GetPort().GetDriveCount())
    {
        found = &m_refs.apple2cSmartPort->GetPort().GetDrive (index).GetMechanism();
    }

    for (LironCard * card : m_refs.lironCards)
    {
        if (card->GetSlot() == slot && index < card->GetDriveCount())
        {
            found = &card->GetDrive (index).GetMechanism();
        }
    }

    for (Apple35ControllerCard * card : m_refs.apple35Cards)
    {
        if (card->GetSlot() == slot && (drive == 0 || drive == 1))
        {
            found = &card->GetCardMemory().GetDrive (drive);
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::InsertFloppy35
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::InsertFloppy35 (int slot, int drive, const std::string & path, std::wstring & outError)
{
    HRESULT        hr     = S_OK;
    Sony35Drive  * target = FindFloppy35 (slot, drive);



    CBREx (target != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = target->Insert (GetBlockMediaFileIo(), path, ProbeBlockMedia (path), outError);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::EjectFloppy35
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::EjectFloppy35 (int slot, int drive)
{
    HRESULT        hr     = S_OK;
    Sony35Drive  * target = FindFloppy35 (slot, drive);



    CBREx (target != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = target->Eject();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::AttachHardDisk
//
//  The image before is attached again when the new one fails, so a failed
//  change is no change.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::AttachHardDisk (int slot, int drive, const std::string & path, std::wstring & outError)
{
    HRESULT                  hr         = S_OK;
    HRESULT                  hrPrevious = S_OK;
    BlockDeviceMedia       * media      = nullptr;
    PortConfig             * port       = nullptr;
    const BlockDriveSpec   * spec       = nullptr;
    uint32_t                 maxBlocks  = 0;
    std::string              previous;
    std::wstring             ignored;



    for (AppleScsiCard * card : m_refs.scsiCards)
    {
        if (card->GetSlot() == slot && drive >= 0 && drive < AppleScsiCard::kTargets)
        {
            media = &card->GetTarget (drive).GetMedia();
        }
    }

    for (ProFileCard * card : m_refs.proFileCards)
    {
        if (card->GetDrive().GetSlot() == slot && drive == 0)
        {
            media     = &card->GetDrive().GetMedia();
            maxBlocks = card->GetDrive().GetCapacity();
        }
    }

    for (SlotConfig & entry : m_config->slots)
    {
        if (entry.slot == slot && drive >= 0 && (size_t) drive < entry.ports.size())
        {
            port = &entry.ports[(size_t) drive];
        }
    }

    CBREx (media != nullptr && port != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    if (maxBlocks == 0)
    {
        spec = BlockDeviceCatalog::FindDrive (port->device);
        CBREx (spec != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

        maxBlocks = spec->maxBlocks;
    }

    previous = media->GetPath();

    hr = media->Attach (GetBlockMediaFileIo(), path, maxBlocks, ProbeBlockMedia (path), outError);

    if (SUCCEEDED (hr) && StorageCardWiring::RejectFloppyImage (*media, outError))
    {
        hr = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
    }

    if (FAILED (hr) && !previous.empty())
    {
        hrPrevious = media->Attach (GetBlockMediaFileIo(), previous, maxBlocks, ProbeBlockMedia (previous), ignored);
        IGNORE_RETURN_VALUE (hrPrevious, S_OK);
    }

    CHR (hr);

    port->image = path;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::DetachSerialPorts
//
//  The connectors let go of their endpoints, then the machine destroys the
//  endpoints it owns.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::DetachSerialPorts()
{
    m_connectors.DetachSerialPorts();
    m_ownedAciaEndpoints.clear();
}

#include "Pch.h"

#include "Shell/MachineHost.h"

#include "Core/Prng.h"
#include "Core/StateHash.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DebugHook.h"
#include "Debugger/Reverse/HistoryRecorder.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/RomDevice.h"
#include "Devices/RomGeneration.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::MachineHost
//
////////////////////////////////////////////////////////////////////////////////

MachineHost::MachineHost() :
    m_memoryBus (std::make_unique<MemoryBus>()),
    m_charRom   (std::make_unique<CharacterRomData>()),
    m_diskStore (std::make_unique<DiskImageStore>()),
    m_config    (std::make_unique<MachineConfig>())
{
    // A disk leaving a bay is stamped with the position, and a disk change is
    // a boundary in reverse execution's history.
    m_diskStore->SetPositionSource (&m_position);
    m_diskStore->SetMediaChangeListener ([this] () { OnMediaChanged(); });
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
        m_historyRecorder->OnInstructionStart (*m_cpu->GetCpu6502(), m_position);
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
        m_historyRecorder->OnInstructionStart (*m_cpu->GetCpu6502(), m_position);
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
        m_historyRecorder->OnInstructionStart (*m_cpu->GetCpu6502(), m_position);
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
//  devices, so the builder calls this again once they are wired.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::AttachInputJournal()
{
    InputJournal  * journal = m_inputJournal.IsOn() ? &m_inputJournal : nullptr;



    m_inputJournal.SetCycleSource    ((m_cpu != nullptr) ? m_cpu->GetCycleCounterPtr() : nullptr);
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

    return isApplied;
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



    const_cast<MachineHost &> (*this).GetHostInputParts (m_stateParts);

    for (const IMachineState * part : m_stateParts)
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



    GetHostInputParts (m_stateParts);

    for (IMachineState * part : m_stateParts)
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
    Byte  cycles = m_cpu->GetLastInstructionCycles();



    m_cpu->AddCycles (cycles);

    if (m_refs.diskController != nullptr)
    {
        m_refs.diskController->Tick (cycles);
    }

    if (m_refs.mockingboard != nullptr)
    {
        m_refs.mockingboard->Tick (cycles);
    }

    m_position++;

    return (cycles);
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
        cycles  = StepOne();
        spent  += cycles;

        // A hook stop returns a short slice: StepOne declined to execute, or
        // the instruction it just ran raised a stop for the next boundary.
        if (m_debugHook != nullptr && (cycles == 0 || (m_debugHook->GetFilter().everyInstruction && m_debugHook->HasPendingStop())))
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
    m_memoryBus->SoftResetAll();

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

    // Auto-flush dirty disks before reseeding device state so writes don't
    // get lost across a power cycle. Mounts persist (matching
    // DiskImageStore::SoftReset semantics -- see the comment block on
    // DiskImageStore::PowerCycle, which is the unmount-everything variant
    // tests can opt into directly). Not while reverse execution holds the
    // disks, nor during a replay of a recorded power cycle.
    hrFlush = m_diskStore->FlushAllUnlessHeld();
    IGNORE_RETURN_VALUE (hrFlush, S_OK);

    m_memoryBus->PowerCycleAll (*m_prng);

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
//  MachineHost::SaveState
//
//  One section holding the header, then every stateful part in the order
//  GetStateParts gives.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    // GetStateParts hands out mutable pointers so LoadState can share it;
    // only SaveState, which is const on every part, is called through them.
    const_cast<MachineHost &> (*this).GetStateParts (m_stateParts);

    writer.BeginSection (kStateTag, kStateVersion);

    WriteStateHeader (writer, m_stateParts.size());

    for (const IMachineState * part : m_stateParts)
    {
        hr = part->SaveState (writer);
        CHR (hr);
    }

    hr = writer.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::LoadState
//
//  The header is checked before any part is touched, so a state from another
//  machine fails without disturbing this one. A failure after that leaves the
//  machine partly loaded.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::LoadState (StateReader & reader)
{
    HRESULT   hr         = S_OK;
    uint16_t  version    = 0;
    uint32_t  savedParts = 0;
    size_t    partCount  = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    CBREx (version == kStateVersion, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = CheckStateHeader (reader, savedParts);
    CHR (hr);

    // After the header, which put the saved disks back in their bays.
    GetStateParts (m_stateParts);
    partCount = m_stateParts.size();

    CBREx (savedParts == partCount, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (IMachineState * part : m_stateParts)
    {
        hr = part->LoadState (reader);
        CHR (hr);
    }

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
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
//  A hash over every ROM image the machine reads: the ROM devices in device
//  order, the //e internal $Cxxx ROM and the slot ROMs, then the //c
//  firmware banks. A debugger ROM patch changes it: a patched ROM is a
//  different ROM.
//
//  Every snapshot carries it, so the hash is kept and taken again only once
//  RomGeneration says a ROM image changed since.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineHost::GetRomIdentity() const
{
    constexpr int          kRomBankCount = 2;
    uint64_t               hash          = StateHash::kSeed;
    const RomDevice      * rom           = nullptr;
    const CxxxRomRouter  * router        = nullptr;
    size_t                 size          = 0;
    int                    index         = 0;
    uint64_t               generation    = RomGeneration::Get();



    if (generation == m_romIdentityGeneration)
    {
        return m_romIdentity;
    }

    for (const std::unique_ptr<MemoryDevice> & owned : m_ownedDevices)
    {
        rom = dynamic_cast<const RomDevice *> (owned.get());

        if (rom != nullptr)
        {
            size = static_cast<size_t> (rom->GetEnd() - rom->GetStart()) + 1;
            hash = HashBytes (hash, rom->GetData(), size);
        }
    }

    router = (m_mmu != nullptr) ? m_mmu->GetCxxxRouter() : nullptr;

    for (index = 0; router != nullptr && index < DiskImageStore::kSlotCount; index++)
    {
        hash = HashBytes (hash, router->GetSlotRom (index));
    }

    if (router != nullptr)
    {
        hash = HashBytes (hash, router->GetInternalRom());
    }

    for (index = 0; m_apple2cRomBank != nullptr && index < kRomBankCount; index++)
    {
        hash = HashBytes (hash, m_apple2cRomBank->GetBankImage (index));
    }

    m_romIdentity           = hash;
    m_romIdentityGeneration = generation;
    m_romIdentityHashes++;

    return hash;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::HashBytes
//
//  StateHash continuing from hash. Every snapshot carries the ROM identity,
//  so this runs at every ring checkpoint; a byte-at-a-time hash over the
//  ROMs cost more than saving the rest of the machine.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineHost::HashBytes (uint64_t hash, const Byte * data, size_t size)
{
    return StateHash::Hash (data, size, hash);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::HashBytes
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineHost::HashBytes (uint64_t hash, const std::vector<Byte> & bytes)
{
    return HashBytes (hash, bytes.data(), bytes.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::WriteStateHeader
//
//  Which machine and ROM set, which disk each drive bay holds, how many parts
//  follow, and the power-on Prng.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::WriteStateHeader (StateWriter & writer, size_t partCount) const
{
    int  slot  = 0;
    int  drive = 0;



    writer.WriteUInt32 (static_cast<uint32_t> (m_currentMachineName.size()));

    for (wchar_t ch : m_currentMachineName)
    {
        writer.WriteWord (static_cast<Word> (ch));
    }

    writer.WriteUInt64 (GetRomIdentity());

    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            writer.WriteUInt64 (m_diskStore->GetMediaId (slot, drive));
        }
    }

    writer.WriteUInt32 (static_cast<uint32_t> (partCount));
    writer.WriteBool   (m_prng != nullptr);
    writer.WriteUInt64 ((m_prng != nullptr) ? m_prng->GetState() : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::CheckStateHeader
//
//  Fails with ERROR_INVALID_DATA unless the state was saved by a machine of
//  the same name and ROM set, and every disk it held is mounted or kept by
//  the disk store. Only then are the saved disks put back in their bays and
//  the Prng restored; the part count is the caller's to check, since it
//  depends on the disks.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::CheckStateHeader (StateReader & reader, uint32_t & outParts)
{
    HRESULT       hr             = S_OK;
    uint32_t      nameLength     = 0;
    size_t        expectedLength = 0;
    std::wstring  name;
    Word          ch             = 0;
    uint64_t      romIdentity    = 0;
    uint64_t      expectedRom    = 0;
    MediaIds      mediaIds       = {};
    bool          canSeat        = false;
    uint32_t      savedParts     = 0;
    bool          hasPrng        = false;
    uint64_t      prngState      = 0;
    uint32_t      i              = 0;



    reader.ReadUInt32 (nameLength);

    expectedLength = m_currentMachineName.size();
    CBREx (nameLength == expectedLength, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (i = 0; i < nameLength; i++)
    {
        reader.ReadWord (ch);
        name.push_back (static_cast<wchar_t> (ch));
    }

    reader.ReadUInt64 (romIdentity);

    for (uint64_t & mediaId : mediaIds)
    {
        reader.ReadUInt64 (mediaId);
    }

    reader.ReadUInt32 (savedParts);
    reader.ReadBool   (hasPrng);
    reader.ReadUInt64 (prngState);

    hr = reader.GetResult();
    CHR (hr);

    expectedRom = GetRomIdentity();

    CBREx (name        == m_currentMachineName, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (romIdentity == expectedRom,          HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    canSeat = CanSeatMedia (mediaIds);
    CBREx (canSeat, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = SeatMedia (mediaIds);
    CHR (hr);

    if (hasPrng && m_prng != nullptr)
    {
        m_prng->SetState (prngState);
    }

    outParts = savedParts;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::CanSeatMedia
//
//  Whether every saved disk is mounted or kept, so the bays can be put back
//  without failing part way.
//
////////////////////////////////////////////////////////////////////////////////

bool MachineHost::CanSeatMedia (const MediaIds & mediaIds) const
{
    bool  canSeat = true;
    int   slot    = 0;
    int   drive   = 0;



    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            canSeat = canSeat && m_diskStore->CanSeatMedia (slot, drive, mediaIds[slot * DiskImageStore::kDriveCount + drive]);
        }
    }

    return canSeat;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SeatMedia
//
//  Puts each saved disk back in its bay, and points the Disk II at whatever
//  its bays now hold. Nothing is flushed: a disk that leaves a bay here is
//  kept by the store with its writes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::SeatMedia (const MediaIds & mediaIds)
{
    HRESULT  hr        = S_OK;
    bool     isChanged = false;
    int      slot      = 0;
    int      drive     = 0;



    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            hr = m_diskStore->SeatMedia (slot, drive, mediaIds[slot * DiskImageStore::kDriveCount + drive], isChanged);
            CHR (hr);

            if (isChanged && slot == kDiskControllerSlot && m_refs.diskController != nullptr)
            {
                m_refs.diskController->SetExternalDisk (drive, m_diskStore->GetImage (slot, drive));
            }
        }
    }

Error:
    return hr;
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

void MachineHost::AttachObservers (const MachineObservers & observers)
{
    if (m_refs.diskController != nullptr)
    {
        m_refs.diskController->SetEventSink (observers.disk);
    }

    if (m_refs.keyboard != nullptr)
    {
        m_refs.keyboard->SetInputEventSink (observers.input);
    }

    // The //e soft-switch bank reports the switches the keyboard does not
    // own (80COL, 80STORE, ALTCHARSET); null on a ][ or ][+.
    if (m_refs.iieSoftSwitches != nullptr)
    {
        m_refs.iieSoftSwitches->SetInputEventSink (observers.input);
    }

    if (m_refs.gamePort != nullptr)
    {
        m_refs.gamePort->SetInputEventSink (observers.input);
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
        m_refs.printerCard,
    };



    for (const IDiagnosticsProvider * device : devices)
    {
        if (device != nullptr)
        {
            providers.push_back (device);
        }
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

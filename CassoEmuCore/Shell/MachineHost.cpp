#include "Pch.h"

#include "Shell/MachineHost.h"
#include "Core/Prng.h"
#include "Debugger/Reverse/HistoryRecorder.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/IAciaEndpoint.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/CharacterRomData.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"
#include "Machines/Apple3/Apple3System.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::MachineHost
//
//  A disk leaving a bay is stamped with the position, and a disk change is
//  a boundary in reverse execution's history. The store frees an image on
//  an eject or a remount, so the controller is pointed again on every bay
//  change rather than by each caller. After a restored state, the bay has
//  the same image and the controller has its restored state.
//
////////////////////////////////////////////////////////////////////////////////

MachineHost::MachineHost() :
    m_diskStore      (std::make_unique<DiskImageStore>()),
    m_config         (std::make_unique<MachineConfig>()),
    m_printerRing    (std::make_unique<PrinterByteRing>()),
    m_connectors     (m_parts.refs, *m_printerRing, *m_config),
    m_stateStream    (*this, *m_diskStore, *m_config, m_currentMachineName),
    m_runner         (m_parts, m_connectors),
    m_hostInput      (m_parts, m_runner.GetPositionPtr()),
    m_storage        (m_parts, *m_config, *m_diskStore),
    m_outputMute     (m_parts, m_connectors),
    m_observerWiring (m_parts),
    m_tape           (m_parts),
    m_clockPanel     (m_parts, *m_config, m_connectors, m_tape)
{
    m_diskStore->SetPositionSource (m_runner.GetPositionPtr());
    m_diskStore->SetMediaChangeListener ([this] () { OnMediaChanged(); });

    m_diskStore->SetBayChangeSink ([this] (int slot, int drive, BayChange change)
    {
        if (change != BayChange::Restored)
        {
            m_storage.ConnectDiskToController (slot, drive);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::~MachineHost
//
//  Out of line, where the types of the parts are complete.
//
////////////////////////////////////////////////////////////////////////////////

MachineHost::~MachineHost()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::OnMediaChanged
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::OnMediaChanged()
{
    HistoryRecorder  * recorder = m_runner.GetHistoryRecorder();



    if (recorder != nullptr)
    {
        recorder->OnMediaChanged (*this);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::NoteDebuggerEdit
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::NoteDebuggerEdit()
{
    HistoryRecorder  * recorder = m_runner.GetHistoryRecorder();



    if (recorder != nullptr)
    {
        recorder->OnMachineEdited (*this);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetCpu
//
//  The new CPU watches the opcodes the runner has.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetCpu (std::unique_ptr<EmuCpu> cpu)
{
    m_parts.cpu = std::move (cpu);
    m_runner.ApplyOpcodeWatch();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetPrng
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetPrng (std::unique_ptr<Prng> prng)
{
    m_parts.prng = std::move (prng);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetMmu
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetMmu (std::unique_ptr<Apple2eMmu> mmu)
{
    m_parts.mmu = std::move (mmu);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetApple2cRomBank
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetApple2cRomBank (std::unique_ptr<Apple2cRomBank> romBank)
{
    m_parts.apple2cRomBank = std::move (romBank);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetMouse
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetMouse (std::unique_ptr<AppleMouse> mouse)
{
    m_parts.mouse = std::move (mouse);
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
    m_parts.memoryBus->SetIoObserver (accelerator.get());
    m_parts.accelerator = std::move (accelerator);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetJoyport
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetJoyport (std::unique_ptr<SiriusJoyport> joyport)
{
    m_parts.joyport = std::move (joyport);
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
    m_parts.videoTiming = std::move (videoTiming);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetApple3
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetApple3 (std::unique_ptr<Apple3System> apple3)
{
    m_parts.apple3 = std::move (apple3);
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



    return dir;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::AttachDevices
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::AttachDevices()
{
    m_hostInput.AttachJournal();
    m_observerWiring.AttachAgain();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetHeldInputWatch
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetHeldInputWatch (HeldInputWatch * watch)
{
    m_hostInput.SetHeldInputWatch (watch);
    m_runner.SetHeldInputWatch (watch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::DetachSerialPorts
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::DetachSerialPorts()
{
    m_connectors.DetachSerialPorts();
    m_parts.ownedAciaEndpoints.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SoftReset
//
//  Dirty disks are flushed through the store before the devices reset, so
//  the guest's writes survive a reset and every safeguard on a write
//  applies: not while reverse execution has the disks, nor during a replay
//  of a reset from history. The tape stops, and stays in. On the Apple ///,
//  environment bit 4 clear turns the reset key off (SVC 8.8), so nothing
//  resets.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SoftReset()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_parts.apple3 != nullptr && !m_parts.apple3->IsResetKeyEnabled(), S_OK);

    m_diskStore->SoftReset();
    m_tape.Stop();

    m_parts.memoryBus->SoftResetAll();

    if (m_parts.accelerator != nullptr)
    {
        m_parts.accelerator->Reset();
    }

    if (m_parts.mmu != nullptr)
    {
        m_parts.mmu->OnSoftReset();
    }

    m_parts.interruptController.SoftReset();

    // //c IOU mouse: /RESET clears the interrupt latches and enables and
    // shuts the IOU access gate, as at power-on.
    if (m_parts.mouse != nullptr)
    {
        m_parts.mouse->Reset();
    }

    if (m_parts.videoTiming != nullptr)
    {
        m_parts.videoTiming->SoftReset();
    }

    // The Apple ///'s reset line resets the VIAs, which selects the ROM the
    // CPU's reset vector is read from.
    if (m_parts.apple3 != nullptr)
    {
        m_parts.apple3->Reset();
    }

    if (m_parts.cpu != nullptr)
    {
        m_parts.cpu->SoftReset();
    }

    // Last, so the window is stamped from the CPU's post-reset counter.
    if (m_parts.joyport != nullptr)
    {
        m_parts.joyport->OnMachineReset();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::PowerCycle
//
//  Refills every DRAM-owning device with the power-on pattern, then runs the
//  reset sequence. Dirty disks are flushed first, so the writes survive;
//  the mounts persist (see DiskImageStore::PowerCycle for the variant that
//  unmounts everything). Not while reverse execution has the disks, nor
//  during a replay of a power cycle from history.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::PowerCycle()
{
    HRESULT  hrFlush = S_OK;



    if (m_parts.prng == nullptr)
    {
        return;
    }

    m_tape.Stop();

    hrFlush = m_diskStore->FlushAllUnlessHeld();
    IGNORE_RETURN_VALUE (hrFlush, S_OK);

    m_parts.memoryBus->PowerCycleAll (*m_parts.prng);

    // The Disk II's power cycle points each drive at its empty disk, but the
    // mounts persist: the drives go back to the disks in the bays, which a
    // replayed power cycle must do the same way.
    m_storage.BindDiskDrives();

    if (m_parts.accelerator != nullptr)
    {
        m_parts.accelerator->Reset();
    }

    if (m_parts.mmu != nullptr)
    {
        m_parts.mmu->OnPowerCycle (*m_parts.prng);
    }

    m_parts.interruptController.PowerCycle();

    // //c IOU mouse: latches clear, interrupts masked, IOU access gate shut.
    if (m_parts.mouse != nullptr)
    {
        m_parts.mouse->Reset();
    }

    if (m_parts.videoTiming != nullptr)
    {
        m_parts.videoTiming->PowerCycle (*m_parts.prng);
    }

    // The Apple ///'s floppy controller isn't on the bus, so its power cycle
    // comes here and the mounted disks are bound to it again.
    if (m_parts.apple3 != nullptr)
    {
        m_parts.apple3->PowerOn (*m_parts.prng);
        m_storage.BindDiskDrives();
    }

    if (m_parts.cpu != nullptr)
    {
        m_parts.cpu->PowerCycle (*m_parts.prng);
    }

    // After the CPU's power cycle, which is what fills main RAM. The bytes are
    // the Apple II monitor's, so they don't apply to the Apple ///.
    if (m_parts.apple3 == nullptr)
    {
        ApplyPowerOnOverrides();
    }

    // Last: the CPU's power cycle zeroes the cycle counter, and the window
    // is measured from that zero.
    if (m_parts.joyport != nullptr)
    {
        m_parts.joyport->OnMachineReset();
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
//  Filled into the caller's list, whose capacity is reused from call to
//  call, so a save doesn't allocate for it.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::GetStateParts (std::vector<IMachineState *> & outParts)
{
    IMachineState  * optional[] =
    {
        m_parts.mmu.get(),
        m_parts.apple2cRomBank.get(),
        m_parts.videoTiming.get(),
        m_parts.mouse.get(),
        m_parts.joyport.get(),
        m_parts.accelerator.get(),
    };
    IMachineState  * device     = nullptr;
    DiskImage      * image      = nullptr;
    int              slot       = 0;
    int              drive      = 0;



    outParts.clear();

    if (m_parts.cpu != nullptr)
    {
        outParts.push_back (m_parts.cpu.get());
    }

    outParts.push_back (&m_parts.interruptController);
    outParts.push_back (m_parts.memoryBus.get());

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

    if (m_parts.apple3 != nullptr)
    {
        m_parts.apple3->GetStateParts (outParts);
    }

    for (const std::unique_ptr<MemoryDevice> & owned : m_parts.ownedDevices)
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
    const CxxxRomRouter  * router = (m_parts.mmu != nullptr) ? m_parts.mmu->GetCxxxRouter() : nullptr;



    return m_romIdentity.Get (m_parts.ownedDevices, router, m_parts.apple2cRomBank.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::ApplyPowerOnOverrides
//
//  Bytes whose power-on contents software reads before writing, and which the
//  fill must not set.
//
//  $03F2-$03F4: the autostart ROM treats a reset as warm, and jumps through
//  the soft-entry vector at $03F2, when the power-up byte at $03F4 equals the
//  vector's high byte XOR $A5. Zeroing all three fails that check, so a power
//  cycle always cold-boots. Without it, a power-on fill that happened to pass
//  sent the ROM into garbage with the screen never cleared (GH #157).
//
//  $4E/$4F: the monitor's random seed, counted up while the ROM waits for a
//  key. A disk that autostarts never waits, so a program that seeds from it
//  reads the fill, and the pattern puts 00 00 here. Pooyan loops forever on a
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



    MemoryBus  & bus     = *m_parts.memoryBus;
    Prng       & prng    = *m_parts.prng;
    Word         address = 0;



    for (address = kSoftEntryLo; address <= kPowerUpByte; address++)
    {
        bus.WriteByte (address, 0);
    }

    bus.WriteByte (kRandomSeedLo, static_cast<Byte> (kNonzeroBit | prng.NextByte()));
    bus.WriteByte (kRandomSeedHi, static_cast<Byte> (kNonzeroBit | prng.NextByte()));
}

#include "Pch.h"

#include "Shell/MachineHost.h"
#include "CassoLink/RunningCassoRegistry.h"

#include "Core/Prng.h"
#include "Devices/RamDevice.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/VideoScanner.h"
#include "Devices/Acia6551.h"
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
#include "Machines/Apple2/Common/SiriusJoyport.h"
#include "Machines/Apple2/Common/UthernetCard.h"
#include "Devices/Acia6551.h"





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
    m_tapeDeck  (std::make_unique<TapeDeck>())
{
    m_memoryBus->SetFloatingBusSource ([this]() { return ReadFloatingBus(); });

    //  The store frees an image on an eject or a remount, so the controller
    //  is re-pointed on every bay change rather than by each caller.
    m_diskStore->SetBayChangeSink ([this] (int slot, int drive, BayChange)
    {
        ConnectDiskToController (slot, drive);
    });
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
    Byte  cycles = 0;



    if (m_cpu == nullptr)
    {
        return (0);
    }

    // StepOne polls the interrupt lines itself and dispatches a pending
    // NMI/IRQ vector in place of the opcode fetch, reporting the cost through
    // GetLastInstructionCycles either way -- so a bare StepOne is the whole
    // step, and a separate interrupt poll would be a second, redundant one.
    m_cpu->StepOne();

    cycles = m_cpu->GetLastInstructionCycles();
    m_cpu->AddCycles (cycles);

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

    for (AciaBridgeEndpoint * bridge : m_serialBridges)
    {
        if (bridge != nullptr)
        {
            bridge->Tick (cycles);
        }
    }

    if (m_refs.uthernet != nullptr)
    {
        m_refs.uthernet->Tick (cycles);
    }

    return (cycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::SetSerialPort
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::SetSerialPort (int port, Acia6551 * acia, bool onCard)
{
    if (port >= 1 && port <= s_kSerialPortCount)
    {
        m_serialPorts[(size_t) port]  = acia;
        m_serialOnCard[(size_t) port] = onCard;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::ConnectSerialPort
//
//  The port loops back before the new device is opened, so after a device
//  that fails to open, the port is a //c's with nothing plugged in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::ConnectSerialPort (int port, const std::string & device, IByteStreamFactory * factory, SerialTrafficLog * log)
{
    HRESULT                       hr     = S_OK;
    bool                          known  = port >= 1 && port <= s_kSerialPortCount;
    Acia6551                    * acia   = known ? m_serialPorts[(size_t) port] : nullptr;
    SerialPortDevice              parsed;
    std::unique_ptr<IByteStream>  stream;
    AciaBridgeEndpoint          * bridge = nullptr;



    CBRAEx (known, E_INVALIDARG);
    CBREx (acia != nullptr, HRESULT_FROM_WIN32 (ERROR_DEV_NOT_EXIST));

    InstallSerialEndpoint (port, m_serialOnCard[(size_t) port] ? nullptr : std::make_unique<AciaLoopbackEndpoint> (acia), nullptr);

    if (m_runningRegistry != nullptr)
    {
        m_runningRegistry->Withdraw (GetCurrentProcessId(), port);
    }

    hr = SerialPortDevice::Parse (device, parsed);
    CHR (hr);
    BAIL_OUT_IF (parsed.kind == SerialPortDevice::Kind::Loopback, S_OK);
    CBREx (factory != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    if (parsed.kind == SerialPortDevice::Kind::Tcp)
    {
        hr = factory->ListenTcp (parsed.tcpPort, LinkTransport::Emulated, stream);
    }
    else
    {
        hr = factory->OpenComPort (parsed.comPort, ComSettings { parsed.baud }, stream);
    }

    CHR (hr);

    hr = AttachSerialStream (port, std::shared_ptr<IByteStream> (std::move (stream)), &bridge);
    CHR (hr);

    //  Casso Explorer finds the port by its file; one that cannot be written
    //  does not stop the port; the port is only unlisted.
    if (m_runningRegistry != nullptr && parsed.kind == SerialPortDevice::Kind::Tcp)
    {
        HRESULT  published = m_runningRegistry->Publish (GetCurrentProcessId(), port, m_config->name, parsed.tcpPort, m_serialOnCard[(size_t) port]);

        IGNORE_RETURN_VALUE (published, S_OK);
    }

    if (log != nullptr)
    {
        bridge->SetByteLog ([log, port] (bool fromGuest, Byte value, uint64_t cycle)
        {
            log->Append (port, fromGuest, value, cycle);
        });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::AttachSerialStream
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::AttachSerialStream (int port, std::shared_ptr<IByteStream> stream, AciaBridgeEndpoint ** outBridge)
{
    HRESULT                              hr     = S_OK;
    Acia6551                           * acia   = nullptr;
    std::unique_ptr<AciaBridgeEndpoint>  bridge;
    AciaBridgeEndpoint                 * raw    = nullptr;



    CBRAEx (port >= 1 && port <= s_kSerialPortCount, E_INVALIDARG);
    CBRAEx (stream != nullptr, E_INVALIDARG);

    acia = m_serialPorts[(size_t) port];
    CBREx (acia != nullptr, HRESULT_FROM_WIN32 (ERROR_DEV_NOT_EXIST));

    bridge = std::make_unique<AciaBridgeEndpoint> (acia, std::move (stream));
    raw    = bridge.get();

    InstallSerialEndpoint (port, std::move (bridge), raw);

    if (outBridge != nullptr)
    {
        *outBridge = raw;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::InstallSerialEndpoint
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::InstallSerialEndpoint (int port, std::unique_ptr<IAciaEndpoint> endpoint, AciaBridgeEndpoint * bridge)
{
    AciaBridgeEndpoint  * old = m_serialBridges[(size_t) port];



    if (old != nullptr && old->GetStream() != nullptr)
    {
        old->GetStream()->Close();
    }

    m_serialPorts[(size_t) port]->SetEndpoint (endpoint.get());

    m_serialBridges[(size_t) port]   = bridge;
    m_serialEndpoints[(size_t) port] = std::move (endpoint);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::DetachSerialPorts
//
//  Before the devices go, so no bridge is ticked against a port that no
//  longer exists. Each stream is closed, which releases its TCP or COM port.
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::DetachSerialPorts()
{
    for (int port = 1; port <= s_kSerialPortCount; port++)
    {
        AciaBridgeEndpoint  * bridge = m_serialBridges[(size_t) port];
        Acia6551            * acia   = m_serialPorts[(size_t) port];

        if (bridge != nullptr && bridge->GetStream() != nullptr)
        {
            bridge->GetStream()->Close();
        }

        if (acia != nullptr)
        {
            acia->SetEndpoint (nullptr);
        }

    }

    WithdrawSerialPorts();

    m_serialPorts     = {};
    m_serialBridges   = {};
    m_serialEndpoints = {};
    m_serialOnCard    = {};
    m_ownedAciaEndpoints.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::WithdrawSerialPorts
//
////////////////////////////////////////////////////////////////////////////////

void MachineHost::WithdrawSerialPorts()
{
    for (int port = 1; port <= s_kSerialPortCount && m_runningRegistry != nullptr; port++)
    {
        m_runningRegistry->Withdraw (GetCurrentProcessId(), port);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::ReadFloatingBus
//
//  The byte the video scanner is reading now, from main RAM as the display
//  scans it whatever the CPU's banking. A machine with no video timing or no
//  display switches, as in tests that build none, returns the last byte on
//  the bus. The timing advances once per instruction, so the position is the
//  instruction's first cycle rather than the cycle of the read.
//
////////////////////////////////////////////////////////////////////////////////

Byte MachineHost::ReadFloatingBus()
{
    AppleSoftSwitchBank  * switches = m_refs.softSwitches;
    Byte                 * mainRam  = (m_mmu != nullptr && m_refs.mainRamDev != nullptr) ? m_refs.mainRamDev->GetData() : nullptr;
    Byte                   value    = m_memoryBus->GetFloatingBusValue();
    VideoScanMode          mode;
    Word                   address  = 0;



    if (m_videoTiming != nullptr && switches != nullptr)
    {
        mode.graphics = switches->IsGraphicsMode();
        mode.mixed    = switches->IsMixedMode();
        mode.page2    = switches->IsPage2();
        mode.hires    = switches->IsHiresMode();
        mode.store80  = m_refs.iieSoftSwitches != nullptr && m_refs.iieSoftSwitches->Is80Store();

        address = VideoScanner::GetScannedAddress (m_videoTiming->GetCycleInFrame(), mode);
        value   = (mainRam != nullptr) ? mainRam[address] : m_memoryBus->ReadByte (address);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::RunCycles
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineHost::RunCycles (uint64_t cycleBudget)
{
    uint64_t  spent = 0;



    if (m_cpu == nullptr)
    {
        return (0);
    }

    while (spent < cycleBudget)
    {
        spent += StepOne();
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
    HRESULT  hrFlush = S_OK;



    // A reset stops the recorder, as pressing reset mid-load would leave a real
    // one running into a guest that is no longer listening. The tape stays in.
    StopTape();

    // Dirty disks are flushed through the store before the controller's reset
    // flushes them itself. A store flush updates the file's size and time in
    // the bay's identity; a write by the controller alone left them stale, so
    // the file was taken as changed by another program and the next flush
    // saved the guest's disk as a spare copy beside it.
    if (m_diskStore != nullptr)
    {
        hrFlush = m_diskStore->FlushAll();
        IGNORE_RETURN_VALUE (hrFlush, S_OK);
    }

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

    StopTape();

    // Auto-flush dirty disks before reseeding device state so writes don't
    // get lost across a power cycle. Mounts persist (matching
    // DiskImageStore::SoftReset semantics -- see the comment block on
    // DiskImageStore::PowerCycle, which is the unmount-everything variant
    // tests can opt into directly).
    hrFlush = m_diskStore->FlushAll();
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





#include "Pch.h"

#include "MachineManager.h"

#include "../EmulatorShell.h"
#include "../AssetBootstrap.h"
#include "../DiskSettings.h"
#include "../resource.h"
#include "../Config/MonitorCatalog.h"
#include "Core/PathResolver.h"
#include "Core/MachineConfig.h"
#include "Core/CpuFactory.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/Prng.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Devices/AppleKeyboard.h"
#include "Devices/Apple2eKeyboard.h"
#include "Devices/AppleSoftSwitchBank.h"
#include "Devices/AppleGamePort.h"
#include "Devices/Apple2eSoftSwitchBank.h"
#include "Devices/AppleSpeaker.h"
#include "Devices/Disk2Controller.h"
#include "Devices/Acia6551.h"
#include "Devices/AciaEndpoints.h"
#include "Devices/Mockingboard/MockingboardCard.h"
#include "Devices/LanguageCard.h"
#include "Devices/Apple2eMmu.h"
#include "Devices/Apple2cRomBank.h"
#include "Devices/AppleMouse.h"
#include "Devices/Printer/ParallelFirmware.h"
#include "Devices/Printer/PrinterCard.h"
#include "Devices/Printer/PrintRaster.h"
#include "Print/PrintJobStore.h"
#include "Video/AppleTextMode.h"
#include "Video/Apple80ColTextMode.h"
#include "Video/AppleLoResMode.h"
#include "Video/AppleHiResMode.h"
#include "Video/AppleDoubleHiResMode.h"
#include "Audio/DriveAudioMixer.h"
#include "Audio/Disk2AudioSource.h"
#include "Shell/CpuManager.h"
#include "Shell/DiskManager.h"
#include "../Ui/Disk2DebugPanel.h"
#include "../Ui/InputDebugPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveMachineSpeedCommand
//
//  Digs the saved speed mode out of the merged config and maps it to the
//  IDM_* the command router expects. 0 means "no saved preference", which
//  every step below can produce: no object, no $cassoUiPrefs, no
//  speedMode key, or a value this build does not recognize.
//
////////////////////////////////////////////////////////////////////////////////

WORD  MachineManager::ResolveMachineSpeedCommand (const JsonValue & mergedJson)
{
    HRESULT            hr      = S_OK;
    const JsonValue *  uiPrefs = nullptr;
    std::string        speed;
    WORD               command = 0;



    if (mergedJson.GetType() == JsonType::Object)
    {
        hr = mergedJson.GetObject ("$cassoUiPrefs", uiPrefs);
    }

    if (SUCCEEDED (hr) && uiPrefs != nullptr)
    {
        _Analysis_assume_ (uiPrefs != nullptr);

        hr = uiPrefs->GetString ("speedMode", speed);
    }

    if (SUCCEEDED (hr))
    {
        if      (speed == "authentic") { command = IDM_MACHINE_SPEED_1X;  }
        else if (speed == "double")    { command = IDM_MACHINE_SPEED_2X;  }
        else if (speed == "maximum")   { command = IDM_MACHINE_SPEED_MAX; }
    }

    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineManager
//
////////////////////////////////////////////////////////////////////////////////

MachineManager::MachineManager (EmulatorShell & shell)
    : m_shell (shell)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateMemoryDevices
//
//  Builds the machine's address space from its config: character ROM, RAM
//  regions, system ROM, soft switches, and the slot cards.
//
//  Devices are added to the bus AND kept in an owned list, because the bus
//  holds raw pointers -- it is a routing table, not an owner -- so the owned
//  list is what keeps them alive and what a machine switch tears down.
//
//  Aux-bank RAM regions are SKIPPED here. The Apple2eMmu owns the auxiliary
//  64 KiB internally and re-points pages at it, so adding a bus device for the
//  same addresses would put two claimants on one range. The first main region
//  is remembered separately, since the MMU page-table wiring needs to name it.
//
//  A missing or unreadable character ROM falls back to the embedded font
//  rather than failing. Text is how the machine tells the user anything,
//  including that something is wrong, so a machine that boots with the wrong
//  font is far more useful than one that will not boot at all.
//
//  System ROM has two shapes. A flat image (//e and earlier) maps directly. A
//  banked image (//c) has its BANK 0 added here as an ordinary flat
//  $C000-$FFFF device, specifically so the normal WireLanguageCard split
//  applies unchanged; the $C028 bank flip is layered on afterwards by
//  WireApple2cRomBank. Building the banking into this step would fork the
//  language-card wiring for one machine.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineManager::CreateMemoryDevices (const MachineConfig & config)
{
    HRESULT  hr      = S_OK;
    bool     romOk   = false;



    std::wstring  wideError;
    std::string   error;



    // Load character generator ROM (used by video renderers, not on bus)
    if (!config.characterRom.resolvedPath.empty())
    {
        HRESULT hrChar = m_shell.m_charRom.LoadFromFile (config.characterRom.resolvedPath);

        if (FAILED (hrChar))
        {
            DEBUGMSG (L"Failed to load character ROM '%hs', using fallback\n",
                      config.characterRom.resolvedPath.c_str());
            m_shell.m_charRom.LoadEmbeddedFallback();
        }
    }
    else
    {
        m_shell.m_charRom.LoadEmbeddedFallback();
    }

    // RAM regions. Skip aux-bank entries: the Apple2eMmu owns the
    // auxiliary 64 KiB internally. Track the main RAM RamDevice for
    // MMU page-table wiring.
    for (const auto & region : config.ram)
    {
        Word                        start  = 0;
        Word                        end    = 0;
        std::unique_ptr<RamDevice>  device;

        if (!region.bank.empty())
        {
            continue;
        }

        start = region.address;
        end = static_cast<Word> (region.address + region.size - 1);

        device = std::make_unique<RamDevice> (start, end);

        if (m_shell.m_refs.mainRamDev == nullptr)
        {
            m_shell.m_refs.mainRamDev = device.get();
        }

        m_shell.m_memoryBus.AddDevice (device.get());
        m_shell.m_ownedDevices.push_back (std::move (device));
    }

    // System ROM. Two shapes:
    //   - Flat (//e and earlier): one image mapped at systemRom.address.
    //   - Banked (//c): a multi-bank file whose active bank is toggled at
    //     runtime. Bank 0 is added here as a flat $C000-$FFFF image so the
    //     normal WireLanguageCard split (LC + CxxxRomRouter) applies; the
    //     Apple2cRomBank is layered on afterward (WireApple2cRomBank) to
    //     enable the $C028 flip.
    if (config.systemRom.romBankSize != 0)
    {
        std::vector<Byte>  fileBytes;
        Word               romStart  = 0;
        Word               romEnd    = 0;

        hr = ReadRomFileBytes (config.systemRom.resolvedPath, fileBytes);

        if (FAILED (hr) || fileBytes.size() < config.systemRom.romBankSize)
        {
            wideError = L"Cannot read banked system ROM: " +
                        std::wstring (config.systemRom.resolvedPath.begin(),
                                      config.systemRom.resolvedPath.end());
            CBRN (false, wideError.c_str());
        }

        romStart = config.systemRom.address;
        romEnd = static_cast<Word> (config.systemRom.address + config.systemRom.romBankSize - 1);

        auto device = RomDevice::CreateFromData (romStart, romEnd,
                                                 fileBytes.data(),
                                                 config.systemRom.romBankSize);

        m_shell.m_memoryBus.AddDevice (device.get());
        m_shell.m_ownedDevices.push_back (std::move (device));
    }
    else
    {
        Word romStart = config.systemRom.address;
        Word romEnd   = static_cast<Word> (config.systemRom.address + config.systemRom.fileSize - 1);

        auto device = RomDevice::CreateFromFile (romStart,
                                                 romEnd,
                                                 config.systemRom.resolvedPath,
                                                 error);

        romOk = (device != nullptr);

        if (!romOk)
        {
            wideError.assign (error.begin(), error.end());
            CBRN (false, wideError.c_str());
        }

        m_shell.m_memoryBus.AddDevice (device.get());
        m_shell.m_ownedDevices.push_back (std::move (device));
    }

    // Internal motherboard devices
    for (const auto & idev : config.internalDevices)
    {
        DeviceConfig                   devCfg;
        std::unique_ptr<MemoryDevice>  device;
        devCfg.type = idev.type;

        // The //e MMU is a coordinator object, not a bus device -- it
        // owns the auxiliary 64 KiB and rebinds the page table on every
        // banking-changed event. Instantiate it directly here; full
        // wiring (siblings, Initialize) happens after the device pass.
        if (devCfg.type == "apple2e-mmu")
        {
            m_shell.m_mmu = std::make_unique<Apple2eMmu>();
            continue;
        }

        device = m_shell.m_registry.Create (devCfg.type, devCfg, m_shell.m_memoryBus);

        if (!device)
        {
            DEBUGMSG (L"Warning: Unknown device type '%hs'\n", devCfg.type.c_str());
            continue;
        }

        // Track specific device pointers for quick access
        if (devCfg.type == "apple2-keyboard" ||
            devCfg.type == "apple2e-keyboard")
        {
            m_shell.m_refs.keyboard = static_cast<AppleKeyboard *> (device.get());

            // Resolve the derived //e pointer here, where the configured
            // device type already says which keyboard was built, rather than
            // dynamic_cast-ing it back out of the base pointer at each use.
            if (devCfg.type == "apple2e-keyboard")
            {
                m_shell.m_refs.iieKeyboard =
                    static_cast<Apple2eKeyboard *> (m_shell.m_refs.keyboard);
            }
        }
        else if (devCfg.type == "apple2-softswitches" ||
                 devCfg.type == "apple2e-softswitches")
        {
            m_shell.m_refs.softSwitches = static_cast<AppleSoftSwitchBank *> (device.get());

            // Resolve the derived //e pointer here, where the configured
            // device type already says which bank was built, rather than
            // dynamic_cast-ing it back out of the base pointer at each use.
            if (devCfg.type == "apple2e-softswitches")
            {
                m_shell.m_refs.iieSoftSwitches =
                    static_cast<Apple2eSoftSwitchBank *> (m_shell.m_refs.softSwitches);
            }
        }
        else if (devCfg.type == "apple2-gameport")
        {
            m_shell.m_refs.gamePort = static_cast<AppleGamePort *> (device.get());
        }
        else if (devCfg.type == "apple2-speaker")
        {
            m_shell.m_refs.speaker = static_cast<AppleSpeaker *> (device.get());
        }

        m_shell.m_memoryBus.AddDevice (device.get());
        m_shell.m_ownedDevices.push_back (std::move (device));
    }

    // Wire IIe keyboard <-> softswitch sibling so $C00C-$C00F reaches
    // the softswitch (the keyboard's range $C000-$C063 would otherwise
    // eat it).
    {
        auto * iieKbd = m_shell.m_refs.iieKeyboard;
        auto * iieSw  = m_shell.m_refs.iieSoftSwitches;

        // One test per device, with everything that device owns wired inside
        // it. The sibling link needs both, so it nests rather than adding a
        // second test of either -- the machines that have one of these have
        // the other, but nothing here depends on that.
        if (iieKbd != nullptr)
        {
            if (iieSw != nullptr)
            {
                iieKbd->SetSoftSwitchSibling (iieSw);
                iieSw->SetKeyboard           (iieKbd);
            }

            if (m_shell.m_refs.speaker != nullptr)
            {
                iieKbd->SetSpeakerSibling (m_shell.m_refs.speaker);
            }

            if (m_shell.m_mmu != nullptr)
            {
                iieKbd->SetMmu (m_shell.m_mmu.get());
            }

            if (m_shell.m_videoTiming != nullptr)
            {
                iieKbd->SetVideoTiming (m_shell.m_videoTiming.get());
            }
        }

        if (iieSw != nullptr)
        {
            if (m_shell.m_videoTiming != nullptr)
            {
                iieSw->SetVideoTiming (m_shell.m_videoTiming.get());
            }

            if (m_shell.m_mmu != nullptr)
            {
                iieSw->SetMmu (m_shell.m_mmu.get());
            }
        }
    }

    // Initialize the //e MMU once main RAM exists. The MMU rebinds the
    // page table for $0000-$BFFF based on RAMRD/RAMWRT/ALTZP/80STORE.
    if (m_shell.m_mmu != nullptr && m_shell.m_refs.mainRamDev != nullptr)
    {
        HRESULT hrMmu = m_shell.m_mmu->Initialize (
            &m_shell.m_memoryBus,
            m_shell.m_refs.mainRamDev,
            nullptr,
            nullptr,
            nullptr,
            m_shell.m_refs.iieSoftSwitches);

        if (FAILED (hrMmu))
        {
            DEBUGMSG (L"Apple2eMmu::Initialize failed (hr=0x%08x)\n", hrMmu);
        }
    }

    // Slot devices and slot ROMs
    for (const auto & slot : config.slots)
    {
        // A slot the user disabled in Settings > Hardware installs neither its
        // device nor its slot ROM -- e.g. removing the slot-6 Disk II
        // controller actually stops the machine from booting off floppy.
        if (!slot.enabled)
        {
            continue;
        }

        // Slot device (e.g., disk-ii)
        if (!slot.device.empty())
        {
            DeviceConfig                   devCfg;
            std::unique_ptr<MemoryDevice>  device;
            devCfg.type    = slot.device;
            devCfg.slot    = slot.slot;
            devCfg.hasSlot = true;

            device = m_shell.m_registry.Create (devCfg.type, devCfg, m_shell.m_memoryBus);

            if (!device)
            {
                DEBUGMSG (L"Warning: Unknown slot device type '%hs'\n", devCfg.type.c_str());
            }
            else
            {
                // Cache the printer card so the background drain worker can
                // reach its ring once the machine is built.
                if (slot.device == "parallel-printer")
                {
                    m_shell.m_refs.printerCard = static_cast<PrinterCard *> (device.get());
                }

                m_shell.m_memoryBus.AddDevice (device.get());
                m_shell.m_ownedDevices.push_back (std::move (device));
            }
        }

        // The parallel printer card ships embedded firmware (no rom file), so
        // its slot ROM is installed here from the checked-in byte array rather
        // than loaded from disk.
        if (slot.device == "parallel-printer")
        {
            std::vector<Byte>  firmware (s_kParallelFirmwareBytes,
                                         s_kParallelFirmwareBytes + sizeof (s_kParallelFirmwareBytes));

            if (m_shell.m_mmu != nullptr)
            {
                // //e: the MMU's $C100-$CFFF router owns the page (INTCXROM /
                // SLOTCXROM switching) and pads the short firmware to a full
                // page with the floating-bus byte.
                m_shell.m_mmu->AttachSlotRom (slot.slot, std::move (firmware));
            }
            else
            {
                // ][/][+: no INTCXROM router, so the firmware is bus-resident at
                // $Cs00 exactly like the disk-ii slot ROM -- WITHOUT this the card
                // is present but PR#s jumps into an empty page and nothing prints.
                // Pad the short firmware to a full page with the floating-bus byte
                // so the RomDevice spans the whole $Cs00-$CsFF page.
                constexpr Byte   kFloatFill = 0xFF;

                Word   romStart = static_cast<Word> (0xC000 + slot.slot * 0x100);
                Word   romEnd   = static_cast<Word> (romStart + 0xFF);

                firmware.resize (0x100, kFloatFill);

                auto device = RomDevice::CreateFromData (romStart, romEnd,
                                                         firmware.data(), firmware.size());

                m_shell.m_memoryBus.AddDevice (device.get());
                m_shell.m_ownedDevices.push_back (std::move (device));
            }
        }

        // Slot ROM at $Cs00-$CsFF
        if (!slot.rom.empty())
        {
            Word romStart = static_cast<Word> (0xC000 + slot.slot * 0x100);
            Word romEnd   = static_cast<Word> (romStart + slot.romSize - 1);

            auto device = RomDevice::CreateFromFile (romStart,
                                                     romEnd,
                                                     slot.resolvedRomPath,
                                                     error);

            if (device == nullptr)
            {
                wideError.assign (error.begin(), error.end());
                CBRN (false, wideError.c_str());
            }

            // On //e the Apple2eMmu owns the $C100-$CFFF router and
            // dispatches between internal ROM and slot ROMs based on
            // INTCXROM/SLOTC3ROM/INTC8ROM. On ][/][+, the slot ROM is
            // bus-resident as before (no INTCXROM concept).
            if (m_shell.m_mmu != nullptr)
            {
                std::vector<Byte>  bytes (slot.romSize);

                for (size_t i = 0; i < slot.romSize; i++)
                {
                    bytes[i] = device->Read (static_cast<Word> (romStart + i));
                }

                m_shell.m_mmu->AttachSlotRom (slot.slot, std::move (bytes));
            }
            else
            {
                m_shell.m_memoryBus.AddDevice (device.get());
            }

            m_shell.m_ownedDevices.push_back (std::move (device));
        }
    }

    // Apple //c: the built-in 5.25" drive is an IWM at slot 6 ($C0E0-$C0EF).
    // Unlike the //e it is not a card in a slot, so it is created here rather
    // than from the config's (empty) slot list. Its $C600 boot firmware is part
    // of the internal //c ROM (served by the no-slots CxxxRomRouter set in
    // WireApple2cRomBank), so no slot ROM is attached -- only the controller,
    // in IWM mode so the reset firmware's mode/status probe passes.
    // //c IOU mouse: destroyed with the outgoing machine, rebuilt
    // below for the //c. The keyboard/soft-switch bank holding the old
    // pointer are torn down with the same machine, and the CPU thread is
    // stopped during construction, so no stale-pointer window exists.
    m_shell.m_mouse.reset();

    if (m_shell.m_config.systemRom.romBankSize != 0)
    {
        auto iwm = std::make_unique<Disk2Controller> (6);
        iwm->SetIwmMode (true);
        m_shell.m_memoryBus.AddDevice (iwm.get());
        m_shell.m_ownedDevices.push_back (std::move (iwm));

        // //c built-in IOU mouse: not a bus device -- the keyboard
        // ($C048 ack, $C063 button) and soft-switch bank ($C015/$C017/$C019
        // status, $C066/$C067 direction, $C058-$C05F IOU programming,
        // $C078/$C079 gate, $C070 VBL clear) forward its register surface;
        // the real ROM 4 mouse firmware (phantom slot 7) runs against it.
        // IRQ lines aggregate through the shared interrupt controller; the
        // CPU cycle fan-out tick is wired in CreateCpu.
        {
            HRESULT                  hrIc   = S_OK;
            Apple2eKeyboard        * iieKbd = nullptr;
            Apple2eSoftSwitchBank  * iieSw  = nullptr;

            m_shell.m_mouse = std::make_unique<AppleMouse> ();

            hrIc = m_shell.m_mouse->AttachInterruptController (&m_shell.m_interruptController);
            IGNORE_RETURN_VALUE (hrIc, S_OK);

            m_shell.m_mouse->SetBus (&m_shell.m_memoryBus);

            if (m_shell.m_videoTiming != nullptr)
            {
                m_shell.m_mouse->SetVideoTiming (m_shell.m_videoTiming.get());
            }

            iieKbd = m_shell.m_refs.iieKeyboard;
            iieSw = m_shell.m_refs.iieSoftSwitches;

            if (iieKbd != nullptr)
            {
                iieKbd->SetMouse (m_shell.m_mouse.get());

                // Enable the //c case switches ($C060 80/40 read + the
                // keyboard-layout remap). Dormant on the //e.
                iieKbd->SetApple2cMode (true);
            }

            if (iieSw != nullptr)
            {
                iieSw->SetMouse (m_shell.m_mouse.get());
            }
        }

        // //c dual 6551 ACIA serial ports (phantom slots 1 & 2): port 1
        // ($C098) = printer, port 2 ($C0A8) = modem. Built in like the IWM
        // (the //c has no config slots) -- the serial firmware is part of the
        // internal //c ROM. Each raises IRQs through the shared interrupt
        // controller. v1 endpoints are loopback (comms self-test); the serial
        // printer-endpoint bridge + Hardware-tab endpoint selector are
        // downstream work.
        for (int slot = 1; slot <= 2; ++slot)
        {
            HRESULT                                hrIc     = S_OK;
            std::unique_ptr<Acia6551>              acia;
            std::unique_ptr<AciaLoopbackEndpoint>  loopback;

            Word  base = static_cast<Word> (Acia6551::kSlotIoBase
                                            + slot * Acia6551::kSlotIoStride
                                            + Acia6551::kAciaRegOffset);
            acia = std::make_unique<Acia6551> (base);

            hrIc = acia->AttachInterruptController (&m_shell.m_interruptController);
            IGNORE_RETURN_VALUE (hrIc, S_OK);

            loopback = std::make_unique<AciaLoopbackEndpoint> (acia.get());
            acia->SetEndpoint (loopback.get());

            m_shell.m_memoryBus.AddDevice (acia.get());
            m_shell.m_ownedAciaEndpoints.push_back (std::move (loopback));
            m_shell.m_ownedDevices.push_back (std::move (acia));
        }
    }

    // Cache Disk2Controller pointer for the status-bar drive activity
    // indicator. We pick the first one we find (typically slot 6).
    m_shell.m_refs.diskController = nullptr;
    for (auto & dev : m_shell.m_ownedDevices)
    {
        Disk2Controller *  dc = dynamic_cast<Disk2Controller *> (dev.get());

        if (dc != nullptr)
        {
            m_shell.m_refs.diskController = dc;
            break;
        }
    }

    // Drive-audio wiring (spec 005-disk-ii-audio FR-008 / FR-012 /
    // FR-015 / FR-016). Allocate one Disk2AudioSource per drive, register
    // each with the mixer, and route the controller's audio-sink events into
    // drive 0's source (single sink covers both drives; the head / motor
    // events themselves are not currently drive-tagged in Disk2Controller --
    // a follow-up could split per-drive sinks).
    //
    // The sources are created UNCONDITIONALLY -- even for a machine with no
    // (realized) Disk ][ controller -- so the settings drive-sound preview
    // (#84 Phase C) still auditions when the user has toggled slot 6 on in
    // settings but not yet committed the controller into the running machine.
    // Without a controller the sources simply receive no real head / motor
    // events; the mixer mixes them as silence until a test sound is fired.
    //
    // Pan policy: each drive's stereo position comes from the shell's
    // stored per-drive pan (user-adjustable). Defaults place Drive 1
    // left-of-center and Drive 2 right-of-center (kDefaultDriveOnePan /
    // kDefaultDriveTwoPan).
    m_shell.m_diskAudioSources.clear();
    m_shell.m_driveAudioMixer.UnregisterAllSources();

    {
        int  driveCount = Disk2Controller::kDriveCount;
        int  drive      = 0;

        for (drive = 0; drive < driveCount; drive++)
        {
            auto   src  = std::make_unique<Disk2AudioSource>();
            float  panL = DriveAudioMixer::kSpeakerCenter;
            float  panR = DriveAudioMixer::kSpeakerCenter;

            // Per-drive stereo position from the shell's stored pan
            // (user-adjustable; defaults place Drive 1 left-of-center and
            // Drive 2 right-of-center). drive index is clamped to the
            // stored-pan array bound.
            DriveAudioMixer::PanToStereo (m_shell.m_drivePan[drive], panL, panR);
            src->SetPan (panL, panR);

            m_shell.m_driveAudioMixer.RegisterSource (src.get());
            src->SetDriveIndex (drive);
            src->SetVolumes (m_shell.m_driveMotorVolume,
                             m_shell.m_driveHeadVolume,
                             m_shell.m_driveDoorVolume);
            m_shell.m_diskAudioSources.push_back (std::move (src));
        }
    }

    // The emulated ImageWriter shares the generic drive-audio bus (FR-016). It
    // is a single persistent shell-owned source; the UnregisterAllSources above
    // dropped it along with the disk sources, so re-register it here on every
    // machine build. Silent until the live preview publishes a paced reveal.
    m_shell.m_driveAudioMixer.RegisterSource (&m_shell.m_printerAudio);

    // Feed real disk head / motor / door events to drive 0's source only when
    // the machine actually has the Disk ][ controller realized.
    if (m_shell.m_refs.diskController != nullptr && !m_shell.m_diskAudioSources.empty())
    {
        m_shell.m_refs.diskController->SetAudioSink (m_shell.m_diskAudioSources[0].get());
    }

    // Start the background printer drain once the card exists, seeding it with
    // this machine's persisted pending strip if one exists (FR-026). A missing
    // or corrupt sidecar falls back to empty paper. Symmetric save is in
    // SwitchMachine teardown and OnDestroy.
    if (m_shell.m_refs.printerCard != nullptr)
    {
        PrintRaster   pending;
        HRESULT       hrLoad = PrintJobStore::Load (m_shell.GetPendingPrintDir(), pending);

        m_shell.m_printerWorker.Start (
            m_shell.m_refs.printerCard->GetByteRing(),
            SUCCEEDED (hrLoad) ? std::move (pending) : PrintRaster());

        // Pace the drain off the guest clock so the card applies real
        // backpressure -- the guest prints at ImageWriter speed (faster at max
        // perf), never racing ahead of the preview. The cycle pointer is stable
        // for this machine's CPU, so setting it once covers later restarts.
        if (m_shell.m_cpu != nullptr)
        {
            m_shell.m_printerWorker.SetCycleClock (m_shell.m_cpu->GetCycleCounterPtr());
        }

        // Prime the live-preview auto-open baseline to the worker's current
        // activity so a page carried over from a previous session does not read as
        // a fresh print and auto-open the preview on boot -- only new printing does.
        m_shell.m_printerAutoOpenActivity = m_shell.m_printerWorker.GetActivityCount();
    }

    // Mockingboard wiring. Cache the card (if the active config installs
    // one), attach both VIA timer-IRQ sources to the shared controller,
    // and register the two PSG audio sources (PSG #1 hard-left, PSG #2
    // hard-right) with the dedicated Mockingboard mixer. The card owns
    // the sources; the mixer holds borrowed pointers, dropped on the next
    // teardown.
    m_shell.m_refs.mockingboard = nullptr;
    m_shell.m_mockingboardAudioMixer.UnregisterAllSources();

    for (auto & dev : m_shell.m_ownedDevices)
    {
        MockingboardCard *  mb = dynamic_cast<MockingboardCard *> (dev.get());

        if (mb != nullptr)
        {
            m_shell.m_refs.mockingboard = mb;
            break;
        }
    }

    if (m_shell.m_refs.mockingboard != nullptr)
    {
        hr = m_shell.m_refs.mockingboard->AttachInterruptController (&m_shell.m_interruptController);
        CHR (hr);

        m_shell.m_mockingboardAudioMixer.RegisterSource (m_shell.m_refs.mockingboard->GetAudioSource (0));
        m_shell.m_mockingboardAudioMixer.RegisterSource (m_shell.m_refs.mockingboard->GetAudioSource (1));

        // The sound+speech variant adds its center-panned voice source; the
        // sound-only card has no chip and the source stays silent anyway.
        if (m_shell.m_refs.mockingboard->GetSpeech() != nullptr)
        {
            m_shell.m_mockingboardAudioMixer.RegisterSource (
                m_shell.m_refs.mockingboard->GetSpeechAudioSource());
        }

        m_shell.m_refs.mockingboard->SetSampleRate (m_shell.m_wasapiAudio.GetSampleRate());

        // On the //e the Apple2eMmu's CxxxRomRouter owns $C100-$CFFF, so a
        // bus device at $Cn00 would be shadowed. Register the card as the
        // slot's active I/O device so the router delegates that page to it
        // (INTCXROM=0). On ][/][+ there is no MMU and the card is
        // bus-resident.
        if (m_shell.m_mmu != nullptr)
        {
            m_shell.m_mmu->GetCxxxRouter()->SetSlotIoDevice (
                m_shell.m_refs.mockingboard->GetSlot(),
                m_shell.m_refs.mockingboard);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireLanguageCard
//
//  Converts a flat ROM device into the language-card arrangement: the ROM
//  image moves INTO the card, the flat device leaves the bus, and a bank
//  device takes over $D000-$FFFF to route each access to card RAM or ROM
//  according to the soft switches.
//
//  Both halves are found by searching the existing device set rather than
//  being passed in, which keeps this a post-pass over whatever
//  CreateMemoryDevices built -- a machine with no language card, or no ROM
//  covering $D000-$FFFF, simply gets nothing wired and needs no special case.
//
//  Splitting the flat ROM is the delicate part. A ROM image starting below
//  $D000 also covers the slot ROM area, so the part below is re-added
//  SEPARATELY and clamped to start at $C100 -- $C000-$C0FF is I/O space, and
//  shadowing it with ROM would break every soft switch in the machine.
//
//  Where that lower ROM goes depends on the machine. A //e hands it to the
//  MMU's CxxxRomRouter, which arbitrates internal ROM against slot cards; a
//  ][ or ][+ has no such arbitration and keeps a plain bus-resident device.
//
//  The //e cross-links exist because three unrelated things need to see
//  language-card state: the MMU re-points the card's read window on ALTZP
//  flips, and both the keyboard and the soft-switch bank report card status
//  through $C011/$C012.
//
//  RebindWindow is called last to seed the read-page mapping now that the ROM
//  image and the MMU are both in place; after this it re-points on its own for
//  every card switch, reset, and ALTZP flip.
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::WireLanguageCard()
{
    LanguageCard *  lc        = nullptr;
    RomDevice    *  romDevice = nullptr;



    // Find the LanguageCard device
    for (auto & dev : m_shell.m_ownedDevices)
    {
        if (lc == nullptr)
        {
            lc = dynamic_cast<LanguageCard *> (dev.get());
        }
    }

    // Find a ROM device covering $D000-$FFFF. Only looked for once the card
    // exists -- with no card there is nothing to hand the ROM image to.
    if (lc != nullptr)
    {
        for (const auto & entry : m_shell.m_memoryBus.GetEntries())
        {
            auto * rom = dynamic_cast<RomDevice *> (entry.device);

            if (romDevice == nullptr && rom != nullptr && entry.start <= 0xD000 && entry.end >= 0xFFFF)
            {
                romDevice = rom;
            }
        }
    }

    // No card or no covering ROM: this machine has no language card to wire.
    if (romDevice != nullptr)
    {
        Word                                 romStart = romDevice->GetStart();
        std::unique_ptr<LanguageCardBank>    lcBank;
        Apple2eKeyboard                    * iieKbd   = nullptr;
        Apple2eSoftSwitchBank              * iieSw    = nullptr;

        // Copy $D000-$FFFF ROM data to language card
        std::vector<Byte>  lcRomData (0x3000);

        for (size_t i = 0; i < 0x3000; i++)
        {
            lcRomData[i] = romDevice->Read (static_cast<Word> (0xD000 + i));
        }

        lc->SetRomData (lcRomData);
        m_shell.m_memoryBus.RemoveDevice (romDevice);

        // Re-add slot ROM ($C100-$CFFF) if original extended below $D000.
        // $C000-$C0FF is I/O space and must not be shadowed by ROM.
        if (romStart < 0xD000)
        {
            Word   slotRomStart = static_cast<Word> (std::max (static_cast<int> (romStart), 0xC100));
            size_t dataOffset   = slotRomStart - romStart;
            size_t lowerSize    = 0xD000 - slotRomStart;

            UNREFERENCED_PARAMETER (dataOffset);

            std::vector<Byte>  lowerData (lowerSize);

            for (size_t i = 0; i < lowerSize; i++)
            {
                lowerData[i] = romDevice->Read (static_cast<Word> (slotRomStart + i));
            }

            // On //e: hand to the MMU's CxxxRomRouter. On ][/][+: keep the
            // legacy bus-resident ROM device.
            if (m_shell.m_mmu != nullptr)
            {
                m_shell.m_mmu->AttachInternalCxxxRom (std::move (lowerData));
            }
            else
            {
                auto lowerRom = RomDevice::CreateFromData (
                    slotRomStart, static_cast<Word> (0xCFFF),
                    lowerData.data(), lowerData.size());

                m_shell.m_memoryBus.AddDevice (lowerRom.get());
                m_shell.m_ownedDevices.push_back (std::move (lowerRom));
            }
        }

        // Bank device intercepts $D000-$FFFF, routing to LC RAM or ROM
        lcBank = std::make_unique<LanguageCardBank> (*lc);
        m_shell.m_memoryBus.AddDevice (lcBank.get());
        m_shell.m_ownedDevices.push_back (std::move (lcBank));

        // //e wiring: LC needs the MMU (for ALTZP routing) and the
        // keyboard sibling needs the LC pointer for $C011/$C012 status
        // reads.
        if (m_shell.m_mmu != nullptr)
        {
            lc->SetMmu (m_shell.m_mmu.get());

            // Let ALTZP flips re-point the LC's $D000-$FFFF read window (aux/main).
            m_shell.m_mmu->SetLanguageCard (lc);
        }

        iieKbd = m_shell.m_refs.iieKeyboard;

        if (iieKbd != nullptr)
        {
            iieKbd->SetLanguageCard (lc);
        }

        iieSw = m_shell.m_refs.iieSoftSwitches;

        if (iieSw != nullptr)
        {
            iieSw->SetLanguageCard (lc);
        }

        // Seed the $D000-$FFFF read-page mapping now that the ROM image and MMU are
        // wired. Thereafter it re-points on LC switches, reset, and ALTZP flips.
        lc->RebindWindow();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRomFileBytes
//
//  Reads an entire ROM image into memory. Used for the //c's banked ROM,
//  whose 32K file does not fit RomDevice::CreateFromFile's exact-size rule.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineManager::ReadRomFileBytes (const std::string & path, std::vector<Byte> & out)
{
    HRESULT         hr       = S_OK;
    bool            isOpen   = false;
    bool            hasBytes = false;
    bool            wasRead  = false;
    std::streamoff  size     = 0;
    std::ifstream   file (path, std::ios::binary | std::ios::ate);



    isOpen = file.good();
    CBR (isOpen);

    size     = file.tellg();
    hasBytes = (size > 0);
    CBR (hasBytes);

    file.seekg (0, std::ios::beg);
    out.resize (static_cast<size_t> (size));
    file.read (reinterpret_cast<char *> (out.data()), size);

    wasRead = file.good();
    CBR (wasRead);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireApple2cRomBank
//
//  Layers the Apple //c firmware-bank coordinator on top of the language
//  card + CxxxRomRouter that WireLanguageCard already populated from bank 0.
//  SetBankImages re-applies bank 0 (idempotent) and enables the $C028 flip
//  via the soft-switch bank's IRomBankSwitch hook. No-op for flat-ROM
//  machines (the //e and earlier).
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::WireApple2cRomBank()
{
    const RomReference &  sysRom = m_shell.m_config.systemRom;



    Apple2eMmu            * mmu      = m_shell.m_mmu.get();
    Apple2eSoftSwitchBank * sw       = m_shell.m_refs.iieSoftSwitches;
    LanguageCard          * lc       = nullptr;
    std::vector<Byte>       fileBytes;
    size_t                  twoBanks = static_cast<size_t> (sysRom.romBankSize) * 2;
    HRESULT                 hrRead   = S_OK;
    bool                    banked   = (sysRom.romBankSize != 0);

    // romBankSize 0 is a flat-ROM machine (the //e and earlier) -- not a
    // failure, so it gets no diagnostic.
    if (banked)
    {
        for (auto & dev : m_shell.m_ownedDevices)
        {
            if (lc == nullptr)
            {
                lc = dynamic_cast<LanguageCard *> (dev.get());
            }
        }

        banked = (mmu != nullptr && sw != nullptr && lc != nullptr);

        if (!banked)
        {
            DEBUGMSG (L"WireApple2cRomBank: missing MMU/soft-switches/LC; banking disabled\n");
        }
    }

    if (banked)
    {
        hrRead = ReadRomFileBytes (sysRom.resolvedPath, fileBytes);
        banked = SUCCEEDED (hrRead) && fileBytes.size() >= twoBanks;

        if (!banked)
        {
            DEBUGMSG (L"WireApple2cRomBank: cannot read both ROM banks; banking disabled\n");
        }
    }

    if (banked)
    {
        std::vector<Byte>   bank0 (fileBytes.begin(),                     fileBytes.begin() + sysRom.romBankSize);
        std::vector<Byte>   bank1 (fileBytes.begin() + sysRom.romBankSize, fileBytes.begin() + twoBanks);

        m_shell.m_apple2cRomBank = std::make_unique<Apple2cRomBank> (*lc, *mmu);
        m_shell.m_apple2cRomBank->SetBankImages (std::move (bank0), std::move (bank1));
        sw->SetRomBankSwitch (m_shell.m_apple2cRomBank.get());

        // //c: no card slots -> $C100-$CFFF is always the internal firmware.
        mmu->GetCxxxRouter()->SetNoExternalSlots (true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WirePageTable
//
//  Sets up the MemoryBus page table to point each $0000-$BFFF page at
//  the CPU's main RAM buffer (memory[]). This is the baseline
//  mapping; the IIe may later swap pages to aux RAM via 80STORE /
//  PAGE2 banking.
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::WirePageTable()
{
    Byte * mainRam = nullptr;



    if (!m_shell.m_cpu)
    {
        return;
    }

    mainRam = const_cast<Byte *> (m_shell.m_cpu->GetMemory());

    // Map all RAM pages ($0000-$BFFF) to main memory
    for (int page = 0x00; page < 0xC0; page++)
    {
        Byte * pagePtr = mainRam + (page * 0x100);
        m_shell.m_memoryBus.SetReadPage  (page, pagePtr);
        m_shell.m_memoryBus.SetWritePage (page, pagePtr);
    }

    // Register banking-change callback so soft switches can trigger
    // remapping.
    m_shell.m_memoryBus.SetBankingChangedCallback ([this]()
    {
        RebuildBankingPages();
    });

    // Initial state
    RebuildBankingPages();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAuxRamBuffer
//
//  Returns the //e auxiliary 64 KiB buffer (owned by Apple2eMmu) or
//  nullptr when no MMU is wired (Apple ][ / ][+).
//
////////////////////////////////////////////////////////////////////////////////

Byte * MachineManager::GetAuxRamBuffer()
{
    return m_shell.m_mmu != nullptr ? m_shell.m_mmu->GetAuxBuffer() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildBankingPages
//
//  When the //e MMU is present, it owns all $0000-$BFFF page-table
//  routing (RAMRD/RAMWRT/ALTZP/80STORE+PAGE2/HIRES) and is invoked
//  directly by the soft-switch bank on every banking-changed event.
//  This shim only handles the legacy fallback where no MMU exists
//  (][/][+) -- those machines never set 80STORE so all pages stay
//  bound to main RAM.
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::RebuildBankingPages()
{
    Byte *  mainRam = nullptr;
    int     page    = 0;



    // Only the legacy no-MMU path does anything here: with an MMU present it
    // owns every $0000-$BFFF page and this would fight it.
    if (m_shell.m_cpu && m_shell.m_mmu == nullptr)
    {
        mainRam = const_cast<Byte *> (m_shell.m_cpu->GetMemory());

        // Text page 1 ($0400-$07FF) and hi-res page 1 ($2000-$3FFF) -- the
        // two windows 80STORE/PAGE2 would otherwise re-point.
        for (page = 0x04; page <= 0x07; page++)
        {
            m_shell.m_memoryBus.SetReadPage  (page, mainRam + (page * 0x100));
            m_shell.m_memoryBus.SetWritePage (page, mainRam + (page * 0x100));
        }

        for (page = 0x20; page <= 0x3F; page++)
        {
            m_shell.m_memoryBus.SetReadPage  (page, mainRam + (page * 0x100));
            m_shell.m_memoryBus.SetWritePage (page, mainRam + (page * 0x100));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateVideoModes
//
//  Builds all five renderers up front, publishes each one by name in
//  m_refs, and installs 40-column text as the active one.
//
//  Every mode is created regardless of machine, because SelectVideoMode
//  switches between them per frame from the soft-switch state and cannot
//  afford to construct one mid-render.
//
//  m_videoModes owns them; m_refs names them. This is the ONLY function that
//  touches the vector's contents, so its order carries no meaning and adding
//  a mode cannot disturb the existing ones -- which was not true while every
//  caller reached in by index and downcast to whatever it believed that slot
//  held.
//
//  Aux memory is wired into the two modes that read it -- 80-column text and
//  double hi-res -- only when an MMU actually provides it, so a ][+ gets the
//  same objects with nothing aux-backed rather than a shorter list.
//
//  Text mode starts active because that is what a machine displays at power-on
//  before any program selects otherwise.
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::CreateVideoModes()
{
    Byte *                                   auxBuf          = nullptr;
    std::unique_ptr<AppleTextMode>           textMode;
    std::unique_ptr<AppleLoResMode>          loResMode;
    std::unique_ptr<AppleHiResMode>          hiResMode;
    std::unique_ptr<AppleDoubleHiResMode>    doubleHiResMode;
    std::unique_ptr<Apple80ColTextMode>      text80;



    textMode        = std::make_unique<AppleTextMode>        (m_shell.m_memoryBus, m_shell.m_charRom);
    loResMode       = std::make_unique<AppleLoResMode>       (m_shell.m_memoryBus);
    hiResMode       = std::make_unique<AppleHiResMode>       (m_shell.m_memoryBus);
    doubleHiResMode = std::make_unique<AppleDoubleHiResMode> (m_shell.m_memoryBus);
    text80          = std::make_unique<Apple80ColTextMode>   (m_shell.m_memoryBus, m_shell.m_charRom);

    m_shell.m_refs.text40      = textMode.get();
    m_shell.m_refs.loRes       = loResMode.get();
    m_shell.m_refs.hiRes       = hiResMode.get();
    m_shell.m_refs.doubleHiRes = doubleHiResMode.get();
    m_shell.m_refs.text80      = text80.get();

    m_shell.m_refs.activeVideoMode = m_shell.m_refs.text40;

    auxBuf = GetAuxRamBuffer();

    if (auxBuf != nullptr)
    {
        text80->SetAuxMemory          (auxBuf);
        doubleHiResMode->SetAuxMemory (auxBuf);

        // DHR needs BOTH banks at once, so it takes main RAM directly too.
        // The bus cannot serve the main half: its $2000-$3FFF pages follow
        // live banking and point at aux under 80STORE+HIRES+PAGE2, which
        // made DHR render the aux bytes into both halves of every pair.
        // This is the same buffer the MMU treats as main.
        if (m_shell.m_refs.mainRamDev != nullptr)
        {
            doubleHiResMode->SetMainMemory (m_shell.m_refs.mainRamDev->GetData());
        }
    }

    m_shell.m_videoModes.push_back (std::move (textMode));
    m_shell.m_videoModes.push_back (std::move (loResMode));
    m_shell.m_videoModes.push_back (std::move (hiResMode));
    m_shell.m_videoModes.push_back (std::move (doubleHiResMode));
    m_shell.m_videoModes.push_back (std::move (text80));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateCpu
//
//  Builds the CPU the config asks for and wires everything that rides on its
//  cycle fan-out.
//
//  The core is chosen through CpuFactory -- 65C02 for the Enhanced //e and the
//  //c, NMOS 6502 for everything else -- and that seam exists to stop the
//  wrong part being built silently. An unbuildable strategy means a shipped
//  config names a CPU we do not have, which is our bug, so it asserts here
//  while CpuFactory itself merely returns E_INVALIDARG and stays a clean,
//  testable validator.
//
//  Three consumers hang off the per-cycle fan-out, and all three are here
//  because they must stay phase-locked to CPU progress rather than to frames:
//
//    video timing        every AddCycles ticks it, so $C019 (RDVBLBAR) tracks
//                        the 17,030-cycle frame
//    interrupt lines     the //c's mouse VBL / movement and the two ACIAs
//                        assert through the controller; quiet on earlier
//                        machines, but the seam is shared
//    //c mouse           VBL-edge latching and paced movement interrupts
//
//  System and slot ROMs are additionally COPIED into the base Cpu's internal
//  memory array. That array is not what execution reads -- the bus is -- but
//  PeekByte and the disassembler read it, so without the copy the debugger
//  shows zeros wherever ROM lives.
//
//  Both the initial build and a machine switch run through here, which is why
//  the trace ring is (re)allocated in this function rather than at startup.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineManager::CreateCpu (const MachineConfig & config)
{
    HRESULT                  hr      = S_OK;
    std::unique_ptr<ICpu>    cpu     = nullptr;
    std::ifstream            romFile;
    Word                     addr    = 0;
    char                     byte    = 0;



    // Select the CPU strategy per the machine profile (65C02 for the
    // Enhanced //e and //c; NMOS 6502 for everything else). Building the
    // wrong part silently is the exact defect this seam removes; an
    // unbuildable strategy means a broken machine profile (a shipped config
    // naming a CPU we don't have), so CHRA asserts -- a debug build breaks
    // for a dev to dig in -- before the machine build fails here. CpuFactory
    // itself just returns E_INVALIDARG, so it stays a clean, testable validator.
    hr = CpuFactory::Create (config.cpu, m_shell.m_memoryBus, cpu);
    CHRA (hr);

    m_shell.m_cpu = std::make_unique<EmuCpu> (m_shell.m_memoryBus, std::move (cpu));

    // --trace: allocate the CPU execution-trace ring now that the CPU
    // exists. Covers both initial machine build and machine switches,
    // since both paths run through here.
    if (m_shell.m_traceCapacity > 0)
    {
        m_shell.m_cpu->EnableTrace (m_shell.m_traceCapacity);
    }

    // Wire the //e video timing model into the EmuCpu cycle fan-out.
    // Every AddCycles call now ticks VideoTiming so $C019 (RDVBLBAR)
    // tracks the 17,030-cycle frame. Null-safe for tests/builds that
    // haven't constructed a timing model.
    if (m_shell.m_videoTiming != nullptr)
    {
        m_shell.m_cpu->SetVideoTiming (m_shell.m_videoTiming.get());
    }

    // Wire the InterruptController to the CPU. On the //c the mouse's VBL +
    // movement lines (and the two ACIAs) assert through it; on the //e and
    // earlier no sources assert yet, so the seam is shared but quiet.
    m_shell.m_interruptController.SetCpu (m_shell.m_cpu->GetCpu());

    // //c IOU mouse: tick the device from the per-instruction cycle fan-out
    // so VBL-edge latching and paced movement interrupts stay phase-locked
    // to CPU progress (null for every other machine).
    if (m_shell.m_mouse != nullptr)
    {
        m_shell.m_cpu->SetCycleSink (m_shell.m_mouse.get());
    }

    // The base Cpu class uses an internal memory[] array. Copy system
    // ROM and slot ROMs into that array so PeekByte/disassembly can
    // see them.
    {
        // System ROM
        if (!config.systemRom.resolvedPath.empty())
        {
            romFile.open (config.systemRom.resolvedPath, std::ios::binary);

            if (romFile.good())
            {
                addr = config.systemRom.address;

                while (romFile.good() && addr < config.systemRom.address + config.systemRom.fileSize)
                {
                    romFile.read (&byte, 1);

                    if (romFile.gcount() == 1)
                    {
                        m_shell.m_cpu->PokeByte (addr, static_cast<Byte> (byte));
                        addr++;
                    }
                }

                romFile.close();
            }
        }

        // Slot ROMs
        for (const auto & slot : config.slots)
        {
            if (!slot.enabled || slot.rom.empty() || slot.resolvedRomPath.empty())
            {
                continue;
            }

            romFile.open (slot.resolvedRomPath, std::ios::binary);

            if (!romFile.good())
            {
                continue;
            }

            addr = static_cast<Word> (0xC000 + slot.slot * 0x100);

            while (romFile.good() && addr < 0xC000 + slot.slot * 0x100 + slot.romSize)
            {
                romFile.read (&byte, 1);

                if (romFile.gcount() == 1)
                {
                    m_shell.m_cpu->PokeByte (addr, static_cast<Byte> (byte));
                    addr++;
                }
            }

            romFile.close();
        }
    }

    m_shell.m_cpu->InitForEmulation(*m_shell.m_prng);

    // Connect speaker to CPU cycle counter for audio timestamps
    if (m_shell.m_refs.speaker != nullptr)
    {
        m_shell.m_refs.speaker->SetCycleCounter (m_shell.m_cpu->GetCycleCounterPtr());
    }

    // Issue #67: drive Disk2Controller bit-stream catch-up off the CPU
    // cycle counter so every $C0Ex read/write resyncs the engine to
    // elapsed CPU time before the soft-switch dispatch fires (matches
    // AppleWin's CpuCalcCycles-at-top-of-handler pattern). MachineManager
    // owns both the EmuCpu and the device list, so this is the right
    // wiring point -- the controller is cached into m_refs.diskController
    // just above in AddDevices.
    if (m_shell.m_refs.diskController != nullptr)
    {
        m_shell.m_refs.diskController->SetCpuCycleSource (m_shell.m_cpu->GetBusCyclePtr());

        // Motor-idle auto-flush: when the drive spins down (operation done),
        // persist dirty images so writes survive a crash / kill before the
        // next eject or exit. The callback fires on the CPU thread inside
        // Tick, which owns the disk writes, so it races nothing; FlushAll
        // skips clean images and the flush-error reporter surfaces failures.
        m_shell.m_refs.diskController->SetMotorOffFlushCallback ([this] ()
        {
            m_shell.m_diskStore.FlushAll();

            // The disk has just stopped, so this is the quietest moment there
            // is to swap what is under it. One line and no decisions: which
            // bay, whether anything settled and what to do about it are all
            // the store's.
            m_shell.m_diskStore.ApplyPendingReload();
        });

        // The spindown hook above is not enough on its own, and the gap is the
        // ordinary case rather than an edge: it fires only on a motor-on to
        // motor-off transition, so a guest sitting at a BASIC prompt -- which
        // is exactly where a build loop leaves it -- would never learn that its
        // disk changed. This fires while nothing is in flight, which is nearly
        // always, rate-limited to once an emulated frame.
        //  Left uninstalled under --no-image-watch, which is the measurement
        //  seam: with neither the watcher nor this, nothing learns of a change
        //  until the emulator is about to write, and the re-check made there
        //  is what has to carry the guarantee on its own.
        if (!m_shell.IsImageWatchDisabled())
        {
            m_shell.m_refs.diskController->SetIdleCallback ([this] ()
            {
                m_shell.m_diskStore.ApplyPendingReload();
            });
        }

        // Restarting after a pick-up. The decision is the store's and the
        // action is the shell's -- a device-layer image store reaching machine
        // lifecycle directly would be a layering inversion.
        m_shell.m_diskStore.SetMachineRestartCallback ([this] ()
        {
            PowerCycle();
        });
    }

    // Drive the analog paddle/joystick PREAD timer ($C070 strobe,
    // $C064-$C067 countdown) off the same CPU bus-cycle accumulator so a
    // paddle read measures elapsed cycles since the strobe.
    {
        auto * iieSw = m_shell.m_refs.iieSoftSwitches;

        if (iieSw != nullptr)
        {
            iieSw->SetCpuCycleSource (m_shell.m_cpu->GetBusCyclePtr());
        }

        if (m_shell.m_refs.gamePort != nullptr)
        {
            m_shell.m_refs.gamePort->SetCpuCycleSource (m_shell.m_cpu->GetBusCyclePtr());
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowMachinePicker
//
//  Legacy `MachinePickerDialog` is retired (FR-027). The consolidated
//  Settings panel hosts the machine selector and routes the actual
//  switch through `SwitchMachine` / `SettingsPanelState::Apply` on
//  commit. Old entry points (`IDM_FILE_OPEN`, status-bar machine cell)
//  funnel here so they keep working with no behavioral surprise to
//  the user.
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::ShowMachinePicker()
{
    OutputDebugStringA ("[MachineManager] ShowMachinePicker unavailable in native-only baseline.\n");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SwitchMachine
//
//  Replaces the running machine in place: load the new config, tear the old
//  one down, rebuild, and carry forward what the user would expect to survive.
//
//  Runs on the CPU THREAD, which shapes several decisions below. Anything
//  UI-facing is posted rather than called -- the color-mode application goes
//  through PostMessage instead of the command dispatcher, because that
//  dispatcher is a UI-thread surface and today's harmless atomic store would
//  quietly inherit this thread the moment anything else were added to it.
//
//  Assets are NOT fetched here. ROM and disk-audio bootstrap happens on the UI
//  thread in ShowMachinePicker before the switch is enqueued, so by the time
//  this runs everything the new machine needs is already on disk.
//
//  The machine is built from the USER-MERGED config, not the shipped config
//  text. Without that, a machine-level edit -- a slot disabled in Settings >
//  Hardware, say -- would apply only to the live speed and color and silently
//  revert on every switch or reboot. The extra $cassoUiPrefs and version keys
//  the merge carries are ignored by the loader.
//
//  Teardown order is the delicate half, and every step is placed against a
//  reference that would otherwise dangle:
//
//    debug panels    hold raw pointers into the OLD CPU's cycle counter
//    event sinks     keyboard, //e soft switches, and game port all point at
//                    panels that outlive the devices
//    printer worker  its drain thread holds a reference into the card's ring,
//                    which clearing the owned devices is about to free
//    audio mixer     borrows PSG sources owned by the Mockingboard card
//    IRQ tokens      reclaimed before their holders are destroyed
//    //c ROM bank    references the language card and the MMU
//
//  m_refs is reset AS A WHOLE rather than field by field. It is a struct of
//  observer pointers into the owning collections, so resetting it wholesale
//  keeps the "every observer dies with its owner" invariant from rotting as
//  observers are added. m_mmu needs its own explicit reset because it survives
//  across switches and is only reassigned when the new config carries an
//  apple2e-mmu -- otherwise a //e to ][ switch keeps a stale RamDevice pointer.
//
//  Three things deliberately CARRY ACROSS the switch:
//
//    dirty disks     flushed before teardown, so user writes are not lost
//    mounted disks   re-mounted on the new machine. The mental model is
//                    physical -- the user changed the computer, not the disk
//                    in the drive -- and re-mounting also updates the new
//                    machine's prefs so it sticks on later launches
//    pending print   persisted while m_currentMachineName still names the
//                    OUTGOING machine, so the strip lands in its folder
//                    (FR-026)
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineManager::SwitchMachine (const std::wstring & machineName)
{
    HRESULT                hr                = S_OK;
    std::vector<fs::path>  searchPaths;
    fs::path               configRelPath;
    fs::path               configPath;
    std::ifstream          configFile;
    bool                   configGood        = false;
    std::stringstream      ss;
    std::string            jsonText;
    std::vector<fs::path>  romSearchPaths;
    std::string            error;
    MachineConfig          newConfig;
    std::string            machineNameNarrow = fs::path (machineName).string();
    JsonValue              defaultJson;
    JsonValue              mergedJson;
    JsonParseError         parseErr;
    WORD                   speedCmd          = 0;
    bool                   foundConfig       = false;
    HRESULT                hrParse           = S_OK;
    HRESULT                hrMerge           = S_OK;
    std::string            carryDisk1;
    std::string            carryDisk2;



    // Find and load the new machine config. ROM/disk-audio asset
    // bootstrap happens on the UI thread in ShowMachinePicker before
    // the switch command is enqueued; by the time we're here, every
    // asset the new machine needs is already on disk.
    searchPaths   = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                     PathResolver::GetWorkingDirectory());
    configRelPath = fs::path ("Machines") / machineNameNarrow
                                          / (machineNameNarrow + ".json");
    configPath    = PathResolver::FindFile (searchPaths, configRelPath);

    foundConfig = !configPath.empty();
    CBRN (foundConfig,
          std::format (L"Machine config not found: {}", machineName).c_str());

    configFile.open (configPath);
    configGood = configFile.good();
    CBRN (configGood,
          std::format (L"Cannot open machine config:\n{}", configPath.wstring()).c_str());

    ss << configFile.rdbuf();
    jsonText = ss.str();

    hrParse = JsonParser::Parse (jsonText, defaultJson, parseErr);

    if (SUCCEEDED (hrParse) && m_shell.m_userConfigStore != nullptr)
    {
        hrMerge = m_shell.m_userConfigStore->Load (machineNameNarrow,
                                                   defaultJson,
                                                   m_shell.m_uiFs,
                                                   mergedJson);

        if (SUCCEEDED (hrMerge))
        {
            speedCmd = ResolveMachineSpeedCommand (mergedJson);

            // Push the persisted colorMode into the running shell so
            // the screen actually reflects what the user saved last
            // session. Without this the live colorMode stays at its
            // default until the user opens Settings -- which then
            // makes Cancel snap the screen to the persisted value
            // because the panel reads the baseline from prefs, not
            // from the live shell state.
            if (mergedJson.GetType() == JsonType::Object)
            {
                const JsonValue *  uiPrefs   = nullptr;
                std::string        colorMode;

                // THE MONITOR'S OWN PHOSPHOR, not zero and not a fixed
                // default: a machine with no saved color mode must still be
                // told what to show. Leaving it unset applied nothing at all,
                // and what the screen kept was the mode of the machine being
                // switched AWAY from -- which is how the //c came up green
                // after an //e and white after an Enhanced //e, with nothing
                // about the //c deciding either.
                WORD               colorCmd  = MonitorCatalog::PhosphorCommand (
                                                   MonitorCatalog::ForMachineJson (mergedJson));

                if (mergedJson.HasObject ("$cassoUiPrefs", uiPrefs) &&
                    uiPrefs != nullptr &&
                    uiPrefs->HasString ("colorMode", colorMode))
                {
                    if      (colorMode == "green")  { colorCmd = IDM_VIEW_GREEN; }
                    else if (colorMode == "amber")  { colorCmd = IDM_VIEW_AMBER; }
                    else if (colorMode == "white")  { colorCmd = IDM_VIEW_WHITE; }
                }

                // SwitchMachine runs on the CPU thread: route through the
                // message loop, not HandleCommand directly -- the command
                // dispatcher is a UI-thread surface (today the color handler
                // is an atomic store, but anything added to it would inherit
                // this thread; see the ApplyDefaultPointerForMachine assert).
                PostMessageW (m_shell.m_hwnd, WM_COMMAND, MAKEWPARAM (colorCmd, 0), 0);

                // //c external drive: adopt the switched-to machine's persisted
                // connected state so the second drive-mount widget matches the
                // saved setting once ReflowChromeForMachineChange relays the
                // chrome. Defaults to not-connected; harmless on non-//c
                // machines (ShouldShowExternalDrive ignores it when the system
                // ROM is not banked).
                {
                    const JsonValue  * extPrefs   = nullptr;
                    const JsonValue  * portsArray = nullptr;
                    bool               connected  = false;
                    bool               mouseConn  = false;
                    bool               fFromPort  = false;

                    if (mergedJson.HasObject ("$cassoUiPrefs", extPrefs) &&
                        extPrefs != nullptr)
                    {
                        HRESULT  hrExt = extPrefs->GetBool ("externalDriveConnected", connected);
                        IGNORE_RETURN_VALUE (hrExt, S_OK);
                    }

                    // The back-panel disk port is the answer when the machine
                    // declares one; the legacy boolean above stays as the
                    // fallback for a config that has not been folded yet.
                    if (mergedJson.HasArray ("ports", portsArray) &&
                        portsArray != nullptr)
                    {
                        for (size_t p = 0; !fFromPort && p < portsArray->GetArraySize(); p++)
                        {
                            const JsonValue  & port       = portsArray->GetArrayElement (p);
                            string             portName;
                            string             portDevice;
                            HRESULT            hrName     = S_OK;
                            HRESULT            hrDev      = S_OK;

                            if (port.GetType() != JsonType::Object)
                            {
                                continue;
                            }

                            hrName = port.GetString ("name",   portName);
                            hrDev  = port.GetString ("device", portDevice);

                            IGNORE_RETURN_VALUE (hrName, S_OK);
                            IGNORE_RETURN_VALUE (hrDev,  S_OK);

                            if (portName == "disk")
                            {
                                connected = !portDevice.empty();
                                fFromPort = true;
                            }
                        }
                    }

                    m_shell.m_externalDriveConnected = connected;

                    // //c mouse peripheral: adopt the switched-to machine's
                    // persisted connected state (default CONNECTED).
                    mouseConn = true;
                    if (extPrefs != nullptr)
                    {
                        HRESULT  hrM = extPrefs->GetBool ("mouseConnected", mouseConn);
                        IGNORE_RETURN_VALUE (hrM, S_OK);
                    }

                    m_shell.m_mouseConnected = mouseConn;

                    // //c: default Pointer -> Mouse when connected and no
                    // pointer mapping is active. Runtime nudge.
                    m_shell.ApplyDefaultPointerForMachine();
                }
            }
        }
    }

    romSearchPaths.push_back (configPath.parent_path().parent_path().parent_path());

    for (const auto & p : searchPaths)
    {
        if (p != romSearchPaths[0])
        {
            romSearchPaths.push_back (p);
        }
    }

    // Build the machine from the user-delta-merged config, not the base config
    // text, so machine-level edits -- e.g. a slot the user disabled in
    // Settings > Hardware (slots[].enabled=false) -- actually take effect on a
    // switch/reboot instead of only the live-applied speed/color. Falls back to
    // the base text when there is no merged result (no user delta). The extra
    // $cassoUiPrefs / version keys the merge carries are ignored by the loader.
    if (mergedJson.GetType() == JsonType::Object)
    {
        jsonText = JsonWriter::Write (mergedJson);
    }

    hr = MachineConfigLoader::Load (jsonText,
                                    machineNameNarrow,
                                    romSearchPaths,
                                    newConfig,
                                    error);
    CHRN (hr, std::format (L"Failed to load machine config:\n{}",
                           std::wstring (error.begin(), error.end())).c_str());

    // Auto-flush every dirty disk before tearing down the previous
    // machine so user writes survive the machine switch.
    {
        HRESULT  hrFlush = m_shell.m_diskStore.FlushAll();
        IGNORE_RETURN_VALUE (hrFlush, S_OK);
    }

    // Snapshot the currently-mounted slot-6 disks so they follow the
    // user across the machine switch. The mental model is physical:
    // the user mounted a disk, changed the host machine, and expects
    // the disk to still be in the drive. Re-mounting on the new
    // machine also updates its per-machine prefs so the disk sticks
    // on subsequent launches. Empty paths fall through to the
    // per-machine prefs lookup inside MountCommandLineDisks.
    carryDisk1 = m_shell.m_diskStore.GetSourcePath (6, 0);
    carryDisk2 = m_shell.m_diskStore.GetSourcePath (6, 1);

    // Tear down current machine. The Disk II debug dialog (if open)
    // holds a raw pointer into the old CPU's cycle counter; revoke it
    // before the CPU is reset so the dialog can't dereference dangling
    // memory between here and CreateCpu below.
    if (m_shell.m_disk2DebugPanel != nullptr)
    {
        m_shell.m_disk2DebugPanel->SetCycleCounter (nullptr);
    }

    if (m_shell.m_inputDebugPanel != nullptr)
    {
        m_shell.m_inputDebugPanel->SetCycleCounter (nullptr);
    }

    if (m_shell.m_refs.keyboard != nullptr)
    {
        m_shell.m_refs.keyboard->SetInputEventSink (nullptr);
    }

    {
        auto * iieSwitches = m_shell.m_refs.iieSoftSwitches;
        if (iieSwitches != nullptr)
        {
            iieSwitches->SetInputEventSink (nullptr);
        }
    }

    if (m_shell.m_refs.gamePort != nullptr)
    {
        m_shell.m_refs.gamePort->SetInputEventSink (nullptr);
    }

    // Tear down ALL per-machine state in one atomic move. m_refs is a
    // struct of observer pointers into the owning collections
    // (m_ownedDevices, m_videoModes); resetting it as a whole keeps
    // the "every observer must be invalidated when its owner goes
    // away" invariant from rotting as new observers are added. m_mmu
    // is a unique_ptr that survives across switches and is only
    // reassigned when the new config carries an apple2e-mmu device;
    // it must be explicitly reset here or it'll keep its stale
    // RamDevice pointer alive across a //e -> ][ switch.
    //
    // Stop the printer drain thread first: its job holds a reference into the
    // card's ring, which m_ownedDevices.clear() is about to free.
    m_shell.m_printerWorker.Stop();

    // Persist the outgoing machine's pending strip before its card is freed --
    // m_currentMachineName is still the outgoing machine here (FR-026). An empty
    // strip clears any stale sidecar.
    if (!m_shell.m_currentMachineName.empty())
    {
        PrinterJob *   printJob = m_shell.m_printerWorker.GetJob();

        if (printJob != nullptr && printJob->HasContent())
        {
            HRESULT   hrSave = PrintJobStore::Save (m_shell.GetPendingPrintDir(), printJob->GetRaster());
            IGNORE_RETURN_VALUE (hrSave, S_OK);
        }
        else
        {
            PrintJobStore::Clear (m_shell.GetPendingPrintDir());
        }
    }

    // The Mockingboard's PSG audio sources are owned by the card device
    // in m_ownedDevices, so the mixer's borrowed pointers must be dropped
    // before that collection is cleared below (CreateMemoryDevices
    // re-registers fresh ones for the new machine).
    m_shell.m_mockingboardAudioMixer.UnregisterAllSources();

    // Reclaim IRQ source tokens before the devices that hold them are
    // destroyed; the rebuilt machine re-registers from a fresh pool.
    m_shell.m_interruptController.ResetSources();

    m_shell.m_cpu.reset();
    // The //c ROM-bank coordinator holds references into the language card
    // (owned) + MMU; drop it before those owners are torn down.
    m_shell.m_apple2cRomBank.reset();
    m_shell.m_ownedDevices.clear();
    m_shell.m_videoModes.clear();
    m_shell.m_memoryBus = MemoryBus();
    m_shell.m_refs      = {};
    m_shell.m_mmu.reset();

    // Initialize with new config
    m_shell.m_currentMachineName = machineName;
    m_shell.m_config             = newConfig;
    m_shell.m_cyclesPerFrame     = newConfig.cyclesPerFrame;

    hr = CreateMemoryDevices (newConfig);
    CHR (hr);

    // CreateMemoryDevices unregistered the old disk-audio sources and
    // built new ones. They've been registered with the mixer but no
    // sample data is loaded yet -- SetMechanism is what triggers
    // LoadSamples on each registered source. Without this re-poke,
    // the new machine's drive plays in eerie silence (FR-009).
    {
        std::wstring  currentMechanism = m_shell.m_driveAudioMixer.GetMechanism();
        HRESULT       hrMech           = m_shell.m_driveAudioMixer.SetMechanism (currentMechanism);
        IGNORE_RETURN_VALUE (hrMech, S_OK);
    }

    WireLanguageCard();
    WireApple2cRomBank();
    CreateVideoModes();

    hr = m_shell.m_memoryBus.Validate();
    CHR (hr);

    hr = CreateCpu (newConfig);
    CHR (hr);

    WirePageTable();

    // Re-attach the new CPU's cycle counter to the debug dialog (the
    // pointer was revoked above before the old CPU was destroyed).
    if (m_shell.m_disk2DebugPanel != nullptr && m_shell.m_cpu != nullptr)
    {
        m_shell.m_disk2DebugPanel->SetCycleCounter (m_shell.m_cpu->GetCycleCounterPtr());
    }

    if (m_shell.m_inputDebugPanel != nullptr && m_shell.m_cpu != nullptr)
    {
        m_shell.m_inputDebugPanel->SetCycleCounter (m_shell.m_cpu->GetCycleCounterPtr());
    }

    // Re-wire the debug dialog onto the freshly built controller +
    // audio source. Without this the dialog goes silent after a
    // machine switch even though it's still on screen.
    m_shell.AttachDebugSinksIfOpen();

    m_shell.UpdateWindowTitle();

    // Record the new active machine in GlobalUserPrefs so the next
    // launch boots it by default. SaveGlobalPrefs flushes the change
    // to UserPrefs.json on disk.
    if (m_shell.m_globalPrefs.lastSelectedMachine != machineNameNarrow)
    {
        m_shell.m_globalPrefs.lastSelectedMachine = machineNameNarrow;
        m_shell.SaveGlobalPrefs();
    }

    // Same cold-power-on sequence as Initialize() -- seed DRAM and
    // run the 6502 /RESET sequence. Without this the newly-built
    // machine starts with a random PC into uninitialized RAM. Mounts
    // persist across the switch (they were flushed above and re-
    // mounted by the new config); aux RAM, LC RAM, and CPU registers
    // are all reseeded.
    //
    // Must run BEFORE the per-machine remount: PowerCycle ejects every
    // drive and rebinds the controller's engine to its empty internal
    // disk, which would silently throw away whatever we just mounted.
    PowerCycle();

    // Remount per-machine disks if any were saved last time this
    // machine was active. The disks that were in the drives before
    // the switch take priority (passed explicitly here) so the user's
    // physical mental model holds: the disk in the drive stays in
    // the drive across a machine swap. Empty paths fall through
    // harmlessly so a never-used machine won't try to mount anything.
    //
    // If the new machine has no Disk II controller at slot 6 (future
    // non-Apple-II family), drop the carry rather than silently relying
    // on MountDiskInSlot6's nullptr CBR. The disk in DiskImageStore
    // was already flushed above, so no user data is lost.
    if (!m_shell.m_diskManager->HasSlot6Controller())
    {
        carryDisk1.clear();
        carryDisk2.clear();
    }

    m_shell.m_diskManager->MountCommandLineDisks (carryDisk1, carryDisk2);

    // Same rule as the color mode: a machine with no saved speed gets the
    // default, never the outgoing machine's.
    if (speedCmd == 0)
    {
        speedCmd = IDM_MACHINE_SPEED_1X;
    }

    if (speedCmd != 0)
    {
        // Same routing rule as the color command above: dispatch on the UI
        // thread via the message loop, never directly from the CPU thread.
        PostMessageW (m_shell.m_hwnd, WM_COMMAND, MAKEWPARAM (speedCmd, 0), 0);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Drives the //e /RESET path: every device clears its reset-sensitive
//  state (80COL/ALTCHARSET no longer survive), the MMU returns to the
//  post-reset banking flags, and the CPU re-loads PC from $FFFC. User
//  RAM is preserved.
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::SoftReset()
{
    m_shell.m_memoryBus.SoftResetAll();

    if (m_shell.m_mmu != nullptr)
    {
        m_shell.m_mmu->OnSoftReset();
    }

    m_shell.m_interruptController.SoftReset();

    // //c IOU mouse: /RESET clears the interrupt latches + enables and
    // shuts the IOU access gate (matches power-on state).
    if (m_shell.m_mouse != nullptr)
    {
        m_shell.m_mouse->Reset();
    }

    if (m_shell.m_videoTiming != nullptr)
    {
        m_shell.m_videoTiming->SoftReset();
    }

    if (m_shell.m_cpu != nullptr)
    {
        m_shell.m_cpu->SoftReset();
    }

    // Re-zero the Disk II Debug Uptime column on every reset so the
    // user sees a clean 00:00 anchor after each Ctrl+Shift+R / Ctrl+Shift+P.
    m_shell.ResetUptimeAnchor();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  Reseeds every DRAM-owning device from the shared Prng then runs the
//  SoftReset sequence. The Prng is constructed once (host process
//  lifetime) so consecutive cycles within a single session continue
//  producing fresh patterns rather than repeating the seed.
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::PowerCycle()
{
    HRESULT  hrFlush = S_OK;



    if (m_shell.m_prng == nullptr)
    {
        return;
    }

    // Auto-flush dirty disks before reseeding device state so writes
    // don't get lost across a power cycle. Mounts persist (matches
    // DiskImageStore::SoftReset semantics -- see comment block on
    // DiskImageStore::PowerCycle, which is the unmount-everything
    // variant tests can opt into directly).
    hrFlush = m_shell.m_diskStore.FlushAll();
    IGNORE_RETURN_VALUE (hrFlush, S_OK);

    m_shell.m_memoryBus.PowerCycleAll (*m_shell.m_prng);

    if (m_shell.m_mmu != nullptr)
    {
        m_shell.m_mmu->OnPowerCycle (*m_shell.m_prng);
    }

    m_shell.m_interruptController.PowerCycle();

    // //c IOU mouse: power-on state (latches clear, interrupts masked,
    // IOU access gate shut).
    if (m_shell.m_mouse != nullptr)
    {
        m_shell.m_mouse->Reset();
    }

    if (m_shell.m_videoTiming != nullptr)
    {
        m_shell.m_videoTiming->PowerCycle (*m_shell.m_prng);
    }

    if (m_shell.m_cpu != nullptr)
    {
        m_shell.m_cpu->PowerCycle (*m_shell.m_prng);
    }

    // Re-zero the Disk II Debug Uptime column on every power-cycle as
    // well as soft-reset.
    m_shell.ResetUptimeAnchor();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectVideoMode
//
//  Resolves the soft-switch state into the active renderer, once per frame.
//
//  The switches are read fresh each call rather than being tracked on change,
//  because a program may flip them at any point in a frame and the renderer
//  only needs their state at render time.
//
//  Two //e cases are not obvious from the switch names.
//
//  When 80STORE is active, $C054/$C055 no longer mean page 1 / page 2 -- they
//  select MAIN vs AUX memory. So page2 is forced false for rendering purposes;
//  honoring it would display the wrong half of memory whenever a program uses
//  80STORE banking, which is most //e software that touches aux.
//
//  Double hi-res requires DHIRES *and* 80COL together. DHIRES alone is not
//  enough -- the mode depends on the 80-column circuitry to interleave main
//  and aux -- so a program that sets only DHIRES still gets standard hi-res,
//  which is what the hardware does (FR-019).
//
//  Page 2 and ALTCHARSET are pushed to renderers OTHER than the active one on
//  purpose: mixed mode overlays text rows over graphics, so the text renderers
//  must stay current even while a graphics mode is active.
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::SelectVideoMode()
{
    bool                     is80ColMode     = false;
    bool                     altCharSet      = false;
    bool                     doubleHiRes     = false;
    Apple2eSoftSwitchBank *  iieSoftSwitches = m_shell.m_refs.iieSoftSwitches;



    // No machine built yet, or one torn down. The modes are created and
    // cleared together, so text40 answers for all of them.
    if (m_shell.m_refs.text40 == nullptr)
    {
        return;
    }

    // Read soft switch state
    if (m_shell.m_refs.softSwitches)
    {
        m_shell.m_graphicsMode = m_shell.m_refs.softSwitches->IsGraphicsMode();
        m_shell.m_mixedMode    = m_shell.m_refs.softSwitches->IsMixedMode();
        m_shell.m_page2        = m_shell.m_refs.softSwitches->IsPage2();
        m_shell.m_hiresMode    = m_shell.m_refs.softSwitches->IsHiresMode();
    }

    // Everything the //e bank contributes, gathered under one test: it is
    // null on a ][ / ][+, where the defaults above are the right answer.
    if (iieSoftSwitches != nullptr)
    {
        // When 80STORE is active, $C054/$C055 control aux/main memory
        // selection -- not page 1/page 2. Suppress page2 for rendering.
        if (iieSoftSwitches->Is80Store())
        {
            m_shell.m_page2 = false;
        }

        is80ColMode = iieSoftSwitches->Is80ColMode();
        altCharSet  = iieSoftSwitches->IsAltCharSet();
        doubleHiRes = iieSoftSwitches->IsDoubleHiRes();
    }

    // Select video mode based on soft switch state
    if (!m_shell.m_graphicsMode)
    {
        // Text mode: use 80-col on //e if enabled, else 40-col
        m_shell.m_refs.activeVideoMode = is80ColMode
                                             ? static_cast<VideoOutput *> (m_shell.m_refs.text80)
                                             : static_cast<VideoOutput *> (m_shell.m_refs.text40);
    }
    else if (!m_shell.m_hiresMode)
    {
        // Lo-res graphics
        m_shell.m_refs.activeVideoMode = m_shell.m_refs.loRes;
    }
    else
    {
        // Hi-res graphics -- double hi-res needs DHIRES *and* 80COL together
        // on the //e (FR-019, audit M8). Otherwise standard hi-res.
        bool useDhr = doubleHiRes && is80ColMode;

        m_shell.m_refs.activeVideoMode = useDhr
                                             ? static_cast<VideoOutput *> (m_shell.m_refs.doubleHiRes)
                                             : static_cast<VideoOutput *> (m_shell.m_refs.hiRes);
    }

    // Pass page2 state to the active renderer
    if (m_shell.m_refs.activeVideoMode != nullptr)
    {
        m_shell.m_refs.activeVideoMode->SetPage2 (m_shell.m_page2);
    }

    // Keep text mode page2-aware for mixed-mode overlay rendering
    m_shell.m_refs.text40->SetPage2 (m_shell.m_page2);

    // Propagate ALTCHARSET to both text-mode renderers.
    m_shell.m_refs.text40->SetAltCharSet (altCharSet);
    m_shell.m_refs.text80->SetAltCharSet (altCharSet);
}


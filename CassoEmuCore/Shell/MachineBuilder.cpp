#include "Pch.h"

#include "MachineBuilder.h"

#include "Audio/DriveAudioMixer.h"
#include "Audio/PrinterAudioSource.h"
#include "Core/ComponentRegistry.h"
#include "Core/CpuFactory.h"
#include "Core/JsonParser.h"
#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"
#include "Core/Prng.h"
#include "Devices/Acia6551.h"
#include "Devices/AciaEndpoints.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Core/TextEncoding.h"
#include "Machines/MachineDefinitions.h"
#include "Machines/MachineDeviceTypes.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/ParallelFirmware.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Print/PrintJobStore.h"
#include "Print/PrinterWorker.h"
#include "WasapiAudio.h"
#include "../AssetBootstrap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineBuilder::MachineBuilder
//
////////////////////////////////////////////////////////////////////////////////

MachineBuilder::MachineBuilder (MachineHost & host, const MachineBuildServices & services)
    : m_host (host),
      m_services (services)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineBuilder::Build
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineBuilder::Build (const MachineConfig & config)
{
    HRESULT  hr = S_OK;



    //  The registry the device pass creates from. This used to be filled by
    //  EmulatorShell::Initialize, between initializing OLE and allocating
    //  framebuffers, so a machine built by anything other than a starting
    //  window came up with every registry-made device missing -- and
    //  silently, because an unknown device type is a diagnostic and a
    //  `continue`. The machine that needs the factories is the one being
    //  built, and this is where it is built.
    ComponentRegistry::RegisterBuiltinDevices (m_host.GetRegistry());

    hr = CreateMemoryDevices (config);
    CHR (hr);

    WireLanguageCard();

    // //c banked ROM: layer the $C028 bank-switch coordinator + no-slots
    // $Cxxx routing on top of the flat bank-0 split WireLanguageCard just
    // did. Without this a //c has no ROM banking (and no SetNoExternalSlots),
    // so $C800 floats and the firmware derails to a garbage screen. No-op for
    // non-banked machines (romBankSize == 0).
    WireBankedRom();

    CreateVideoModes();

    // Overlapping device address ranges are a build error, not something to
    // discover when the guest reads one of them.
    hr = m_host.GetMemoryBus().Validate();
    CHR (hr);

    hr = CreateCpu (config);
    CHR (hr);

    WirePageTable();

Error:
    return hr;
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
//  WireBankedRom. Building the banking into this step would fork the
//  language-card wiring for one machine.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineBuilder::CreateMemoryDevices (const MachineConfig & config)
{
    HRESULT  hr      = S_OK;
    bool     romOk   = false;



    std::wstring  wideError;
    std::string   error;



    // Load character generator ROM (used by video renderers, not on bus)
    if (!config.characterRom.resolvedPath.empty())
    {
        HRESULT hrChar = m_host.GetCharacterRom().LoadFromFile (config.characterRom.resolvedPath);

        if (FAILED (hrChar))
        {
            DEBUGMSG (L"Failed to load character ROM '%hs', using fallback\n",
                      config.characterRom.resolvedPath.c_str());
            m_host.GetCharacterRom().LoadEmbeddedFallback();
        }
    }
    else
    {
        m_host.GetCharacterRom().LoadEmbeddedFallback();
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

        if (m_host.GetRefs().mainRamDev == nullptr)
        {
            m_host.GetRefs().mainRamDev = device.get();
        }

        m_host.GetMemoryBus().AddDevice (device.get());
        m_host.GetOwnedDevices().push_back (std::move (device));
    }

    // System ROM. Two shapes:
    //   - Flat (//e and earlier): one image mapped at systemRom.address.
    //   - Banked (//c): a multi-bank file whose active bank is toggled at
    //     runtime. Bank 0 is added here as a flat $C000-$FFFF image so the
    //     normal WireLanguageCard split (LC + CxxxRomRouter) applies; the
    //     Apple2cRomBank is layered on afterward (WireBankedRom) to
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

        m_host.GetMemoryBus().AddDevice (device.get());
        m_host.GetOwnedDevices().push_back (std::move (device));
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

        m_host.GetMemoryBus().AddDevice (device.get());
        m_host.GetOwnedDevices().push_back (std::move (device));
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
        if (devCfg.type == MachineDeviceTypes::kMmu)
        {
            m_host.SetMmu (std::make_unique<Apple2eMmu>());
            continue;
        }

        device = m_host.GetRegistry().Create (devCfg.type, devCfg, m_host.GetMemoryBus());

        if (!device)
        {
            DEBUGMSG (L"Warning: Unknown device type '%hs'\n", devCfg.type.c_str());
            continue;
        }

        // Track specific device pointers for quick access
        if (devCfg.type == "apple2-family-keyboard" ||
            devCfg.type == "apple2e-family-keyboard")
        {
            m_host.GetRefs().keyboard = static_cast<AppleKeyboard *> (device.get());

            // Resolve the derived //e pointer here, where the configured
            // device type already says which keyboard was built, rather than
            // dynamic_cast-ing it back out of the base pointer at each use.
            if (devCfg.type == "apple2e-family-keyboard")
            {
                m_host.GetRefs().iieKeyboard =
                    static_cast<Apple2eKeyboard *> (m_host.GetRefs().keyboard);
            }
        }
        else if (devCfg.type == "apple2-family-softswitches" ||
                 devCfg.type == "apple2e-family-softswitches")
        {
            m_host.GetRefs().softSwitches = static_cast<AppleSoftSwitchBank *> (device.get());

            // Resolve the derived //e pointer here, where the configured
            // device type already says which bank was built, rather than
            // dynamic_cast-ing it back out of the base pointer at each use.
            if (devCfg.type == "apple2e-family-softswitches")
            {
                m_host.GetRefs().iieSoftSwitches =
                    static_cast<Apple2eSoftSwitchBank *> (m_host.GetRefs().softSwitches);
            }
        }
        else if (devCfg.type == "apple2-family-gameport")
        {
            m_host.GetRefs().gamePort = static_cast<AppleGamePort *> (device.get());
        }
        else if (devCfg.type == "apple2-family-speaker")
        {
            m_host.GetRefs().speaker = static_cast<AppleSpeaker *> (device.get());
        }

        m_host.GetMemoryBus().AddDevice (device.get());
        m_host.GetOwnedDevices().push_back (std::move (device));
    }

    // Wire IIe keyboard <-> softswitch sibling so $C00C-$C00F reaches
    // the softswitch (the keyboard's range $C000-$C063 would otherwise
    // eat it).
    {
        auto * iieKbd = m_host.GetRefs().iieKeyboard;
        auto * iieSw  = m_host.GetRefs().iieSoftSwitches;

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

            if (m_host.GetRefs().speaker != nullptr)
            {
                iieKbd->SetSpeakerSibling (m_host.GetRefs().speaker);
            }

            if (m_host.GetMmu() != nullptr)
            {
                iieKbd->SetMmu (m_host.GetMmu());
            }

            if (m_host.GetVideoTiming() != nullptr)
            {
                iieKbd->SetVideoTiming (m_host.GetVideoTiming());
            }
        }

        if (iieSw != nullptr)
        {
            if (m_host.GetVideoTiming() != nullptr)
            {
                iieSw->SetVideoTiming (m_host.GetVideoTiming());
            }

            if (m_host.GetMmu() != nullptr)
            {
                iieSw->SetMmu (m_host.GetMmu());
            }
        }
    }

    // Initialize the //e MMU once main RAM exists. The MMU rebinds the
    // page table for $0000-$BFFF based on RAMRD/RAMWRT/ALTZP/80STORE.
    if (m_host.GetMmu() != nullptr && m_host.GetRefs().mainRamDev != nullptr)
    {
        HRESULT hrMmu = m_host.GetMmu()->Initialize (
            &m_host.GetMemoryBus(),
            m_host.GetRefs().mainRamDev,
            nullptr,
            nullptr,
            nullptr,
            m_host.GetRefs().iieSoftSwitches);

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

            device = m_host.GetRegistry().Create (devCfg.type, devCfg, m_host.GetMemoryBus());

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
                    m_host.GetRefs().printerCard = static_cast<PrinterCard *> (device.get());
                }

                m_host.GetMemoryBus().AddDevice (device.get());
                m_host.GetOwnedDevices().push_back (std::move (device));
            }
        }

        // The parallel printer card ships embedded firmware (no rom file), so
        // its slot ROM is installed here from the checked-in byte array rather
        // than loaded from disk.
        if (slot.device == "parallel-printer")
        {
            std::vector<Byte>  firmware (s_kParallelFirmwareBytes,
                                         s_kParallelFirmwareBytes + sizeof (s_kParallelFirmwareBytes));

            if (m_host.GetMmu() != nullptr)
            {
                // //e: the MMU's $C100-$CFFF router owns the page (INTCXROM /
                // SLOTCXROM switching) and pads the short firmware to a full
                // page with the floating-bus byte.
                m_host.GetMmu()->AttachSlotRom (slot.slot, std::move (firmware));
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

                m_host.GetMemoryBus().AddDevice (device.get());
                m_host.GetOwnedDevices().push_back (std::move (device));
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
            if (m_host.GetMmu() != nullptr)
            {
                std::vector<Byte>  bytes (slot.romSize);

                for (size_t i = 0; i < slot.romSize; i++)
                {
                    bytes[i] = device->Read (static_cast<Word> (romStart + i));
                }

                m_host.GetMmu()->AttachSlotRom (slot.slot, std::move (bytes));
            }
            else
            {
                m_host.GetMemoryBus().AddDevice (device.get());
            }

            m_host.GetOwnedDevices().push_back (std::move (device));
        }
    }

    // Apple //c: the built-in 5.25" drive is an IWM at slot 6 ($C0E0-$C0EF).
    // Unlike the //e it is not a card in a slot, so it is created here rather
    // than from the config's (empty) slot list. Its $C600 boot firmware is part
    // of the internal //c ROM (served by the no-slots CxxxRomRouter set in
    // WireBankedRom), so no slot ROM is attached -- only the controller,
    // in IWM mode so the reset firmware's mode/status probe passes.
    // //c IOU mouse: destroyed with the outgoing machine, rebuilt
    // below for the //c. The keyboard/soft-switch bank holding the old
    // pointer are torn down with the same machine, and the CPU thread is
    // stopped during construction, so no stale-pointer window exists.
    m_host.SetMouse (nullptr);

    if (m_host.GetConfig().systemRom.romBankSize != 0)
    {
        auto iwm = std::make_unique<Disk2Controller> (6);
        iwm->SetIwmMode (true);
        m_host.GetMemoryBus().AddDevice (iwm.get());
        m_host.GetOwnedDevices().push_back (std::move (iwm));

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

            m_host.SetMouse (std::make_unique<AppleMouse> ());

            hrIc = m_host.GetMouse()->AttachInterruptController (&m_host.GetInterruptController());
            IGNORE_RETURN_VALUE (hrIc, S_OK);

            m_host.GetMouse()->SetBus (&m_host.GetMemoryBus());

            if (m_host.GetVideoTiming() != nullptr)
            {
                m_host.GetMouse()->SetVideoTiming (m_host.GetVideoTiming());
            }

            iieKbd = m_host.GetRefs().iieKeyboard;
            iieSw = m_host.GetRefs().iieSoftSwitches;

            if (iieKbd != nullptr)
            {
                iieKbd->SetMouse (m_host.GetMouse());

                // Enable the //c case switches ($C060 80/40 read + the
                // keyboard-layout remap). Dormant on the //e.
                iieKbd->SetApple2cMode (true);
            }

            if (iieSw != nullptr)
            {
                iieSw->SetMouse (m_host.GetMouse());
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

            hrIc = acia->AttachInterruptController (&m_host.GetInterruptController());
            IGNORE_RETURN_VALUE (hrIc, S_OK);

            loopback = std::make_unique<AciaLoopbackEndpoint> (acia.get());
            acia->SetEndpoint (loopback.get());

            m_host.GetMemoryBus().AddDevice (acia.get());
            m_host.GetOwnedAciaEndpoints().push_back (std::move (loopback));
            m_host.GetOwnedDevices().push_back (std::move (acia));
        }
    }

    // Cache Disk2Controller pointer for the status-bar drive activity
    // indicator. We pick the first one we find (typically slot 6).
    m_host.GetRefs().diskController = nullptr;
    for (auto & dev : m_host.GetOwnedDevices())
    {
        Disk2Controller *  dc = dynamic_cast<Disk2Controller *> (dev.get());

        if (dc != nullptr)
        {
            m_host.GetRefs().diskController = dc;
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
    if (m_services.diskAudioSources != nullptr && m_services.driveAudioMixer != nullptr)
    {
        int  driveCount = Disk2Controller::kDriveCount;
        int  drive      = 0;

        m_services.diskAudioSources->clear();
        m_services.driveAudioMixer->UnregisterAllSources();

        for (drive = 0; drive < driveCount; drive++)
        {
            auto   src  = std::make_unique<Disk2AudioSource>();
            float  panL = DriveAudioMixer::kSpeakerCenter;
            float  panR = DriveAudioMixer::kSpeakerCenter;

            // Per-drive stereo position from the shell's stored pan
            // (user-adjustable; defaults place Drive 1 left-of-center and
            // Drive 2 right-of-center). drive index is clamped to the
            // stored-pan array bound.
            if (m_services.drivePan != nullptr)
            {
                DriveAudioMixer::PanToStereo (m_services.drivePan[drive], panL, panR);
            }

            src->SetPan (panL, panR);

            m_services.driveAudioMixer->RegisterSource (src.get());
            src->SetDriveIndex (drive);

            if (m_services.driveMotorVolume != nullptr)
            {
                src->SetVolumes (*m_services.driveMotorVolume,
                                 *m_services.driveHeadVolume,
                                 *m_services.driveDoorVolume);
            }

            m_services.diskAudioSources->push_back (std::move (src));
        }

        // The emulated ImageWriter shares the generic drive-audio bus
        // (FR-016). It is a single persistent shell-owned source; the
        // UnregisterAllSources above dropped it along with the disk sources,
        // so re-register it here on every machine build. Silent until the
        // live preview publishes a paced reveal.
        if (m_services.printerAudio != nullptr)
        {
            m_services.driveAudioMixer->RegisterSource (m_services.printerAudio);
        }

        // Feed real disk head / motor / door events to drive 0's source only
        // when the machine actually has the Disk ][ controller realized.
        if (m_host.GetRefs().diskController != nullptr && !m_services.diskAudioSources->empty())
        {
            m_host.GetRefs().diskController->SetAudioSink ((*m_services.diskAudioSources)[0].get());
        }
    }

    // Start the background printer drain once the card exists, seeding it with
    // this machine's persisted pending strip if one exists (FR-026). A missing
    // or corrupt sidecar falls back to empty paper. Symmetric save is in
    // SwitchMachine teardown and OnDestroy.
    if (m_host.GetRefs().printerCard != nullptr && m_services.printerWorker != nullptr)
    {
        PrintRaster   pending;
        HRESULT       hrLoad = PrintJobStore::Load (m_host.GetPendingPrintDir(), pending);

        m_services.printerWorker->Start (
            m_host.GetRefs().printerCard->GetByteRing(),
            SUCCEEDED (hrLoad) ? std::move (pending) : PrintRaster());

        // Pace the drain off the guest clock so the card applies real
        // backpressure -- the guest prints at ImageWriter speed (faster at max
        // perf), never racing ahead of the preview. The cycle pointer is stable
        // for this machine's CPU, so setting it once covers later restarts.
        if (m_host.GetCpu() != nullptr)
        {
            m_services.printerWorker->SetCycleClock (m_host.GetCpu()->GetCycleCounterPtr());
        }

        // Prime the live-preview auto-open baseline to the worker's current
        // activity so a page carried over from a previous session does not read as
        // a fresh print and auto-open the preview on boot -- only new printing does.
        if (m_services.printerAutoOpenActivity != nullptr)
        {
            *m_services.printerAutoOpenActivity = m_services.printerWorker->GetActivityCount();
        }
    }

    // Mockingboard wiring. Cache the card (if the active config installs
    // one), attach both VIA timer-IRQ sources to the shared controller,
    // and register the two PSG audio sources (PSG #1 hard-left, PSG #2
    // hard-right) with the dedicated Mockingboard mixer. The card owns
    // the sources; the mixer holds borrowed pointers, dropped on the next
    // teardown.
    m_host.GetRefs().mockingboard = nullptr;

    if (m_services.mockingboardAudioMixer != nullptr)
    {
        m_services.mockingboardAudioMixer->UnregisterAllSources();
    }

    for (auto & dev : m_host.GetOwnedDevices())
    {
        MockingboardCard *  mb = dynamic_cast<MockingboardCard *> (dev.get());

        if (mb != nullptr)
        {
            m_host.GetRefs().mockingboard = mb;
            break;
        }
    }

    if (m_host.GetRefs().mockingboard != nullptr)
    {
        hr = m_host.GetRefs().mockingboard->AttachInterruptController (&m_host.GetInterruptController());
        CHR (hr);

        if (m_services.mockingboardAudioMixer != nullptr)
        {
            m_services.mockingboardAudioMixer->RegisterSource (m_host.GetRefs().mockingboard->GetAudioSource (0));
            m_services.mockingboardAudioMixer->RegisterSource (m_host.GetRefs().mockingboard->GetAudioSource (1));

            // The sound+speech variant adds its center-panned voice source;
            // the sound-only card has no chip and the source stays silent
            // anyway.
            if (m_host.GetRefs().mockingboard->GetSpeech() != nullptr)
            {
                m_services.mockingboardAudioMixer->RegisterSource (
                    m_host.GetRefs().mockingboard->GetSpeechAudioSource());
            }
        }

        if (m_services.wasapiAudio != nullptr)
        {
            m_host.GetRefs().mockingboard->SetSampleRate (m_services.wasapiAudio->GetSampleRate());
        }

        // On the //e the Apple2eMmu's CxxxRomRouter owns $C100-$CFFF, so a
        // bus device at $Cn00 would be shadowed. Register the card as the
        // slot's active I/O device so the router delegates that page to it
        // (INTCXROM=0). On ][/][+ there is no MMU and the card is
        // bus-resident.
        if (m_host.GetMmu() != nullptr)
        {
            m_host.GetMmu()->GetCxxxRouter()->SetSlotIoDevice (
                m_host.GetRefs().mockingboard->GetSlot(),
                m_host.GetRefs().mockingboard);
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

void MachineBuilder::WireLanguageCard()
{
    LanguageCard *  lc        = nullptr;
    RomDevice    *  romDevice = nullptr;



    // Find the LanguageCard device
    for (auto & dev : m_host.GetOwnedDevices())
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
        for (const auto & entry : m_host.GetMemoryBus().GetEntries())
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
        m_host.GetMemoryBus().RemoveDevice (romDevice);

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
            if (m_host.GetMmu() != nullptr)
            {
                m_host.GetMmu()->AttachInternalCxxxRom (std::move (lowerData));
            }
            else
            {
                auto lowerRom = RomDevice::CreateFromData (
                    slotRomStart, static_cast<Word> (0xCFFF),
                    lowerData.data(), lowerData.size());

                m_host.GetMemoryBus().AddDevice (lowerRom.get());
                m_host.GetOwnedDevices().push_back (std::move (lowerRom));
            }
        }

        // Bank device intercepts $D000-$FFFF, routing to LC RAM or ROM
        lcBank = std::make_unique<LanguageCardBank> (*lc);
        m_host.GetMemoryBus().AddDevice (lcBank.get());
        m_host.GetOwnedDevices().push_back (std::move (lcBank));

        m_host.GetRefs().languageCard = lc;

        // //e wiring: LC needs the MMU (for ALTZP routing) and the
        // keyboard sibling needs the LC pointer for $C011/$C012 status
        // reads.
        if (m_host.GetMmu() != nullptr)
        {
            lc->SetMmu (m_host.GetMmu());

            // Let ALTZP flips re-point the LC's $D000-$FFFF read window (aux/main).
            m_host.GetMmu()->SetLanguageCard (lc);
        }

        iieKbd = m_host.GetRefs().iieKeyboard;

        if (iieKbd != nullptr)
        {
            iieKbd->SetLanguageCard (lc);
        }

        iieSw = m_host.GetRefs().iieSoftSwitches;

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

HRESULT MachineBuilder::ReadRomFileBytes (const std::string & path, std::vector<Byte> & out)
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
//  WireBankedRom
//
//  Layers the Apple //c firmware-bank coordinator on top of the language
//  card + CxxxRomRouter that WireLanguageCard already populated from bank 0.
//  SetBankImages re-applies bank 0 (idempotent) and enables the $C028 flip
//  via the soft-switch bank's IRomBankSwitch hook. No-op for flat-ROM
//  machines (the //e and earlier).
//
////////////////////////////////////////////////////////////////////////////////

void MachineBuilder::WireBankedRom()
{
    const RomReference &  sysRom = m_host.GetConfig().systemRom;



    Apple2eMmu            * mmu      = m_host.GetMmu();
    Apple2eSoftSwitchBank * sw       = m_host.GetRefs().iieSoftSwitches;
    LanguageCard          * lc       = nullptr;
    std::vector<Byte>       fileBytes;
    size_t                  twoBanks = static_cast<size_t> (sysRom.romBankSize) * 2;
    HRESULT                 hrRead   = S_OK;
    bool                    banked   = (sysRom.romBankSize != 0);

    // romBankSize 0 is a flat-ROM machine (the //e and earlier) -- not a
    // failure, so it gets no diagnostic.
    if (banked)
    {
        lc     = m_host.GetRefs().languageCard;
        banked = (mmu != nullptr && sw != nullptr && lc != nullptr);

        if (!banked)
        {
            DEBUGMSG (L"WireBankedRom: missing MMU/soft-switches/LC; banking disabled\n");
        }
    }

    if (banked)
    {
        hrRead = ReadRomFileBytes (sysRom.resolvedPath, fileBytes);
        banked = SUCCEEDED (hrRead) && fileBytes.size() >= twoBanks;

        if (!banked)
        {
            DEBUGMSG (L"WireBankedRom: cannot read both ROM banks; banking disabled\n");
        }
    }

    if (banked)
    {
        std::vector<Byte>   bank0 (fileBytes.begin(),                     fileBytes.begin() + sysRom.romBankSize);
        std::vector<Byte>   bank1 (fileBytes.begin() + sysRom.romBankSize, fileBytes.begin() + twoBanks);

        m_host.SetApple2cRomBank (std::make_unique<Apple2cRomBank> (*lc, *mmu));
        m_host.GetApple2cRomBank()->SetBankImages (std::move (bank0), std::move (bank1));
        sw->SetRomBankSwitch (m_host.GetApple2cRomBank());

        // A machine with no card slots has nothing that could answer in
        // $C100-$CFFF, so the router leaves the whole range to the internal
        // firmware. That is a fact about the machine, and the machine says
        // it: the //c declares zero slots, every other model declares seven.
        // Reading it here rather than assuming it means a later banked-ROM
        // machine that DOES have slots keeps them.
        {
            const MachineDefinition *  definition =
                MachineDefinitions::Find (TextEncoding::WideToNarrow (m_host.GetCurrentMachineName()));

            if (definition != nullptr && definition->slotCount == 0)
            {
                mmu->GetCxxxRouter()->SetNoExternalSlots (true);
            }
        }
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

void MachineBuilder::WirePageTable()
{
    Byte * mainRam = nullptr;



    if (!m_host.GetCpu())
    {
        return;
    }

    mainRam = const_cast<Byte *> (m_host.GetCpu()->GetMemory());

    // Map all RAM pages ($0000-$BFFF) to main memory
    for (int page = 0x00; page < 0xC0; page++)
    {
        Byte * pagePtr = mainRam + (page * 0x100);
        m_host.GetMemoryBus().SetReadPage  (page, pagePtr);
        m_host.GetMemoryBus().SetWritePage (page, pagePtr);
    }

    // Register banking-change callback so soft switches can trigger
    // remapping.
    m_host.GetMemoryBus().SetBankingChangedCallback ([this]()
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

Byte * MachineBuilder::GetAuxRamBuffer()
{
    return m_host.GetMmu() != nullptr ? m_host.GetMmu()->GetAuxBuffer() : nullptr;
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

void MachineBuilder::RebuildBankingPages()
{
    Byte *  mainRam = nullptr;
    int     page    = 0;



    // Only the legacy no-MMU path does anything here: with an MMU present it
    // owns every $0000-$BFFF page and this would fight it.
    if (m_host.GetCpu() && m_host.GetMmu() == nullptr)
    {
        mainRam = const_cast<Byte *> (m_host.GetCpu()->GetMemory());

        // Text page 1 ($0400-$07FF) and hi-res page 1 ($2000-$3FFF) -- the
        // two windows 80STORE/PAGE2 would otherwise re-point.
        for (page = 0x04; page <= 0x07; page++)
        {
            m_host.GetMemoryBus().SetReadPage  (page, mainRam + (page * 0x100));
            m_host.GetMemoryBus().SetWritePage (page, mainRam + (page * 0x100));
        }

        for (page = 0x20; page <= 0x3F; page++)
        {
            m_host.GetMemoryBus().SetReadPage  (page, mainRam + (page * 0x100));
            m_host.GetMemoryBus().SetWritePage (page, mainRam + (page * 0x100));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateVideoModes
//
//  Builds all five renderers up front, publishes each one by name in
//  the refs, and installs 40-column text as the active one.
//
//  Every mode is created regardless of machine, because SelectVideoMode
//  switches between them per frame from the soft-switch state and cannot
//  afford to construct one mid-render.
//
//  The machine's video-mode vector owns them and its refs point at them.
//  This is the ONLY function that
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

void MachineBuilder::CreateVideoModes()
{
    Byte *                                   auxBuf          = nullptr;
    std::unique_ptr<AppleTextMode>           textMode;
    std::unique_ptr<AppleLoResMode>          loResMode;
    std::unique_ptr<AppleHiResMode>          hiResMode;
    std::unique_ptr<AppleDoubleHiResMode>    doubleHiResMode;
    std::unique_ptr<Apple80ColTextMode>      text80;



    textMode        = std::make_unique<AppleTextMode>        (m_host.GetMemoryBus(), m_host.GetCharacterRom());
    loResMode       = std::make_unique<AppleLoResMode>       (m_host.GetMemoryBus());
    hiResMode       = std::make_unique<AppleHiResMode>       (m_host.GetMemoryBus());
    doubleHiResMode = std::make_unique<AppleDoubleHiResMode> (m_host.GetMemoryBus());
    text80          = std::make_unique<Apple80ColTextMode>   (m_host.GetMemoryBus(), m_host.GetCharacterRom());

    m_host.GetRefs().text40      = textMode.get();
    m_host.GetRefs().loRes       = loResMode.get();
    m_host.GetRefs().hiRes       = hiResMode.get();
    m_host.GetRefs().doubleHiRes = doubleHiResMode.get();
    m_host.GetRefs().text80      = text80.get();

    m_host.GetRefs().activeVideoMode = m_host.GetRefs().text40;

    auxBuf = GetAuxRamBuffer();

    if (auxBuf != nullptr)
    {
        text80->SetAuxMemory          (auxBuf);
        doubleHiResMode->SetAuxMemory (auxBuf);

        // DHR and 80-column text need BOTH banks at once, so they take main
        // RAM directly too. The bus cannot serve the main half: its pages
        // follow live banking and point at aux under 80STORE+PAGE2 ($2000-
        // $3FFF with HIRES, $0400-$07FF always), which made DHR render the
        // aux bytes into both halves of every pair, and the mixed-mode text
        // overlay show aux in both columns whenever a frame was scanned while
        // a program had PAGE2 on. This is the same buffer the MMU treats as
        // main.
        if (m_host.GetRefs().mainRamDev != nullptr)
        {
            doubleHiResMode->SetMainMemory (m_host.GetRefs().mainRamDev->GetData());
            text80->SetMainMemory          (m_host.GetRefs().mainRamDev->GetData());
        }
    }

    m_host.GetVideoModes().push_back (std::move (textMode));
    m_host.GetVideoModes().push_back (std::move (loResMode));
    m_host.GetVideoModes().push_back (std::move (hiResMode));
    m_host.GetVideoModes().push_back (std::move (doubleHiResMode));
    m_host.GetVideoModes().push_back (std::move (text80));
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

HRESULT MachineBuilder::CreateCpu (const MachineConfig & config)
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
    hr = CpuFactory::Create (config.cpu, m_host.GetMemoryBus(), cpu);
    CHRA (hr);

    m_host.SetCpu (std::make_unique<EmuCpu> (m_host.GetMemoryBus(),
                                                       std::move (cpu)));

    // --trace: allocate the CPU execution-trace ring now that the CPU
    // exists. Covers both initial machine build and machine switches,
    // since both paths run through here.
    if (m_services.GetTraceCapacity() > 0)
    {
        m_host.GetCpu()->EnableTrace (m_services.GetTraceCapacity());
    }

    // Wire the //e video timing model into the EmuCpu cycle fan-out.
    // Every AddCycles call now ticks VideoTiming so $C019 (RDVBLBAR)
    // tracks the 17,030-cycle frame. Null-safe for tests/builds that
    // haven't constructed a timing model.
    if (m_host.GetVideoTiming() != nullptr)
    {
        m_host.GetCpu()->SetVideoTiming (m_host.GetVideoTiming());
    }

    // Wire the InterruptController to the CPU. On the //c the mouse's VBL +
    // movement lines (and the two ACIAs) assert through it; on the //e and
    // earlier no sources assert yet, so the seam is shared but quiet.
    m_host.GetInterruptController().SetCpu (m_host.GetCpu()->GetCpu());

    // //c IOU mouse: tick the device from the per-instruction cycle fan-out
    // so VBL-edge latching and paced movement interrupts stay phase-locked
    // to CPU progress (null for every other machine).
    if (m_host.GetMouse() != nullptr)
    {
        m_host.GetCpu()->SetCycleSink (m_host.GetMouse());
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
                        m_host.GetCpu()->PokeByte (addr, static_cast<Byte> (byte));
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
                    m_host.GetCpu()->PokeByte (addr, static_cast<Byte> (byte));
                    addr++;
                }
            }

            romFile.close();
        }
    }

    m_host.GetCpu()->InitForEmulation(*m_host.GetPrng());

    // Connect speaker to CPU cycle counter for audio timestamps
    if (m_host.GetRefs().speaker != nullptr)
    {
        m_host.GetRefs().speaker->SetCycleCounter (m_host.GetCpu()->GetCycleCounterPtr());
    }

    // Issue #67: drive Disk2Controller bit-stream catch-up off the CPU
    // cycle counter so every $C0Ex read/write resyncs the engine to
    // elapsed CPU time before the soft-switch dispatch fires (matches
    // AppleWin's CpuCalcCycles-at-top-of-handler pattern). MachineManager
    // owns both the EmuCpu and the device list, so this is the right
    // wiring point -- the controller is cached into the refs' diskController
    // just above in AddDevices.
    if (m_host.GetRefs().diskController != nullptr)
    {
        m_host.GetRefs().diskController->SetCpuCycleSource (m_host.GetCpu()->GetBusCyclePtr());

        // Motor-idle auto-flush: when the drive spins down (operation done),
        // persist dirty images so writes survive a crash / kill before the
        // next eject or exit. The callback fires on the CPU thread inside
        // Tick, which owns the disk writes, so it races nothing; FlushAll
        // skips clean images and the flush-error reporter surfaces failures.
        m_host.GetRefs().diskController->SetMotorOffFlushCallback ([this] ()
        {
            m_host.GetDiskStore().FlushAll();

            // The disk has just stopped, so this is the quietest moment there
            // is to swap what is under it. One line and no decisions: which
            // bay, whether anything settled and what to do about it are all
            // the store's.
            m_host.GetDiskStore().ApplyPendingReload();
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
        if (!m_services.IsImageWatchDisabled())
        {
            m_host.GetRefs().diskController->SetIdleCallback ([this] ()
            {
                m_host.GetDiskStore().ApplyPendingReload();
            });
        }

        // Restarting after a pick-up. The decision is the store's and the
        // action is the shell's -- a device-layer image store reaching machine
        // lifecycle directly would be a layering inversion.
        m_host.GetDiskStore().SetMachineRestartCallback (m_services.requestPowerCycle);
    }

    // Drive the analog paddle/joystick PREAD timer ($C070 strobe,
    // $C064-$C067 countdown) off the same CPU bus-cycle accumulator so a
    // paddle read measures elapsed cycles since the strobe.
    {
        auto * iieSw = m_host.GetRefs().iieSoftSwitches;

        if (iieSw != nullptr)
        {
            iieSw->SetCpuCycleSource (m_host.GetCpu()->GetBusCyclePtr());
        }

        if (m_host.GetRefs().gamePort != nullptr)
        {
            m_host.GetRefs().gamePort->SetCpuCycleSource (m_host.GetCpu()->GetBusCyclePtr());
        }
    }

Error:
    return hr;
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

void MachineBuilder::SelectVideoMode()
{
    bool                     is80ColMode     = false;
    bool                     altCharSet      = false;
    bool                     doubleHiRes     = false;
    Apple2eSoftSwitchBank *  iieSoftSwitches = m_host.GetRefs().iieSoftSwitches;



    // No machine built yet, or one torn down. The modes are created and
    // cleared together, so text40 answers for all of them.
    if (m_host.GetRefs().text40 == nullptr)
    {
        return;
    }

    // Read soft switch state
    if (m_host.GetRefs().softSwitches)
    {
        m_host.GetSoftSwitchMirror().graphicsMode = m_host.GetRefs().softSwitches->IsGraphicsMode();
        m_host.GetSoftSwitchMirror().mixedMode    = m_host.GetRefs().softSwitches->IsMixedMode();
        m_host.GetSoftSwitchMirror().page2        = m_host.GetRefs().softSwitches->IsPage2();
        m_host.GetSoftSwitchMirror().hiresMode    = m_host.GetRefs().softSwitches->IsHiresMode();
    }

    // Everything the //e bank contributes, gathered under one test: it is
    // null on a ][ / ][+, where the defaults above are the right answer.
    if (iieSoftSwitches != nullptr)
    {
        // When 80STORE is active, $C054/$C055 control aux/main memory
        // selection -- not page 1/page 2. Suppress page2 for rendering.
        if (iieSoftSwitches->Is80Store())
        {
            m_host.GetSoftSwitchMirror().page2 = false;
        }

        is80ColMode = iieSoftSwitches->Is80ColMode();
        altCharSet  = iieSoftSwitches->IsAltCharSet();
        doubleHiRes = iieSoftSwitches->IsDoubleHiRes();
    }

    // Select video mode based on soft switch state
    if (!m_host.GetSoftSwitchMirror().graphicsMode)
    {
        // Text mode: use 80-col on //e if enabled, else 40-col
        m_host.GetRefs().activeVideoMode = is80ColMode
                                             ? static_cast<VideoOutput *> (m_host.GetRefs().text80)
                                             : static_cast<VideoOutput *> (m_host.GetRefs().text40);
    }
    else if (!m_host.GetSoftSwitchMirror().hiresMode)
    {
        // Lo-res graphics
        m_host.GetRefs().activeVideoMode = m_host.GetRefs().loRes;
    }
    else
    {
        // Hi-res graphics -- double hi-res needs DHIRES *and* 80COL together
        // on the //e (FR-019, audit M8). Otherwise standard hi-res.
        bool useDhr = doubleHiRes && is80ColMode;

        m_host.GetRefs().activeVideoMode = useDhr
                                             ? static_cast<VideoOutput *> (m_host.GetRefs().doubleHiRes)
                                             : static_cast<VideoOutput *> (m_host.GetRefs().hiRes);
    }

    // Pass page2 state to the active renderer
    if (m_host.GetRefs().activeVideoMode != nullptr)
    {
        m_host.GetRefs().activeVideoMode->SetPage2 (m_host.GetSoftSwitchMirror().page2);
    }

    // Keep text mode page2-aware for mixed-mode overlay rendering
    m_host.GetRefs().text40->SetPage2 (m_host.GetSoftSwitchMirror().page2);

    // Propagate ALTCHARSET to both text-mode renderers.
    m_host.GetRefs().text40->SetAltCharSet (altCharSet);
    m_host.GetRefs().text80->SetAltCharSet (altCharSet);
}


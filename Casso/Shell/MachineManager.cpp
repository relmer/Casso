#include "Pch.h"

#include "MachineManager.h"

#include "../EmulatorShell.h"
#include "../AssetBootstrap.h"
#include "../DiskSettings.h"
#include "../resource.h"
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
#include "Devices/LanguageCard.h"
#include "Devices/Apple2eMmu.h"
#include "Devices/Apple2cRomBank.h"
#include "Devices/AppleMouse.h"
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
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    WORD  ResolveMachineSpeedCommand (const JsonValue & mergedJson)
    {
        HRESULT            hr      = S_OK;
        const JsonValue *  uiPrefs = nullptr;
        std::string        speed;


        if (mergedJson.GetType() != JsonType::Object)
        {
            return 0;
        }

        hr = mergedJson.GetObject ("$cassoUiPrefs", uiPrefs);
        if (FAILED (hr) || uiPrefs == nullptr)
        {
            return 0;
        }
        _Analysis_assume_ (uiPrefs != nullptr);

        hr = uiPrefs->GetString ("speedMode", speed);
        if (FAILED (hr))
        {
            return 0;
        }

        if (speed == "authentic") return IDM_MACHINE_SPEED_1X;
        if (speed == "double")    return IDM_MACHINE_SPEED_2X;
        if (speed == "maximum")   return IDM_MACHINE_SPEED_MAX;

        return 0;
    }
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
        if (!region.bank.empty())
        {
            continue;
        }

        Word start = region.address;
        Word end   = static_cast<Word> (region.address + region.size - 1);

        auto device = std::make_unique<RamDevice> (start, end);

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

        hr = ReadRomFileBytes (config.systemRom.resolvedPath, fileBytes);

        if (FAILED (hr) || fileBytes.size () < config.systemRom.romBankSize)
        {
            wideError = L"Cannot read banked system ROM: " +
                        std::wstring (config.systemRom.resolvedPath.begin (),
                                      config.systemRom.resolvedPath.end ());
            CBRN (false, wideError.c_str ());
        }

        Word romStart = config.systemRom.address;
        Word romEnd   = static_cast<Word> (config.systemRom.address + config.systemRom.romBankSize - 1);

        auto device = RomDevice::CreateFromData (romStart, romEnd,
                                                 fileBytes.data (),
                                                 config.systemRom.romBankSize);

        m_shell.m_memoryBus.AddDevice (device.get ());
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
        DeviceConfig devCfg;
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

        auto device = m_shell.m_registry.Create (devCfg.type, devCfg, m_shell.m_memoryBus);

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
        }
        else if (devCfg.type == "apple2-softswitches" ||
                 devCfg.type == "apple2e-softswitches")
        {
            m_shell.m_refs.softSwitches = static_cast<AppleSoftSwitchBank *> (device.get());
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
        auto * iieKbd = dynamic_cast<Apple2eKeyboard *>       (m_shell.m_refs.keyboard);
        auto * iieSw  = dynamic_cast<Apple2eSoftSwitchBank *> (m_shell.m_refs.softSwitches);

        if (iieKbd != nullptr && iieSw != nullptr)
        {
            iieKbd->SetSoftSwitchSibling (iieSw);
            iieSw->SetKeyboard           (iieKbd);
        }

        if (iieKbd != nullptr && m_shell.m_refs.speaker != nullptr)
        {
            iieKbd->SetSpeakerSibling (m_shell.m_refs.speaker);
        }

        if (iieKbd != nullptr && m_shell.m_mmu != nullptr)
        {
            iieKbd->SetMmu (m_shell.m_mmu.get());
        }

        if (iieKbd != nullptr && m_shell.m_videoTiming != nullptr)
        {
            iieKbd->SetVideoTiming (m_shell.m_videoTiming.get());
        }

        if (iieSw != nullptr && m_shell.m_videoTiming != nullptr)
        {
            iieSw->SetVideoTiming (m_shell.m_videoTiming.get());
        }

        if (iieSw != nullptr && m_shell.m_mmu != nullptr)
        {
            iieSw->SetMmu (m_shell.m_mmu.get());
        }
    }

    // Initialize the //e MMU once main RAM exists. The MMU rebinds the
    // page table for $0000-$BFFF based on RAMRD/RAMWRT/ALTZP/80STORE.
    if (m_shell.m_mmu != nullptr && m_shell.m_refs.mainRamDev != nullptr)
    {
        auto * iieSw = dynamic_cast<Apple2eSoftSwitchBank *> (m_shell.m_refs.softSwitches);

        HRESULT hrMmu = m_shell.m_mmu->Initialize (
            &m_shell.m_memoryBus,
            m_shell.m_refs.mainRamDev,
            nullptr,
            nullptr,
            nullptr,
            iieSw);

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
            DeviceConfig devCfg;
            devCfg.type    = slot.device;
            devCfg.slot    = slot.slot;
            devCfg.hasSlot = true;

            auto device = m_shell.m_registry.Create (devCfg.type, devCfg, m_shell.m_memoryBus);

            if (!device)
            {
                DEBUGMSG (L"Warning: Unknown slot device type '%hs'\n", devCfg.type.c_str());
            }
            else
            {
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
    // //c IOU mouse (US4): destroyed with the outgoing machine, rebuilt
    // below for the //c. The keyboard/soft-switch bank holding the old
    // pointer are torn down with the same machine, and the CPU thread is
    // stopped during construction, so no stale-pointer window exists.
    m_shell.m_mouse.reset ();

    if (m_shell.m_config.systemRom.romBankSize != 0)
    {
        auto iwm = std::make_unique<Disk2Controller> (6);
        iwm->SetIwmMode (true);
        m_shell.m_memoryBus.AddDevice (iwm.get ());
        m_shell.m_ownedDevices.push_back (std::move (iwm));

        // //c built-in IOU mouse (US4): not a bus device -- the keyboard
        // ($C048 ack, $C063 button) and soft-switch bank ($C015/$C017/$C019
        // status, $C066/$C067 direction, $C058-$C05F IOU programming,
        // $C078/$C079 gate, $C070 VBL clear) forward its register surface;
        // the real ROM 4 mouse firmware (phantom slot 7) runs against it.
        // IRQ lines aggregate through the shared interrupt controller; the
        // CPU cycle fan-out tick is wired in CreateCpu.
        {
            m_shell.m_mouse = std::make_unique<AppleMouse> ();

            HRESULT  hrIc = m_shell.m_mouse->AttachInterruptController (&m_shell.m_interruptController);
            IGNORE_RETURN_VALUE (hrIc, S_OK);

            m_shell.m_mouse->SetBus (&m_shell.m_memoryBus);

            if (m_shell.m_videoTiming != nullptr)
            {
                m_shell.m_mouse->SetVideoTiming (m_shell.m_videoTiming.get ());
            }

            auto * iieKbd = dynamic_cast<Apple2eKeyboard *>       (m_shell.m_refs.keyboard);
            auto * iieSw  = dynamic_cast<Apple2eSoftSwitchBank *> (m_shell.m_refs.softSwitches);

            if (iieKbd != nullptr)
            {
                iieKbd->SetMouse (m_shell.m_mouse.get ());
            }
            if (iieSw != nullptr)
            {
                iieSw->SetMouse (m_shell.m_mouse.get ());
            }
        }

        // //c dual 6551 ACIA serial ports (phantom slots 1 & 2): port 1
        // ($C098) = printer, port 2 ($C0A8) = modem. Built in like the IWM
        // (the //c has no config slots) -- the serial firmware is part of the
        // internal //c ROM. Each raises IRQs through the shared interrupt
        // controller. v1 endpoints are loopback (comms self-test); the serial
        // printer-endpoint bridge + Hardware-tab endpoint selector are
        // downstream work in issue #87.
        for (int slot = 1; slot <= 2; ++slot)
        {
            Word  base = static_cast<Word> (Acia6551::kSlotIoBase
                                            + slot * Acia6551::kSlotIoStride
                                            + Acia6551::kAciaRegOffset);
            auto  acia = std::make_unique<Acia6551> (base);

            HRESULT  hrIc = acia->AttachInterruptController (&m_shell.m_interruptController);
            IGNORE_RETURN_VALUE (hrIc, S_OK);

            auto  loopback = std::make_unique<AciaLoopbackEndpoint> (acia.get ());
            acia->SetEndpoint (loopback.get ());

            m_shell.m_memoryBus.AddDevice (acia.get ());
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
            auto  src = std::make_unique<Disk2AudioSource>();
            float panL = DriveAudioMixer::kSpeakerCenter;
            float panR = DriveAudioMixer::kSpeakerCenter;

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

    // Feed real disk head / motor / door events to drive 0's source only when
    // the machine actually has the Disk ][ controller realized.
    if (m_shell.m_refs.diskController != nullptr && !m_shell.m_diskAudioSources.empty())
    {
        m_shell.m_refs.diskController->SetAudioSink (m_shell.m_diskAudioSources[0].get());
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireLanguageCard
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::WireLanguageCard()
{
    LanguageCard *  lc        = nullptr;
    RomDevice    *  romDevice = nullptr;



    // Find the LanguageCard device
    for (auto & dev : m_shell.m_ownedDevices)
    {
        lc = dynamic_cast<LanguageCard *> (dev.get());

        if (lc != nullptr)
        {
            break;
        }
    }

    if (lc == nullptr)
    {
        return;
    }

    // Find a ROM device covering $D000-$FFFF
    for (const auto & entry : m_shell.m_memoryBus.GetEntries())
    {
        auto * rom = dynamic_cast<RomDevice *> (entry.device);

        if (rom != nullptr && entry.start <= 0xD000 && entry.end >= 0xFFFF)
        {
            romDevice = rom;
            break;
        }
    }

    if (romDevice == nullptr)
    {
        return;
    }

    Word romStart = romDevice->GetStart();

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
    auto lcBank = std::make_unique<LanguageCardBank> (*lc);
    m_shell.m_memoryBus.AddDevice (lcBank.get());
    m_shell.m_ownedDevices.push_back (std::move (lcBank));

    // //e wiring: LC needs the MMU (for ALTZP routing) and the
    // keyboard sibling needs the LC pointer for $C011/$C012 status
    // reads.
    if (m_shell.m_mmu != nullptr)
    {
        lc->SetMmu (m_shell.m_mmu.get());
    }

    auto * iieKbd = dynamic_cast<Apple2eKeyboard *> (m_shell.m_refs.keyboard);

    if (iieKbd != nullptr)
    {
        iieKbd->SetLanguageCard (lc);
    }

    auto * iieSw = dynamic_cast<Apple2eSoftSwitchBank *> (m_shell.m_refs.softSwitches);

    if (iieSw != nullptr)
    {
        iieSw->SetLanguageCard (lc);
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
    std::ifstream   file (path, std::ios::binary | std::ios::ate);

    if (!file.good ())
    {
        return E_FAIL;
    }

    std::streamoff  size = file.tellg ();

    if (size <= 0)
    {
        return E_FAIL;
    }

    file.seekg (0, std::ios::beg);
    out.resize (static_cast<size_t> (size));
    file.read (reinterpret_cast<char *> (out.data ()), size);

    return file.good () ? S_OK : E_FAIL;
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

void MachineManager::WireApple2cRomBank ()
{
    const RomReference &  sysRom = m_shell.m_config.systemRom;

    if (sysRom.romBankSize == 0)
    {
        return;
    }

    Apple2eMmu            * mmu = m_shell.m_mmu.get ();
    Apple2eSoftSwitchBank * sw  = dynamic_cast<Apple2eSoftSwitchBank *> (m_shell.m_refs.softSwitches);
    LanguageCard          * lc  = nullptr;

    for (auto & dev : m_shell.m_ownedDevices)
    {
        lc = dynamic_cast<LanguageCard *> (dev.get ());

        if (lc != nullptr)
        {
            break;
        }
    }

    if (mmu == nullptr || sw == nullptr || lc == nullptr)
    {
        DEBUGMSG (L"WireApple2cRomBank: missing MMU/soft-switches/LC; banking disabled\n");
        return;
    }

    std::vector<Byte>   fileBytes;
    size_t              twoBanks = static_cast<size_t> (sysRom.romBankSize) * 2;

    if (FAILED (ReadRomFileBytes (sysRom.resolvedPath, fileBytes)) ||
        fileBytes.size () < twoBanks)
    {
        DEBUGMSG (L"WireApple2cRomBank: cannot read both ROM banks; banking disabled\n");
        return;
    }

    std::vector<Byte>   bank0 (fileBytes.begin (),                     fileBytes.begin () + sysRom.romBankSize);
    std::vector<Byte>   bank1 (fileBytes.begin () + sysRom.romBankSize, fileBytes.begin () + twoBanks);

    m_shell.m_apple2cRomBank = std::make_unique<Apple2cRomBank> (*lc, *mmu);
    m_shell.m_apple2cRomBank->SetBankImages (std::move (bank0), std::move (bank1));
    sw->SetRomBankSwitch (m_shell.m_apple2cRomBank.get ());

    // //c: no card slots -> $C100-$CFFF is always the internal firmware.
    mmu->GetCxxxRouter ()->SetNoExternalSlots (true);
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
    if (!m_shell.m_cpu)
    {
        return;
    }

    Byte * mainRam = const_cast<Byte *> (m_shell.m_cpu->GetMemory());

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
    if (!m_shell.m_cpu)
    {
        return;
    }

    if (m_shell.m_mmu != nullptr)
    {
        return;
    }

    Byte * mainRam = const_cast<Byte *> (m_shell.m_cpu->GetMemory());

    for (int page = 0x04; page <= 0x07; page++)
    {
        Byte * p = mainRam + (page * 0x100);
        m_shell.m_memoryBus.SetReadPage  (page, p);
        m_shell.m_memoryBus.SetWritePage (page, p);
    }
    for (int page = 0x20; page <= 0x3F; page++)
    {
        Byte * p = mainRam + (page * 0x100);
        m_shell.m_memoryBus.SetReadPage  (page, p);
        m_shell.m_memoryBus.SetWritePage (page, p);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateVideoModes
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::CreateVideoModes()
{
    auto textMode = std::make_unique<AppleTextMode> (m_shell.m_memoryBus, m_shell.m_charRom);
    m_shell.m_refs.activeVideoMode = textMode.get();
    m_shell.m_videoModes.push_back (std::move (textMode));

    auto loResMode = std::make_unique<AppleLoResMode> (m_shell.m_memoryBus);
    m_shell.m_videoModes.push_back (std::move (loResMode));

    auto hiResMode = std::make_unique<AppleHiResMode> (m_shell.m_memoryBus);
    m_shell.m_videoModes.push_back (std::move (hiResMode));

    auto doubleHiResMode = std::make_unique<AppleDoubleHiResMode> (m_shell.m_memoryBus);
    m_shell.m_videoModes.push_back (std::move (doubleHiResMode));

    // Index 4: 80-column text (used on //e). Wired with aux memory
    // from the Apple2eMmu when present.
    auto text80 = std::make_unique<Apple80ColTextMode> (m_shell.m_memoryBus, m_shell.m_charRom);

    Byte * auxBuf = GetAuxRamBuffer();

    if (auxBuf != nullptr)
    {
        text80->SetAuxMemory (auxBuf);

        // DHR also needs aux memory access (FR-019). Index 3 =
        // AppleDoubleHiResMode.
        auto * dhr = static_cast<AppleDoubleHiResMode *> (m_shell.m_videoModes[3].get());
        dhr->SetAuxMemory (auxBuf);
    }

    m_shell.m_videoModes.push_back (std::move (text80));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateCpu
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
    // wrong part silently is the exact defect this seam removes, so a
    // strategy that cannot be built fails the machine build here.
    hr = CpuFactory::Create (config.cpu, m_shell.m_memoryBus, cpu);
    CHR (hr);

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
        m_shell.m_cpu->SetCycleSink (m_shell.m_mouse.get ());
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
            m_shell.m_diskStore.FlushAll ();
        });
    }

    // Drive the analog paddle/joystick PREAD timer ($C070 strobe,
    // $C064-$C067 countdown) off the same CPU bus-cycle accumulator so a
    // paddle read measures elapsed cycles since the strobe.
    {
        auto * iieSw = dynamic_cast<Apple2eSoftSwitchBank *> (m_shell.m_refs.softSwitches);

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
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineManager::SwitchMachine (const std::wstring & machineName)
{
    HRESULT                hr             = S_OK;
    std::vector<fs::path>  searchPaths;
    fs::path               configRelPath;
    fs::path               configPath;
    std::ifstream          configFile;
    bool                   configGood     = false;
    std::stringstream      ss;
    std::string            jsonText;
    std::vector<fs::path>  romSearchPaths;
    std::string            error;
    MachineConfig          newConfig;
    std::string            machineNameNarrow = fs::path (machineName).string();
    JsonValue              defaultJson;
    JsonValue              mergedJson;
    JsonParseError         parseErr;
    WORD                   speedCmd = 0;
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

    CBRN (!configPath.empty(),
          std::format (L"Machine config not found: {}", machineName).c_str());

    configFile.open (configPath);
    configGood = configFile.good();
    CBRN (configGood,
          std::format (L"Cannot open machine config:\n{}", configPath.wstring()).c_str());

    ss << configFile.rdbuf();
    jsonText = ss.str();

    if (SUCCEEDED (JsonParser::Parse (jsonText, defaultJson, parseErr)) &&
        m_shell.m_userConfigStore != nullptr)
    {
        if (SUCCEEDED (m_shell.m_userConfigStore->Load (machineNameNarrow,
                                                         defaultJson,
                                                         m_shell.m_uiFs,
                                                         mergedJson)))
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
                WORD               colorCmd  = 0;

                if (SUCCEEDED (mergedJson.GetObject ("$cassoUiPrefs", uiPrefs)) &&
                    uiPrefs != nullptr &&
                    SUCCEEDED (uiPrefs->GetString ("colorMode", colorMode)))
                {
                    if      (colorMode == "color")  { colorCmd = IDM_VIEW_COLOR; }
                    else if (colorMode == "green")  { colorCmd = IDM_VIEW_GREEN; }
                    else if (colorMode == "amber")  { colorCmd = IDM_VIEW_AMBER; }
                    else if (colorMode == "white")  { colorCmd = IDM_VIEW_WHITE; }

                    if (colorCmd != 0)
                    {
                        m_shell.HandleCommand (colorCmd);
                    }
                }

                // //c external drive: adopt the switched-to machine's persisted
                // connected state so the second drive-mount widget matches the
                // saved setting once ReflowChromeForMachineChange relays the
                // chrome. Defaults to not-connected; harmless on non-//c
                // machines (ShouldShowExternalDrive ignores it when the system
                // ROM is not banked).
                {
                    const JsonValue *  extPrefs  = nullptr;
                    bool               connected = false;

                    if (SUCCEEDED (mergedJson.GetObject ("$cassoUiPrefs", extPrefs)) &&
                        extPrefs != nullptr)
                    {
                        HRESULT  hrExt = extPrefs->GetBool ("externalDriveConnected", connected);
                        IGNORE_RETURN_VALUE (hrExt, S_OK);
                    }
                    m_shell.m_externalDriveConnected = connected;

                    // //c mouse peripheral: adopt the switched-to machine's
                    // persisted connected state (default CONNECTED, FR-013b).
                    bool  mouseConn = true;
                    if (extPrefs != nullptr)
                    {
                        HRESULT  hrM = extPrefs->GetBool ("mouseConnected", mouseConn);
                        IGNORE_RETURN_VALUE (hrM, S_OK);
                    }
                    m_shell.m_mouseConnected = mouseConn;
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
        auto * iieSwitches = dynamic_cast<Apple2eSoftSwitchBank *> (m_shell.m_refs.softSwitches);
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

    if (speedCmd != 0)
    {
        m_shell.HandleCommand (speedCmd);
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
////////////////////////////////////////////////////////////////////////////////

void MachineManager::SelectVideoMode()
{
    if (m_shell.m_videoModes.size() < 3)
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

    // When 80STORE is active on the //e, $C054/$C055 control aux/main
    // memory selection -- not page 1/page 2. Suppress page2 for video
    // rendering.
    auto * iieSoftSwitches = dynamic_cast<Apple2eSoftSwitchBank *> (m_shell.m_refs.softSwitches);

    if (iieSoftSwitches != nullptr && iieSoftSwitches->Is80Store())
    {
        m_shell.m_page2 = false;
    }

    bool is80ColMode  = iieSoftSwitches != nullptr && iieSoftSwitches->Is80ColMode();
    bool altCharSet   = iieSoftSwitches != nullptr && iieSoftSwitches->IsAltCharSet();

    // Select video mode based on soft switch state
    if (!m_shell.m_graphicsMode)
    {
        // Text mode: use 80-col on //e if enabled, else 40-col
        if (is80ColMode && m_shell.m_videoModes.size() > 4)
        {
            m_shell.m_refs.activeVideoMode = m_shell.m_videoModes[4].get();
        }
        else
        {
            m_shell.m_refs.activeVideoMode = m_shell.m_videoModes[0].get();
        }
    }
    else if (!m_shell.m_hiresMode)
    {
        // Lo-res graphics
        m_shell.m_refs.activeVideoMode = m_shell.m_videoModes[1].get();
    }
    else
    {
        // Hi-res graphics -- use DHR (index 3) when DHIRES + 80COL are
        // both active on the //e (FR-019, audit M8). Otherwise standard
        // hi-res (index 2).
        bool useDhr = iieSoftSwitches != nullptr
                   && iieSoftSwitches->IsDoubleHiRes()
                   && is80ColMode
                   && m_shell.m_videoModes.size() > 3;

        if (useDhr)
        {
            m_shell.m_refs.activeVideoMode = m_shell.m_videoModes[3].get();
        }
        else
        {
            m_shell.m_refs.activeVideoMode = m_shell.m_videoModes[2].get();
        }
    }

    // Pass page2 state to the active renderer
    if (m_shell.m_refs.activeVideoMode != nullptr)
    {
        m_shell.m_refs.activeVideoMode->SetPage2 (m_shell.m_page2);
    }

    // Keep text mode page2-aware for mixed-mode overlay rendering
    m_shell.m_videoModes[0]->SetPage2 (m_shell.m_page2);

    // Propagate ALTCHARSET to both text-mode renderers.
    static_cast<AppleTextMode *> (m_shell.m_videoModes[0].get())->SetAltCharSet (altCharSet);

    if (m_shell.m_videoModes.size() > 4)
    {
        static_cast<Apple80ColTextMode *> (m_shell.m_videoModes[4].get())->SetAltCharSet (altCharSet);
    }
}

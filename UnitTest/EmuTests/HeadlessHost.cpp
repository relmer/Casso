#include "../CassoEmuCore/Pch.h"

#include "HeadlessHost.h"

#include "Core/MemoryBusCpu.h"
#include "Devices/RomDevice.h"


namespace
{
    static constexpr Word    kSystemRomStart   = 0xC000;
    static constexpr Word    kCxxxRomStart     = 0xC100;
    static constexpr Word    kCxxxRomEnd       = 0xCFFF;
    static constexpr Word    kLcRomStart       = 0xD000;
    static constexpr Word    kRamEnd           = 0xBFFF;
    static constexpr size_t  kSystemRomSize    = 0x4000;     // 16 KiB apple2e.rom
    static constexpr size_t  kCxxxRomSize      = 0x0F00;     // $C100-$CFFF (3840 bytes)
    static constexpr size_t  kLcRomSize        = 0x3000;     // $D000-$FFFF (12 KiB)
    static constexpr size_t  kCxxxRomOffset    = kCxxxRomStart - kSystemRomStart;
    static constexpr size_t  kLcRomOffset      = kLcRomStart   - kSystemRomStart;
    static constexpr int     kRamPageCount     = 0xC0;       // pages $00-$BF
    static constexpr int     kPageSize         = 0x100;
    static constexpr int     kCpuStepBatch     = 64;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessHost::BuildCommon
//
//  Stamps out a fully-mocked EmulatorCore: pinned Prng, MockHostShell,
//  FixtureProvider rooted at UnitTest/Fixtures/, and the IAudioSink the
//  host shell hands out. Constitution §II — no Win32, no audio device,
//  no host filesystem outside fixtures, no registry, no network.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeadlessHost::BuildCommon (HeadlessMachineKind kind, EmulatorCore & outCore)
{
    HRESULT     hr = S_OK;

    outCore.machineKind = kind;
    outCore.prng        = std::make_unique<Prng> (kPinnedSeed);
    outCore.host        = std::make_unique<MockHostShell> ();
    outCore.fixtures    = std::make_unique<FixtureProvider> ();
    outCore.audioSink   = nullptr;

    hr = outCore.host->OpenAudioDevice (outCore.audioSink);
    if (FAILED (hr))
    {
        goto Error;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessHost::BuildAppleII
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeadlessHost::BuildAppleII (EmulatorCore & outCore)
{
    return BuildCommon (HeadlessMachineKind::AppleII, outCore);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessHost::BuildAppleIIPlus
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeadlessHost::BuildAppleIIPlus (EmulatorCore & outCore)
{
    return BuildCommon (HeadlessMachineKind::AppleIIPlus, outCore);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessHost::BuildAppleIIe
//
//  Wires a full //e from `apple2e.rom` (loaded via the IFixtureProvider
//  -- no host filesystem access). Mirrors the production wiring order
//  from EmulatorShell::Initialize so the deterministic harness sees the
//  same memory map / banking semantics the real shell exposes:
//
//    1. MemoryBus + RamDevice + AppleIIeMmu (skips bus for MMU)
//    2. Internal devices (keyboard, speaker, soft switches, language card)
//    3. Sibling pointers (kbd/ss/speaker/mmu/lc/videoTiming cross-wiring)
//    4. mmu->Initialize -- adds CxxxRomRouter to the bus, rebinds page table
//    5. Carve apple2e.rom into the LC ($D000-$FFFF) and the CxxxRomRouter
//       ($C100-$CFFF). The system-rom RomDevice is intentionally NOT added
//       to the bus -- the LC bank intercepts $D000-$FFFF and the CxxxRomRouter
//       owns $C100-$CFFF (audit C8 carryover).
//    6. LanguageCardBank claims $D000-$FFFF.
//    7. EmuCpu + VideoTiming wiring; copy ROM bytes into the CPU's internal
//       memory[] so PeekByte/disassembly + the reset-vector ReadWord see them.
//    8. WirePageTable equivalent: bind every $0000-$BFFF page to the CPU's
//       memory[] buffer (the //e MMU rebinds during banking changes).
//    9. CPU InitForEmulation -- randomizes RAM and reads the reset vector.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeadlessHost::BuildAppleIIe (EmulatorCore & outCore)
{
    HRESULT                hr          = S_OK;
    std::vector<uint8_t>   romBytes;
    Byte                 * mainRamBase = nullptr;
    int                    page;

    hr = BuildCommon (HeadlessMachineKind::AppleIIe, outCore);
    if (FAILED (hr))
    {
        goto Error;
    }

    hr = outCore.fixtures->OpenFixture ("apple2e.rom", romBytes);
    if (FAILED (hr))
    {
        goto Error;
    }

    if (romBytes.size () != kSystemRomSize)
    {
        hr = E_UNEXPECTED;
        goto Error;
    }

    outCore.bus          = std::make_unique<MemoryBus> ();
    outCore.mainRam      = std::make_unique<RamDevice> (0x0000, kRamEnd);
    outCore.videoTiming  = std::make_unique<VideoTiming> ();
    outCore.mmu          = std::make_unique<AppleIIeMmu> ();
    outCore.keyboard     = std::make_unique<AppleIIeKeyboard> (outCore.bus.get ());
    outCore.softSwitches = std::make_unique<AppleIIeSoftSwitchBank> (outCore.bus.get ());
    outCore.speaker      = std::make_unique<AppleSpeaker> ();
    outCore.languageCard = std::make_unique<LanguageCard> (*outCore.bus);
    outCore.lcBank       = std::make_unique<LanguageCardBank> (*outCore.languageCard);

    outCore.bus->AddDevice (outCore.mainRam.get ());
    outCore.bus->AddDevice (outCore.keyboard.get ());
    outCore.bus->AddDevice (outCore.speaker.get ());
    outCore.bus->AddDevice (outCore.softSwitches.get ());
    outCore.bus->AddDevice (outCore.languageCard.get ());

    outCore.keyboard->SetSoftSwitchSibling (outCore.softSwitches.get ());
    outCore.softSwitches->SetKeyboard      (outCore.keyboard.get ());
    outCore.keyboard->SetSpeakerSibling    (outCore.speaker.get ());
    outCore.keyboard->SetMmu               (outCore.mmu.get ());
    outCore.keyboard->SetVideoTiming       (outCore.videoTiming.get ());
    outCore.softSwitches->SetVideoTiming   (outCore.videoTiming.get ());
    outCore.softSwitches->SetMmu           (outCore.mmu.get ());

    hr = outCore.mmu->Initialize (
        outCore.bus.get (),
        outCore.mainRam.get (),
        nullptr,
        nullptr,
        nullptr,
        outCore.softSwitches.get ());
    if (FAILED (hr))
    {
        goto Error;
    }

    {
        std::vector<Byte>   cxxxData (kCxxxRomSize);
        std::vector<Byte>   lcRom    (kLcRomSize);

        std::copy (romBytes.begin () + kCxxxRomOffset,
                   romBytes.begin () + kCxxxRomOffset + kCxxxRomSize,
                   cxxxData.begin ());
        std::copy (romBytes.begin () + kLcRomOffset,
                   romBytes.begin () + kLcRomOffset + kLcRomSize,
                   lcRom.begin ());

        outCore.mmu->AttachInternalCxxxRom (std::move (cxxxData));
        outCore.languageCard->SetRomData    (lcRom);
    }

    outCore.bus->AddDevice (outCore.lcBank.get ());

    outCore.languageCard->SetMmu  (outCore.mmu.get ());
    outCore.keyboard->SetLanguageCard     (outCore.languageCard.get ());
    outCore.softSwitches->SetLanguageCard (outCore.languageCard.get ());

    outCore.cpu = std::make_unique<EmuCpu> (*outCore.bus);
    outCore.cpu->SetVideoTiming (outCore.videoTiming.get ());
    outCore.speaker->SetCycleCounter (outCore.cpu->GetCycleCounterPtr ());

    for (size_t i = 0; i < romBytes.size (); i++)
    {
        outCore.cpu->PokeByte (
            static_cast<Word> (kSystemRomStart + i),
            romBytes[i]);
    }

    mainRamBase = const_cast<Byte *> (outCore.cpu->GetMemory ());

    for (page = 0; page < kRamPageCount; page++)
    {
        Byte * pagePtr = mainRamBase + (page * kPageSize);
        outCore.bus->SetReadPage  (page, pagePtr);
        outCore.bus->SetWritePage (page, pagePtr);
    }

    outCore.cpu->InitForEmulation ();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessHost::BuildAppleIIeWithDiskII
//
//  Phase 11 (T097-T104). Extends BuildAppleIIe by attaching the Disk II
//  boot ROM (disk2.rom) to slot 6 via mmu->AttachSlotRom and adding a
//  DiskIIController + DiskImageStore to outCore. Tests then mount
//  synthetic disks via the store and call core.RunCycles to drive the
//  controller in lock-step with the CPU. No third-party disk software is
//  loaded — every fixture is built in memory at test-init time.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeadlessHost::BuildAppleIIeWithDiskII (EmulatorCore & outCore)
{
    HRESULT                hr = S_OK;
    std::vector<uint8_t>   slot6Rom;

    hr = BuildAppleIIe (outCore);
    if (FAILED (hr))
    {
        goto Error;
    }

    hr = outCore.fixtures->OpenFixture ("disk2.rom", slot6Rom);
    if (FAILED (hr))
    {
        goto Error;
    }

    outCore.mmu->AttachSlotRom (6, std::move (slot6Rom));

    outCore.diskController = std::make_unique<DiskIIController> (6);
    outCore.diskStore      = std::make_unique<DiskImageStore> ();

    outCore.bus->AddDevice (outCore.diskController.get ());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCore::PowerCycle
//
//  Phase 7. Re-seeds every DRAM-owning device from the shared Prng then
//  runs the 6502 /RESET sequence. Mirrors EmulatorShell::PowerCycle so
//  the headless harness exposes the same cold-boot semantics tests need.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCore::PowerCycle ()
{
    if (!HasAppleIIe ())
    {
        return;
    }

    bus->PowerCycleAll (*prng);
    mmu->OnPowerCycle  (*prng);
    cpu->PowerCycle    (*prng);

    if (videoTiming != nullptr)
    {
        videoTiming->PowerCycle (*prng);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCore::SoftReset
//
//  Phase 8 (T079). Mirrors EmulatorShell::SoftReset: fans the
//  SoftReset-shaped event out across the bus, then explicitly resets
//  the MMU + VideoTiming + CPU. RAM contents (main, aux, all six LC
//  banks) are preserved by design — only flag state and the CPU's
//  post-reset register-file state change.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCore::SoftReset ()
{
    if (!HasAppleIIe ())
    {
        return;
    }

    bus->SoftResetAll ();
    mmu->OnSoftReset  ();

    if (videoTiming != nullptr)
    {
        videoTiming->SoftReset ();
    }

    cpu->SoftReset ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCore::RunCycles
//
//  Pumps the EmuCpu in small batches until the cycle counter reaches the
//  requested budget. Batching keeps the per-instruction cycle accounting
//  tight without sacrificing throughput on the 5M-cycle cold-boot tests.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorCore::RunCycles (uint64_t cycleBudget)
{
    uint64_t   target;
    int        i;
    uint32_t   stepCycles;

    if (!HasAppleIIe ())
    {
        return;
    }

    target = cpu->GetTotalCycles () + cycleBudget;

    while (cpu->GetTotalCycles () < target)
    {
        for (i = 0; i < kCpuStepBatch; i++)
        {
            cpu->StepOne ();
            stepCycles = cpu->GetLastInstructionCycles ();
            cpu->AddCycles (stepCycles);

            if (diskController != nullptr)
            {
                diskController->Tick (stepCycles);
            }
        }
    }
}
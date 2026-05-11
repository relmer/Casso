#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "HeadlessHost.h"
#include "MemoryProbeHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace
{
    static constexpr uint64_t   kColdBootCycles      = 5000000ULL;
    static constexpr Word       kProbeAddrMain       = 0x4000;
    static constexpr Word       kProbeAddrZp         = 0x0080;
    static constexpr Word       kProbeAddrLcBank     = 0xD000;
    static constexpr Word       kProbeAddrLcHigh     = 0xE100;
    static constexpr Word       kProbeAddrHires      = 0x2000;
    static constexpr Word       kResetVector         = 0xFFFC;
    static constexpr Byte       kPatternMain         = 0xAA;
    static constexpr Byte       kPatternAux          = 0x55;
    static constexpr Byte       kPatternMainZp       = 0x37;
    static constexpr Byte       kPatternAuxZp        = 0xC4;
    static constexpr Byte       kPatternMainBank1    = 0x11;
    static constexpr Byte       kPatternMainBank2    = 0x22;
    static constexpr Byte       kPatternAuxBank1     = 0x33;
    static constexpr Byte       kPatternAuxBank2     = 0x44;
    static constexpr Byte       kPatternHighMain     = 0x66;
    static constexpr Byte       kPatternHighAux      = 0x77;
    static constexpr Byte       kPatternHires        = 0x99;
    static constexpr Byte       kPostResetSp         = 0xFD;

    static constexpr Word   kSwitch80StoreOff   = 0xC000;
    static constexpr Word   kSwitch80StoreOn    = 0xC001;
    static constexpr Word   kSwitchPage2Off     = 0xC054;
    static constexpr Word   kSwitchPage2On      = 0xC055;
    static constexpr Word   kSwitchHiresOff     = 0xC056;
    static constexpr Word   kSwitchHiresOn      = 0xC057;

    static constexpr Word   kLcReadEvenBank2    = 0xC080;
    static constexpr Word   kLcOddBank2A        = 0xC081;
    static constexpr Word   kLcOddBank2B        = 0xC083;
    static constexpr Word   kLcReadEvenBank1    = 0xC088;
    static constexpr Word   kLcOddBank1A        = 0xC089;
    static constexpr Word   kLcOddBank1B        = 0xC08B;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Phase8IntegrationTests
//
//  User Story 3 (P1). Drives deterministic //e-specific scenarios across
//  the headless //e built by HeadlessHost::BuildAppleIIe — proves the
//  MMU + Language Card rewrites from Phases 2-3 land the right bytes in
//  the right buffers end-to-end.
//
//  Constitution §II: every test uses HeadlessHost + IFixtureProvider
//  only; no host filesystem, no Win32, no audio device.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Phase8IntegrationTests)
{
public:

    ////////////////////////////////////////////////////////////////////////
    //
    //  Helper: Boot a //e to the cold-boot prompt and rebind the bus
    //  page table off the CPU's cold-boot scratch buffer onto the
    //  MMU's mainRam/auxRam buffers.
    //
    ////////////////////////////////////////////////////////////////////////

    static void BootAndRebind (HeadlessHost & host, EmulatorCore & core)
    {
        HRESULT   hr;

        hr = host.BuildAppleIIe (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIe must succeed");
        Assert::IsTrue (core.HasAppleIIe (), L"//e wiring must be complete");

        core.PowerCycle ();
        core.RunCycles  (kColdBootCycles);

        MemoryProbeHelpers::RebindMainBaseline (core);
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Phase 8 / T075 — RAMRD/RAMWRT route main vs aux independently.
    //
    //  Acceptance scenario 1 (FR-005, audit §2). Write 0xAA into main
    //  $4000, 0x55 into aux $4000; both copies must coexist; reads
    //  selected via RAMRD pick the right side.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase8_RamRd_RamWrt_RouteAuxIndependently)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        Byte           mainValue;
        Byte           auxValue;

        BootAndRebind (host, core);

        MemoryProbeHelpers::WriteMain (core, kProbeAddrMain, kPatternMain);
        MemoryProbeHelpers::WriteAux  (core, kProbeAddrMain, kPatternAux);

        mainValue = MemoryProbeHelpers::ReadMain (core, kProbeAddrMain);
        auxValue  = MemoryProbeHelpers::ReadAux  (core, kProbeAddrMain);

        Assert::AreEqual (kPatternMain, mainValue,
            L"$4000 main RAM must hold 0xAA after RAMWRT=0 store");
        Assert::AreEqual (kPatternAux, auxValue,
            L"$4000 aux RAM must hold 0x55 after RAMWRT=1 store");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Phase 8 / T076a — LC pre-write any-two-odd-reads enables WRITERAM.
    //
    //  Acceptance scenario 2 (FR-008, FR-009, audit M6). Start by
    //  clearing WRITERAM (read at an even $C08x switch). Two odd-
    //  address reads then re-arm WRITERAM and the next $D000 store
    //  lands in LC bank2 main.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase8_LcPreWrite_AnyTwoOddReads_EnablesWrite)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        BootAndRebind (host, core);

        core.bus->ReadByte (kLcReadEvenBank2);
        Assert::IsFalse (core.languageCard->IsWriteRam (),
            L"Even-address read must clear WRITERAM");

        core.bus->ReadByte (kLcOddBank2A);
        Assert::IsFalse (core.languageCard->IsWriteRam (),
            L"One odd read must NOT yet enable WRITERAM");

        core.bus->ReadByte (kLcOddBank2B);
        Assert::IsTrue (core.languageCard->IsWriteRam (),
            L"Second odd read at a different $C08x must enable WRITERAM");
        Assert::IsTrue (core.languageCard->IsBank2 (),
            L"$C08x reads in the $C080-$C087 range must select bank2");

        core.languageCard->WriteRam (kProbeAddrLcBank, kPatternMainBank2);

        Assert::AreEqual (kPatternMainBank2,
            core.languageCard->ReadRam (kProbeAddrLcBank),
            L"Write must land in LC bank2 main when WRITERAM is enabled");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Phase 8 / T076b — Intervening write to an odd $C08x clears the
    //  pre-write arm.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase8_LcPreWrite_InterveningWriteResets)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        BootAndRebind (host, core);

        core.bus->ReadByte  (kLcReadEvenBank2);
        core.bus->ReadByte  (kLcOddBank2A);
        core.bus->WriteByte (kLcOddBank2A, 0);

        Assert::AreEqual (0, core.languageCard->GetPreWriteCount (),
            L"Write to an odd $C08x must clear the pre-write counter");

        core.bus->ReadByte (kLcOddBank2B);

        Assert::IsFalse (core.languageCard->IsWriteRam (),
            L"After an intervening write, one further odd read alone "
            L"must NOT enable WRITERAM");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Phase 8 / T076c — Power-on default is BANK2 | WRITERAM (pre-armed).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase8_LcPowerOnDefaultsToBank2WriteRamPrearmed)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        HRESULT        hr;

        hr = host.BuildAppleIIe (core);
        Assert::IsTrue (SUCCEEDED (hr));

        core.PowerCycle ();

        Assert::IsTrue (core.languageCard->IsBank2 (),
            L"Power-on default must select bank2");
        Assert::IsTrue (core.languageCard->IsWriteRam (),
            L"Power-on default must pre-arm WRITERAM");
        Assert::IsFalse (core.languageCard->IsReadRam (),
            L"Power-on default must read from ROM (READRAM=0)");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Phase 8 / T077a — ALTZP routes ZP/stack to aux RAM.
    //
    //  Acceptance scenario 3 first half (FR-006). Independent main and
    //  aux copies of $0080 coexist; ALTZP toggles which side reads see.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase8_AltZp_RoutesZpStackToAux)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        Byte           mainValue;
        Byte           auxValue;

        BootAndRebind (host, core);

        MemoryProbeHelpers::WriteMainZp (core, kProbeAddrZp, kPatternMainZp);
        MemoryProbeHelpers::WriteAuxZp  (core, kProbeAddrZp, kPatternAuxZp);

        mainValue = MemoryProbeHelpers::ReadMainZp (core, kProbeAddrZp);
        auxValue  = MemoryProbeHelpers::ReadAuxZp  (core, kProbeAddrZp);

        Assert::AreEqual (kPatternMainZp, mainValue,
            L"$0080 main ZP must hold the value written with ALTZP=0");
        Assert::AreEqual (kPatternAuxZp, auxValue,
            L"$0080 aux ZP must hold the value written with ALTZP=1");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Phase 8 / T077b — ALTZP routes the LC window to the aux LC bank.
    //
    //  Acceptance scenario 3 second half (FR-006, FR-010). Main bank2
    //  RAM and aux bank2 RAM are physically distinct; ALTZP picks which
    //  one is visible at $D000-$DFFF.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase8_AltZp_RoutesLcWindowToAuxBank)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        Byte           mainBank2;
        Byte           auxBank2;

        BootAndRebind (host, core);

        MemoryProbeHelpers::WriteLcMainBank2 (core, kProbeAddrLcBank, kPatternMainBank2);
        MemoryProbeHelpers::WriteLcAuxBank2  (core, kProbeAddrLcBank, kPatternAuxBank2);

        mainBank2 = MemoryProbeHelpers::ReadLcMainBank2 (core, kProbeAddrLcBank);
        auxBank2  = MemoryProbeHelpers::ReadLcAuxBank2  (core, kProbeAddrLcBank);

        Assert::AreEqual (kPatternMainBank2, mainBank2,
            L"LC main bank2 must hold the ALTZP=0 store");
        Assert::AreEqual (kPatternAuxBank2, auxBank2,
            L"LC aux bank2 must hold the ALTZP=1 store");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Phase 8 / T078 — 80STORE + HIRES + PAGE2 routes hires writes to
    //  aux regardless of RAMWRT.
    //
    //  Acceptance scenario 4 (FR-007, audit §1.1). With 80STORE on,
    //  $2000-$3FFF is carved out of the RAMRD/RAMWRT routing and PAGE2
    //  picks main/aux; the carve-out must apply only when both 80STORE
    //  AND HIRES are on (per AppleIIeMmu::ResolveHires20_3F).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase8_Store80_PlusHiresPlusPage2_RoutesHiresWritesToAux)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        Byte           auxValue;
        Byte           mainValue;

        BootAndRebind (host, core);

        // RAMWRT=0 — proves the carve-out routes to aux on its own
        // merits (not the RAMWRT shortcut).
        core.mmu->SetRamWrt (false);

        // HIRES on, PAGE2 on, then 80STORE on so the MMU re-resolves
        // $2000-$3FFF off the new (HIRES, PAGE2) state.
        core.bus->ReadByte (kSwitchHiresOn);
        core.bus->ReadByte (kSwitchPage2On);
        core.bus->WriteByte (kSwitch80StoreOn, 0);

        Assert::IsTrue (core.mmu->Get80Store (),
            L"$C001 write must engage 80STORE on the MMU");

        core.bus->WriteByte (kProbeAddrHires, kPatternHires);

        // Disengage 80STORE so we can probe aux/main independently
        // via the standard RAMRD-driven helpers.
        core.bus->WriteByte (kSwitch80StoreOff, 0);
        core.bus->ReadByte  (kSwitchPage2Off);
        core.bus->ReadByte  (kSwitchHiresOff);

        auxValue  = MemoryProbeHelpers::ReadAux  (core, kProbeAddrHires);
        mainValue = MemoryProbeHelpers::ReadMain (core, kProbeAddrHires);

        Assert::AreEqual (kPatternHires, auxValue,
            L"Hires write under 80STORE+HIRES+PAGE2 must land in aux");
        Assert::AreNotEqual (kPatternHires, mainValue,
            L"Main hires page must NOT have received the write");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Phase 8 / T079 — Soft reset preserves aux + LC RAM and posts the
    //  CPU to its post-reset state.
    //
    //  Acceptance scenario 5 (FR-012, FR-034, audit C7). Aux RAM, all
    //  six LC banks (main bank1/bank2/high + aux bank1/bank2/high)
    //  must survive; CPU comes back with I=1, SP=0xFD, PC=(FFFC).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase8_SoftReset_PreservesAuxAndLcRam_AndPostsCpuPostResetState)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        Word           expectedPc;
        Byte           p;

        BootAndRebind (host, core);

        // Stamp known patterns into every preserved buffer.
        MemoryProbeHelpers::WriteAux         (core, kProbeAddrMain,    kPatternAux);
        MemoryProbeHelpers::WriteAuxZp       (core, kProbeAddrZp,      kPatternAuxZp);
        MemoryProbeHelpers::WriteLcMainBank1 (core, kProbeAddrLcBank,  kPatternMainBank1);
        MemoryProbeHelpers::WriteLcMainBank2 (core, kProbeAddrLcBank,  kPatternMainBank2);
        MemoryProbeHelpers::WriteLcAuxBank1  (core, kProbeAddrLcBank,  kPatternAuxBank1);
        MemoryProbeHelpers::WriteLcAuxBank2  (core, kProbeAddrLcBank,  kPatternAuxBank2);

        core.mmu->SetAltZp (false);
        core.languageCard->WriteRam (kProbeAddrLcHigh, kPatternHighMain);
        core.mmu->SetAltZp (true);
        core.languageCard->WriteRam (kProbeAddrLcHigh, kPatternHighAux);

        expectedPc = core.cpu->PeekWord (kResetVector);

        core.SoftReset ();

        // MMU paging flags must reset to the documented post-reset
        // posture (audit §10).
        Assert::IsFalse (core.mmu->GetRamRd    (), L"RAMRD must clear on soft reset");
        Assert::IsFalse (core.mmu->GetRamWrt   (), L"RAMWRT must clear on soft reset");
        Assert::IsFalse (core.mmu->GetAltZp    (), L"ALTZP must clear on soft reset");
        Assert::IsFalse (core.mmu->Get80Store  (), L"80STORE must clear on soft reset");
        Assert::IsFalse (core.mmu->GetIntCxRom (), L"INTCXROM must clear on soft reset");

        // CPU post-reset register state per FR-034.
        Assert::AreEqual (kPostResetSp, core.cpu->GetSP (),
            L"SP must equal 0xFD after soft reset");
        Assert::AreEqual (expectedPc, core.cpu->GetPC (),
            L"PC must reload from $FFFC after soft reset");

        // Aux + LC RAM contents survive (audit C7 fix).
        p = MemoryProbeHelpers::ReadAux (core, kProbeAddrMain);
        Assert::AreEqual (kPatternAux, p,
            L"Aux $4000 must survive soft reset");

        p = MemoryProbeHelpers::ReadAuxZp (core, kProbeAddrZp);
        Assert::AreEqual (kPatternAuxZp, p,
            L"Aux ZP $0080 must survive soft reset");

        p = MemoryProbeHelpers::ReadLcMainBank1 (core, kProbeAddrLcBank);
        Assert::AreEqual (kPatternMainBank1, p, L"LC main bank1 must survive");
        p = MemoryProbeHelpers::ReadLcMainBank2 (core, kProbeAddrLcBank);
        Assert::AreEqual (kPatternMainBank2, p, L"LC main bank2 must survive");
        p = MemoryProbeHelpers::ReadLcAuxBank1  (core, kProbeAddrLcBank);
        Assert::AreEqual (kPatternAuxBank1, p, L"LC aux bank1 must survive");
        p = MemoryProbeHelpers::ReadLcAuxBank2  (core, kProbeAddrLcBank);
        Assert::AreEqual (kPatternAuxBank2, p, L"LC aux bank2 must survive");

        core.mmu->SetAltZp (false);
        Assert::AreEqual (kPatternHighMain, core.languageCard->ReadRam (kProbeAddrLcHigh),
            L"LC main high $E100 must survive");
        core.mmu->SetAltZp (true);
        Assert::AreEqual (kPatternHighAux, core.languageCard->ReadRam (kProbeAddrLcHigh),
            L"LC aux high $E100 must survive");
    }
};

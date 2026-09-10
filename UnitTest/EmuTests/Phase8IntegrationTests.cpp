#include "Pch.h"
#include "TestMachine.h"
#include "MemoryProbeHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Phase8IntegrationTests
//
//  User Story 3 (P1). Drives deterministic //e-specific scenarios across
//  the headless //e built by MachineBuilder — proves the
//  MMU + Language Card rewrites from Phases 2-3 land the right bytes in
//  the right buffers end-to-end.
//
//  Constitution §II: every test uses TestMachine + IFixtureProvider
//  only; no host filesystem, no Win32, no audio device.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Phase8IntegrationTests)
{
public:

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

    ////////////////////////////////////////////////////////////////////////
    //
    //  Helper: Boot a //e to the cold-boot prompt and rebind the bus
    //  page table off the CPU's cold-boot scratch buffer onto the
    //  MMU's mainRam/auxRam buffers.
    //
    ////////////////////////////////////////////////////////////////////////

    static void BootAndRebind (MachineHost & machine)
    {
        Assert::IsTrue ((machine.GetCpu() != nullptr && machine.GetMmu() != nullptr), L"//e wiring must be complete");

        machine.PowerCycle();
        machine.RunCycles  (kColdBootCycles);

        MemoryProbeHelpers::RebindMainBaseline (machine);
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
        TestMachine   machine ("Apple2e", TestMachine::Slots::Empty);
        Byte           mainValue;
        Byte           auxValue;

        BootAndRebind (machine);

        MemoryProbeHelpers::WriteMain (machine, kProbeAddrMain, kPatternMain);
        MemoryProbeHelpers::WriteAux  (machine, kProbeAddrMain, kPatternAux);

        mainValue = MemoryProbeHelpers::ReadMain (machine, kProbeAddrMain);
        auxValue  = MemoryProbeHelpers::ReadAux  (machine, kProbeAddrMain);

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
        TestMachine   machine ("Apple2e", TestMachine::Slots::Empty);

        BootAndRebind (machine);

        machine.GetMemoryBus().ReadByte (kLcReadEvenBank2);
        Assert::IsFalse (machine.GetRefs().languageCard->IsWriteRam(),
            L"Even-address read must clear WRITERAM");

        machine.GetMemoryBus().ReadByte (kLcOddBank2A);
        Assert::IsFalse (machine.GetRefs().languageCard->IsWriteRam(),
            L"One odd read must NOT yet enable WRITERAM");

        machine.GetMemoryBus().ReadByte (kLcOddBank2B);
        Assert::IsTrue (machine.GetRefs().languageCard->IsWriteRam(),
            L"Second odd read at a different $C08x must enable WRITERAM");
        Assert::IsTrue (machine.GetRefs().languageCard->IsBank2(),
            L"$C08x reads in the $C080-$C087 range must select bank2");

        machine.GetRefs().languageCard->WriteRam (kProbeAddrLcBank, kPatternMainBank2);

        Assert::AreEqual (kPatternMainBank2,
            machine.GetRefs().languageCard->ReadRam (kProbeAddrLcBank),
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
        TestMachine   machine ("Apple2e", TestMachine::Slots::Empty);

        BootAndRebind (machine);

        machine.GetMemoryBus().ReadByte  (kLcReadEvenBank2);
        machine.GetMemoryBus().ReadByte  (kLcOddBank2A);
        machine.GetMemoryBus().WriteByte (kLcOddBank2A, 0);

        Assert::AreEqual (0, machine.GetRefs().languageCard->GetPreWriteCount(),
            L"Write to an odd $C08x must clear the pre-write counter");

        machine.GetMemoryBus().ReadByte (kLcOddBank2B);

        Assert::IsFalse (machine.GetRefs().languageCard->IsWriteRam(),
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
        TestMachine   machine ("Apple2e", TestMachine::Slots::Empty);

        machine.PowerCycle();

        Assert::IsTrue (machine.GetRefs().languageCard->IsBank2(),
            L"Power-on default must select bank2");
        Assert::IsTrue (machine.GetRefs().languageCard->IsWriteRam(),
            L"Power-on default must pre-arm WRITERAM");
        Assert::IsFalse (machine.GetRefs().languageCard->IsReadRam(),
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
        TestMachine   machine ("Apple2e", TestMachine::Slots::Empty);
        Byte           mainValue;
        Byte           auxValue;

        BootAndRebind (machine);

        MemoryProbeHelpers::WriteMainZp (machine, kProbeAddrZp, kPatternMainZp);
        MemoryProbeHelpers::WriteAuxZp  (machine, kProbeAddrZp, kPatternAuxZp);

        mainValue = MemoryProbeHelpers::ReadMainZp (machine, kProbeAddrZp);
        auxValue  = MemoryProbeHelpers::ReadAuxZp  (machine, kProbeAddrZp);

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
        TestMachine   machine ("Apple2e", TestMachine::Slots::Empty);
        Byte           mainBank2;
        Byte           auxBank2;

        BootAndRebind (machine);

        MemoryProbeHelpers::WriteLcMainBank2 (machine, kProbeAddrLcBank, kPatternMainBank2);
        MemoryProbeHelpers::WriteLcAuxBank2  (machine, kProbeAddrLcBank, kPatternAuxBank2);

        mainBank2 = MemoryProbeHelpers::ReadLcMainBank2 (machine, kProbeAddrLcBank);
        auxBank2  = MemoryProbeHelpers::ReadLcAuxBank2  (machine, kProbeAddrLcBank);

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
    //  AND HIRES are on (per Apple2eMmu::ResolveHires20_3F).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase8_Store80_PlusHiresPlusPage2_RoutesHiresWritesToAux)
    {
        TestMachine   machine ("Apple2e", TestMachine::Slots::Empty);
        Byte           auxValue;
        Byte           mainValue;

        BootAndRebind (machine);

        // RAMWRT=0 — proves the carve-out routes to aux on its own
        // merits (not the RAMWRT shortcut).
        machine.GetMmu()->SetRamWrt (false);

        // HIRES on, PAGE2 on, then 80STORE on so the MMU re-resolves
        // $2000-$3FFF off the new (HIRES, PAGE2) state.
        machine.GetMemoryBus().ReadByte (kSwitchHiresOn);
        machine.GetMemoryBus().ReadByte (kSwitchPage2On);
        machine.GetMemoryBus().WriteByte (kSwitch80StoreOn, 0);

        Assert::IsTrue (machine.GetMmu()->Get80Store(),
            L"$C001 write must engage 80STORE on the MMU");

        machine.GetMemoryBus().WriteByte (kProbeAddrHires, kPatternHires);

        // Disengage 80STORE so we can probe aux/main independently
        // via the standard RAMRD-driven helpers.
        machine.GetMemoryBus().WriteByte (kSwitch80StoreOff, 0);
        machine.GetMemoryBus().ReadByte  (kSwitchPage2Off);
        machine.GetMemoryBus().ReadByte  (kSwitchHiresOff);

        auxValue  = MemoryProbeHelpers::ReadAux  (machine, kProbeAddrHires);
        mainValue = MemoryProbeHelpers::ReadMain (machine, kProbeAddrHires);

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
        TestMachine   machine ("Apple2e", TestMachine::Slots::Empty);
        Word           expectedPc;
        Byte           p;

        BootAndRebind (machine);

        // Stamp known patterns into every preserved buffer.
        MemoryProbeHelpers::WriteAux         (machine, kProbeAddrMain,    kPatternAux);
        MemoryProbeHelpers::WriteAuxZp       (machine, kProbeAddrZp,      kPatternAuxZp);
        MemoryProbeHelpers::WriteLcMainBank1 (machine, kProbeAddrLcBank,  kPatternMainBank1);
        MemoryProbeHelpers::WriteLcMainBank2 (machine, kProbeAddrLcBank,  kPatternMainBank2);
        MemoryProbeHelpers::WriteLcAuxBank1  (machine, kProbeAddrLcBank,  kPatternAuxBank1);
        MemoryProbeHelpers::WriteLcAuxBank2  (machine, kProbeAddrLcBank,  kPatternAuxBank2);

        machine.GetMmu()->SetAltZp (false);
        machine.GetRefs().languageCard->WriteRam (kProbeAddrLcHigh, kPatternHighMain);
        machine.GetMmu()->SetAltZp (true);
        machine.GetRefs().languageCard->WriteRam (kProbeAddrLcHigh, kPatternHighAux);

        expectedPc = machine.GetCpu()->PeekWord (kResetVector);

        machine.SoftReset();

        // MMU paging flags must reset to the documented post-reset
        // posture (audit §10).
        Assert::IsFalse (machine.GetMmu()->GetRamRd    (), L"RAMRD must clear on soft reset");
        Assert::IsFalse (machine.GetMmu()->GetRamWrt   (), L"RAMWRT must clear on soft reset");
        Assert::IsFalse (machine.GetMmu()->GetAltZp    (), L"ALTZP must clear on soft reset");
        Assert::IsFalse (machine.GetMmu()->Get80Store  (), L"80STORE must clear on soft reset");
        Assert::IsFalse (machine.GetMmu()->GetIntCxRom(), L"INTCXROM must clear on soft reset");

        // CPU post-reset register state per FR-034.
        Assert::AreEqual (kPostResetSp, machine.GetCpu()->GetSP(),
            L"SP must equal 0xFD after soft reset");
        Assert::AreEqual (expectedPc, machine.GetCpu()->GetPC(),
            L"PC must reload from $FFFC after soft reset");

        // Aux + LC RAM contents survive (audit C7 fix).
        p = MemoryProbeHelpers::ReadAux (machine, kProbeAddrMain);
        Assert::AreEqual (kPatternAux, p,
            L"Aux $4000 must survive soft reset");

        p = MemoryProbeHelpers::ReadAuxZp (machine, kProbeAddrZp);
        Assert::AreEqual (kPatternAuxZp, p,
            L"Aux ZP $0080 must survive soft reset");

        p = MemoryProbeHelpers::ReadLcMainBank1 (machine, kProbeAddrLcBank);
        Assert::AreEqual (kPatternMainBank1, p, L"LC main bank1 must survive");
        p = MemoryProbeHelpers::ReadLcMainBank2 (machine, kProbeAddrLcBank);
        Assert::AreEqual (kPatternMainBank2, p, L"LC main bank2 must survive");
        p = MemoryProbeHelpers::ReadLcAuxBank1  (machine, kProbeAddrLcBank);
        Assert::AreEqual (kPatternAuxBank1, p, L"LC aux bank1 must survive");
        p = MemoryProbeHelpers::ReadLcAuxBank2  (machine, kProbeAddrLcBank);
        Assert::AreEqual (kPatternAuxBank2, p, L"LC aux bank2 must survive");

        machine.GetMmu()->SetAltZp (false);
        Assert::AreEqual (kPatternHighMain, machine.GetRefs().languageCard->ReadRam (kProbeAddrLcHigh),
            L"LC main high $E100 must survive");
        machine.GetMmu()->SetAltZp (true);
        Assert::AreEqual (kPatternHighAux, machine.GetRefs().languageCard->ReadRam (kProbeAddrLcHigh),
            L"LC aux high $E100 must survive");
    }
};


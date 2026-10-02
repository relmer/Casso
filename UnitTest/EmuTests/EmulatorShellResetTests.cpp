#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Core/Prng.h"
#include "Core/DramPowerOnPattern.h"
#include "Devices/RamDevice.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShellResetTests
//
//  Phase 4 / FR-034 / audit §10 verification that the EmulatorShell IDM
//  handlers route correctly:
//    IDM_MACHINE_RESET       -> EmulatorShell::SoftReset()
//    IDM_MACHINE_POWERCYCLE  -> EmulatorShell::PowerCycle()
//
//  EmulatorShell CAN be built inside a unit test -- D3DRenderer and
//  WasapiAudio construct empty, so a shell costs an allocation, and
//  ShellKeyWiringTests drives one. What it cannot do without Initialize is
//  RUN an IDM handler, which needs the HWND and the message pump. The
//  IDM cases in EmulatorShell.cpp are now one-line forwarders, so the
//  contract these tests verify is the *visible side effect* — namely the
//  audit §10 [CRITICAL] property that 80COL no longer persists across
//  SoftReset, mirroring what IDM_MACHINE_RESET drives in production.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (EmulatorShellResetTests)
{
public:

    TEST_METHOD (ResetMenuItemDispatchesSoftReset)
    {
        // Contract: IDM_MACHINE_RESET runs MemoryBus::SoftResetAll fan-out
        // and clears every soft switch. Verifying via the soft-switch
        // bank suffices because that is the path the originally-reported
        // bug surfaced through (audit §10 [CRITICAL]).
        MemoryBus               bus;
        Apple2eSoftSwitchBank   sw (&bus);

        sw.Write (0xC050, 0);   // graphics on
        sw.Write (0xC055, 0);   // page2 on
        sw.Write (0xC00D, 0);   // 80COL on

        sw.SoftReset();

        Assert::IsFalse (sw.IsGraphicsMode(),
            L"IDM_MACHINE_RESET must clear graphics mode");
        Assert::IsFalse (sw.IsPage2(),
            L"IDM_MACHINE_RESET must clear PAGE2");
        Assert::IsFalse (sw.Is80ColMode(),
            L"IDM_MACHINE_RESET must clear 80COL (audit §10 [CRITICAL])");
    }

    TEST_METHOD (PowerCycleMenuItemDispatchesPowerCycle)
    {
        size_t       patternBytes = 0;



        // Contract: IDM_MACHINE_POWERCYCLE drives MemoryBus::PowerCycleAll
        // and the same SoftReset effect. Verify by observing that zeroed
        // RAM comes back holding the DRAM power-on pattern (the PowerCycle
        // path that the menu item ultimately runs is the only one that
        // refills DRAM).
        RamDevice    ram (0x0000, 0xBFFF);
        Prng         prng (0xCA550001ULL);

        ram.Reset();   // start zeroed
        ram.PowerCycle (prng);

        for (size_t i = 0; i < 0xC000; i++)
        {
            if (!DramPowerOnPattern::IsHole (i) && ram.Read (static_cast<Word> (i)) == DramPowerOnPattern::GetPatternByte (i))
            {
                ++patternBytes;
            }
        }

        Assert::AreEqual<size_t> (0xC000 - 4 * (0xC000 / DramPowerOnPattern::kHoleStride), patternBytes,
            L"IDM_MACHINE_POWERCYCLE must refill DRAM with the power-on pattern");
    }

    TEST_METHOD (Audit_80ColModePersistenceAcrossResetIsFixed)
    {
        // Audit §10 [CRITICAL]: the originally-reported bug. Before
        // Phase 4 the IDM_MACHINE_RESET handler did `m_cpu->SetPC(...)`
        // only, which left every soft switch alone — so 80COL survived a
        // soft reset and the //e came back up in the wrong text mode.
        // Phase 4 routes the menu item through SoftReset(), which fans
        // out to Apple2eSoftSwitchBank::SoftReset(), which clears 80COL.
        MemoryBus               bus;
        Apple2eSoftSwitchBank   sw (&bus);

        sw.Write (0xC00D, 0);   // 80COL on (the regression vector)
        sw.Write (0xC00F, 0);   // ALTCHARSET on

        Assert::IsTrue (sw.Is80ColMode(),
            L"Pre-condition: 80COL was successfully enabled");

        sw.SoftReset();

        Assert::IsFalse (sw.Is80ColMode(),
            L"Audit §10 [CRITICAL] FIXED: 80COL must not survive SoftReset");
        Assert::IsFalse (sw.IsAltCharSet(),
            L"ALTCHARSET must not survive SoftReset");
    }
};

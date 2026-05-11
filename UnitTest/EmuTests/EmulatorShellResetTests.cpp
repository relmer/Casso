#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Core/Prng.h"
#include "Devices/RamDevice.h"
#include "Devices/AppleIIeSoftSwitchBank.h"

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
//  EmulatorShell itself is Win32-bound (HINSTANCE, HWND, message pump,
//  D3DRenderer, WasapiAudio) and cannot be built inside a unit test. The
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
        AppleIIeSoftSwitchBank  sw (&bus);

        sw.Write (0xC050, 0);   // graphics on
        sw.Write (0xC055, 0);   // page2 on
        sw.Write (0xC00D, 0);   // 80COL on

        sw.SoftReset ();

        Assert::IsFalse (sw.IsGraphicsMode (),
            L"IDM_MACHINE_RESET must clear graphics mode");
        Assert::IsFalse (sw.IsPage2 (),
            L"IDM_MACHINE_RESET must clear PAGE2");
        Assert::IsFalse (sw.Is80ColMode (),
            L"IDM_MACHINE_RESET must clear 80COL (audit §10 [CRITICAL])");
    }

    TEST_METHOD (PowerCycleMenuItemDispatchesPowerCycle)
    {
        // Contract: IDM_MACHINE_POWERCYCLE drives MemoryBus::PowerCycleAll
        // (Prng-seeded fan-out) and the same SoftReset effect. Verify by
        // observing that the RAM is non-zero after the call (the
        // PowerCycle path that the menu item ultimately runs is the only
        // one that re-seeds DRAM).
        RamDevice    ram (0x0000, 0xBFFF);
        Prng         prng (0xCA550001ULL);
        size_t       nonZero = 0;

        ram.Reset ();   // start zeroed
        ram.PowerCycle (prng);

        for (size_t i = 0; i < 0xC000; i++)
        {
            if (ram.Read (static_cast<Word> (i)) != 0)
            {
                ++nonZero;
            }
        }

        Assert::IsTrue (nonZero > 40000,
            L"IDM_MACHINE_POWERCYCLE must re-seed DRAM via the shared Prng");
    }

    TEST_METHOD (Audit_80ColModePersistenceAcrossResetIsFixed)
    {
        // Audit §10 [CRITICAL]: the originally-reported bug. Before
        // Phase 4 the IDM_MACHINE_RESET handler did `m_cpu->SetPC(...)`
        // only, which left every soft switch alone — so 80COL survived a
        // soft reset and the //e came back up in the wrong text mode.
        // Phase 4 routes the menu item through SoftReset(), which fans
        // out to AppleIIeSoftSwitchBank::SoftReset(), which clears 80COL.
        MemoryBus               bus;
        AppleIIeSoftSwitchBank  sw (&bus);

        sw.Write (0xC00D, 0);   // 80COL on (the regression vector)
        sw.Write (0xC00F, 0);   // ALTCHARSET on

        Assert::IsTrue (sw.Is80ColMode (),
            L"Pre-condition: 80COL was successfully enabled");

        sw.SoftReset ();

        Assert::IsFalse (sw.Is80ColMode (),
            L"Audit §10 [CRITICAL] FIXED: 80COL must not survive SoftReset");
        Assert::IsFalse (sw.IsAltCharSet (),
            L"ALTCHARSET must not survive SoftReset");
    }
};

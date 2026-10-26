#include "Pch.h"
#include "TestMachine.h"
#include "FixtureProvider.h"
#include "KeystrokeInjector.h"
#include "MachineIdle.h"
#include "TextScreenScraper.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Core/InterruptController.h"
#include "Devices/RamDevice.h"
#include "Machines/Apple2/Common/VideoTiming.h"
#include "HResultAssert.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleMouseTests
//
//  Two tiers. The device tier drives the IOU mouse hardware directly:
//  movement-interrupt latching across acknowledge (the "neither starve nor
//  double-fire" edge case), direction-line polarity, VBL latching, button
//  polarity, and the IOU access gate. The firmware tier is the oracle: it
//  boots the real //c ROM 4 and calls the mouse firmware's own protocol
//  entry points (phantom slot 7 on ROM 4 — $C712-$C719 table) against the
//  hardware model, proving the register contract end to end. The ROM comes
//  from scripts/FetchRoms.ps1 -Fixtures, and its absence fails the build
//  step by name rather than skipping anything.
//
//  This file has the BASIC hook diagnostic, which mounts a disk image from
//  %LOCALAPPDATA%\Casso under its real path. The rest are in the unit suite's
//  AppleMouseTests.
//
////////////////////////////////////////////////////////////////////////////////

// Shared by both TEST_CLASSes below, so these live at file scope rather than
// on either one. `static` supplies the internal linkage the anonymous
// namespace was there for.





////////////////////////////////////////////////////////////////////////////////
//
//  DiagWritableDiskPath
//
//  A writable, bootable DOS 3.3 disk in the emulator's own cache, which the
//  three Diag_ cases below boot and (two of them) write to.
//
//  RESOLVED FROM THE ENVIRONMENT, not hard-coded. All three carried an
//  absolute path containing one developer's user name until 2026-09-02, so
//  they could not run for anybody else however the disk was provisioned --
//  the skip was structural rather than a property of the machine. Same cache
//  the emulator writes into and GuestSession reads from.
//
//  Not provisioned by anything: it is a disk a developer saved from a running
//  emulator, so these stay skip-when-absent rather than becoming a gate on a
//  file no script produces.
//
////////////////////////////////////////////////////////////////////////////////

static std::string DiagWritableDiskPath()
{
    wchar_t *  cacheRoot = nullptr;
    size_t     len       = 0;
    bool       hasCache  = _wdupenv_s (&cacheRoot, &len, L"LOCALAPPDATA") == 0
                           && cacheRoot != nullptr;
    std::string  path;



    if (hasCache)
    {
        path = (std::filesystem::path (cacheRoot) / L"Casso" / L"Disks" /
                L"DOS 3.3 Writable.woz").string();
        free (cacheRoot);
    }

    return path;
}

// IOU switch addresses ($C058-$C05F while access is enabled).
static constexpr Word  s_kDisXy  = 0xC058;
static constexpr Word  s_kEnbXy  = 0xC059;
static constexpr Word  s_kDisVbl = 0xC05A;
static constexpr Word  s_kEnbVbl = 0xC05B;





////////////////////////////////////////////////////////////////////////////////
//
//  EnableXyInterrupts
//
//  Enable movement interrupts through the front door: IOU access on,
//  ENBXY, IOU access off (the same bracket the firmware uses).
//
////////////////////////////////////////////////////////////////////////////////

static void EnableXyInterrupts (AppleMouse & mouse)
{
    mouse.WriteIouAccess (true);
    mouse.AccessIouSwitch (s_kEnbXy);
    mouse.WriteIouAccess (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleMouseFirmwareTests
//
//  The mouse as guest software SEES it: the $C4xx firmware entry points and
//  the soft switches behind them.
//
//  A separate suite from the device tests because it exercises the other side
//  of the contract -- the device can be correct while the firmware interface
//  it is reached through is wired wrong, and only these would notice.
//
//  The PTRIG acknowledgement is covered specifically. The //c's VBL interrupt
//  latch is sticky and is cleared by a partially-decoded PTRIG access across
//  $C070-$C07F, which is how MousePaint acks it -- getting that decode wrong
//  leaves the latch set and produces dead clicks and a laggy cursor.
//
//  Entry points are called the way firmware calls them, so the tests fail if
//  the ROM's expectations and the device's behavior drift apart.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleMouseFirmwareTests)
{
public:

    //  A TOOL, NOT A TEST. It asserts nothing about what it observes: it boots
    //  a //c from a writable DOS 3.3 disk, types the BASIC mouse program
    //  (PR#7 plus CHR$(1) to turn the mouse on, IN#7 to redirect input so
    //  INPUT reads X, Y and the button from the firmware), injects host
    //  motion and a click, then DUMPS the screen and a status line for a
    //  human to read. Whatever it finds, it passes.
    //
    //  So do not read a green run here as evidence of anything. It is the
    //  instrument to reach for when a mouse problem is being investigated and
    //  the question is what the firmware hook actually does, which the
    //  protocol-entry oracle tests above cannot show because they check the
    //  device rather than the BASIC path through it.
    //
    //  RUN IT DELIBERATELY, ON DEMAND:
    //
    //      scripts\RunTests.ps1 -Scenario -Filter Diag_BasicInSevenHookTracksMotion
    //
    //  after removing its TEST_IGNORE attribute locally, since an ignored test
    //  is skipped even when picked by name, and put a bootable, writable DOS
    //  3.3 disk at %LOCALAPPDATA%\Casso\Disks\DOS 3.3 Writable.woz first --
    //  save one from a running emulator, or build one with
    //  `CassoCli disk create "<that path>" --format dos33 --bootable <master>`.
    //  The attribute makes an ordinary suite run report it as skipped rather
    //  than passed, and a run without that disk fails.
    BEGIN_TEST_METHOD_ATTRIBUTE (Diag_BasicInSevenHookTracksMotion)
        TEST_IGNORE()
    END_TEST_METHOD_ATTRIBUTE()

    TEST_METHOD (Diag_BasicInSevenHookTracksMotion)
    {
        const std::string  kDiskPath = DiagWritableDiskPath();
        std::ifstream f (kDiskPath, std::ios::binary);

        if (!f.good())
        {
            Assert::Fail (L"the writable DOS 3.3 disk is absent");
        }
        else
        {
            TestMachine  machine ("Apple2c");
            char          st[128];

            std::vector<uint8_t>  bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char> ());

            machine.PowerCycle();
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, kDiskPath, DiskFormat::Woz, bytes));

            MachineIdle::RunUntilIdle (machine, 60'000'000);                       // boot DOS 3.3 to ]
            auto dump = [&] (const char * tag)
            {
                Logger::WriteMessage (tag);
                for (const std::string & row : TextScreenScraper::Scrape (machine))
                {
                    Logger::WriteMessage (row.c_str());
                }
            };

            KeystrokeInjector::InjectLine (machine, "10 D$=CHR$(4)");
            KeystrokeInjector::InjectLine (machine, "20 PRINT D$;\"PR#7\":PRINT CHR$(1):PRINT D$;\"PR#0\"");
            KeystrokeInjector::InjectLine (machine, "30 PRINT D$;\"IN#7\"");
            KeystrokeInjector::InjectLine (machine, "40 INPUT \"\";X,Y,B");
            KeystrokeInjector::InjectLine (machine, "50 PRINT X;\" \";Y;\" \";B");
            KeystrokeInjector::InjectLine (machine, "60 GOTO 40");
            KeystrokeInjector::InjectLine (machine, "RUN", 2'000'000);

            machine.GetMouse()->MoveBy (+7, +4);                       // host motion
            machine.RunCycles (4'000'000);
            machine.GetMouse()->SetButton (true);
            machine.RunCycles (2'000'000);
            dump ("---- screen after RUN + motion ----");
            sprintf_s (st, "xyEn=%d mode07FF=%02X PC=%04X",
                       machine.GetMouse()->AreXyInterruptsEnabled() ? 1 : 0,
                       machine.GetCpu()->ReadByte (0x07FF), machine.GetCpu()->GetPC());
            Logger::WriteMessage (st);
        }
    }
};

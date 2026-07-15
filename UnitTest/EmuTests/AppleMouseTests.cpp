#include "Pch.h"
#include "HeadlessHost.h"
#include "FixtureProvider.h"
#include "KeystrokeInjector.h"
#include "TextScreenScraper.h"
#include "Devices/AppleMouse.h"
#include "Core/InterruptController.h"
#include "Video/VideoTiming.h"

#include <fstream>

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
//  hardware model, proving the register contract end to end. Firmware
//  tests skip when the copyrighted ROM fixture is absent.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr size_t   kRomSize = 0x8000;

    bool Apple2cRomAvailable()
    {
        FixtureProvider        fp;
        std::vector<uint8_t>   bytes;
        return SUCCEEDED (fp.OpenFixture ("Apple2c.rom", bytes)) && bytes.size () == kRomSize;
    }

    // IOU switch addresses ($C058-$C05F while access is enabled).
    constexpr Word  kDisXy  = 0xC058;
    constexpr Word  kEnbXy  = 0xC059;
    constexpr Word  kDisVbl = 0xC05A;
    constexpr Word  kEnbVbl = 0xC05B;

    // Enable movement interrupts through the front door: IOU access on,
    // ENBXY, IOU access off (the same bracket the firmware uses).
    void EnableXyInterrupts (AppleMouse & mouse)
    {
        mouse.WriteIouAccess (true);
        mouse.AccessIouSwitch (kEnbXy);
        mouse.WriteIouAccess (false);
    }
}


TEST_CLASS (AppleMouseDeviceTests)
{
public:

    // One interrupt per movement unit: the line asserts when a unit latches,
    // HOLDS until the $C048 acknowledge (no starvation), and never re-fires
    // for the same unit (no double-fire). The next unit loads only after the
    // ack, re-asserting the line exactly once per unit.
    TEST_METHOD (MovementLatch_HoldsUntilAck_NeverDoubleFires)
    {
        InterruptController  ic;         // no CPU: observe via IsAnyAsserted
        AppleMouse           mouse;

        Assert::IsTrue (SUCCEEDED (mouse.AttachInterruptController (&ic)));
        EnableXyInterrupts (mouse);

        mouse.MoveBy (2, 0);
        mouse.Tick (1);
        Assert::IsTrue  (ic.IsAnyAsserted (), L"first unit must assert the line");
        Assert::AreEqual<Byte> (0x80, mouse.ReadXInterruptStatus (), L"$C015 bit 7 pending");

        // Level-held, not pulsed: many ticks later it is still asserted and
        // still the SAME single unit (no second latch before the ack).
        for (int i = 0; i < 100; ++i) { mouse.Tick (1); }
        Assert::IsTrue (ic.IsAnyAsserted (), L"line must hold until acknowledged");

        mouse.AccessRstXY();
        Assert::IsFalse (ic.IsAnyAsserted (),          L"$C048 ack must drop the line");
        Assert::AreEqual<Byte> (0x00, mouse.ReadXInterruptStatus (), L"ack clears the pending flag");

        mouse.Tick (1);
        Assert::IsTrue (ic.IsAnyAsserted (), L"second queued unit re-asserts after ack");

        mouse.AccessRstXY();
        mouse.Tick (1);
        Assert::IsFalse (ic.IsAnyAsserted (), L"queue drained: no third interrupt");
    }


    // $C066/$C067 direction-line polarity, exactly as the firmware's service
    // loop consumes them: MOUX1 bit 7 = 1 -> X increments; MOUY1 is inverted
    // by the firmware (EOR #$80), so bit 7 = 0 -> Y increments.
    TEST_METHOD (DirectionLines_MatchFirmwarePolarity)
    {
        InterruptController  ic;
        AppleMouse           mouse;

        Assert::IsTrue (SUCCEEDED (mouse.AttachInterruptController (&ic)));
        EnableXyInterrupts (mouse);

        mouse.MoveBy (+1, +1);
        mouse.Tick (1);
        Assert::AreEqual<Byte> (0x80, mouse.ReadMouX1 (), L"+X (right) -> MOUX1 bit 7 set");
        Assert::AreEqual<Byte> (0x00, mouse.ReadMouY1 (), L"+Y (down)  -> MOUY1 bit 7 clear");

        mouse.AccessRstXY();
        mouse.MoveBy (-1, -1);
        mouse.Tick (1);
        Assert::AreEqual<Byte> (0x00, mouse.ReadMouX1 (), L"-X (left) -> MOUX1 bit 7 clear");
        Assert::AreEqual<Byte> (0x80, mouse.ReadMouY1 (), L"-Y (up)   -> MOUY1 bit 7 set");
    }


    // VBL: the latch sets at vblank onset regardless of ENVBL (the enable
    // masks only the IRQ line), reads at $C019 bit 7, and clears on the
    // $C070 access. The line is gated by ENVBL/DISVBL.
    TEST_METHOD (VblLatch_OnsetSetsFlag_EnvblGatesLine_PtrigClears)
    {
        InterruptController  ic;
        AppleMouse           mouse;
        VideoTiming          vt;

        Assert::IsTrue (SUCCEEDED (mouse.AttachInterruptController (&ic)));
        mouse.SetVideoTiming (&vt);

        // Masked VBL: tick into vblank -- flag latches, line stays low.
        vt.Tick (VideoTiming::kVblankStartCycle + 1);
        mouse.Tick (1);
        Assert::AreEqual<Byte> (0x80, mouse.ReadVblInterrupt (), L"latch sets at onset even when masked");
        Assert::IsFalse (ic.IsAnyAsserted (), L"DISVBL (default) masks the line");

        // Enable: pending latch surfaces on the line immediately.
        mouse.WriteIouAccess (true);
        mouse.AccessIouSwitch (kEnbVbl);
        mouse.WriteIouAccess (false);
        Assert::IsTrue (ic.IsAnyAsserted (), L"ENVBL with a pending latch asserts");

        // $C070 acknowledge.
        mouse.AccessPtrig();
        Assert::AreEqual<Byte> (0x00, mouse.ReadVblInterrupt (), L"$C070 clears the latch");
        Assert::IsFalse (ic.IsAnyAsserted (), L"ack drops the line");

        // Next frame's onset latches again (edge, not level): tick through
        // the display period (so the mouse observes not-vblank) and into the
        // following vblank.
        vt.Tick (VideoTiming::kCyclesPerFrame - 2000);   // wraps into display
        mouse.Tick (1);                                   // observe display
        Assert::AreEqual<Byte> (0x00, mouse.ReadVblInterrupt (), L"still clear during display");
        vt.Tick (4000);                                   // next vblank onset
        mouse.Tick (1);
        Assert::AreEqual<Byte> (0x80, mouse.ReadVblInterrupt (), L"next onset re-latches");
        Assert::IsTrue (ic.IsAnyAsserted (), L"enabled + latched -> asserted");
    }


    // $C063: ACTIVE LOW. Idle high (0x80); a press pulls the line to 0.
    TEST_METHOD (Button_ActiveLow)
    {
        AppleMouse  mouse;

        Assert::AreEqual<Byte> (0x80, mouse.ReadButton (), L"released idles high");
        mouse.SetButton (true);
        Assert::AreEqual<Byte> (0x00, mouse.ReadButton (), L"pressed pulls low");
        mouse.SetButton (false);
        Assert::AreEqual<Byte> (0x80, mouse.ReadButton (), L"release restores high");
    }


    // DISXY masks the line without discarding the pending flags; re-enabling
    // surfaces them again (mask, not clear).
    TEST_METHOD (DisXy_MasksLineWithoutClearingFlags)
    {
        InterruptController  ic;
        AppleMouse           mouse;

        Assert::IsTrue (SUCCEEDED (mouse.AttachInterruptController (&ic)));
        EnableXyInterrupts (mouse);

        mouse.MoveBy (1, 0);
        mouse.Tick (1);
        Assert::IsTrue (ic.IsAnyAsserted());

        mouse.WriteIouAccess (true);
        mouse.AccessIouSwitch (kDisXy);
        Assert::IsFalse (ic.IsAnyAsserted (),           L"DISXY masks the line");
        Assert::AreEqual<Byte> (0x80, mouse.ReadXInterruptStatus (), L"flag survives the mask");

        mouse.AccessIouSwitch (kEnbXy);
        mouse.WriteIouAccess (false);
        Assert::IsTrue (ic.IsAnyAsserted (), L"re-enable surfaces the pending flag");
    }
};


TEST_CLASS (AppleMouseFirmwareTests)
{
public:

    // ROM 4 (Memory Expansion //c) identifies the mouse at phantom slot 7:
    // the Pascal pointing-device signature $C705=$38 $C707=$18 $C70B=$01
    // $C70C=$20 $C7FB=$D6, read through the live bus (no-slots $Cxxx
    // routing). This is how MousePaint-class software finds the mouse.
    TEST_METHOD (FirmwareIdentifiesMouseAtSlot7)
    {
        if (!Apple2cRomAvailable())
        {
            Logger::WriteMessage ("SKIPPED: no Apple2c.rom fixture");
            return;
        }

        HeadlessHost   host;
        EmulatorCore   core;

        Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)));
        core.PowerCycle();

        Assert::AreEqual<Byte> (0x38, core.bus->ReadByte (0xC705), L"$C705 signature");
        Assert::AreEqual<Byte> (0x18, core.bus->ReadByte (0xC707), L"$C707 signature");
        Assert::AreEqual<Byte> (0x01, core.bus->ReadByte (0xC70B), L"$C70B signature");
        Assert::AreEqual<Byte> (0x20, core.bus->ReadByte (0xC70C), L"$C70C device class");
        Assert::AreEqual<Byte> (0xD6, core.bus->ReadByte (0xC7FB), L"$C7FB mouse id");
    }


    // The oracle: the REAL ROM 4 mouse firmware runs against the hardware
    // model. A RAM driver calls the firmware's own protocol entries
    // (INITMOUSE $C740 -> SETMOUSE $C71C mode 1, transparent) and spins with
    // interrupts enabled; injected host motion then flows entirely through
    // the firmware's IRQ service -- movement interrupt, $C066/$C067
    // direction reads, position update, $C048 acknowledge -- one interrupt
    // per unit. READMOUSE ($C728) must report the summed position in the
    // slot-7 screen holes, and the button must read through bit 7 of the
    // status hole. Skips when the ROM fixture is absent.
    TEST_METHOD (FirmwareTracksMotionAndButton_TransparentMode)
    {
        if (!Apple2cRomAvailable())
        {
            Logger::WriteMessage ("SKIPPED: no Apple2c.rom fixture");
            return;
        }

        HeadlessHost   host;
        EmulatorCore   core;

        Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)));
        core.PowerCycle();

        // Let the reset firmware initialize (screen, zero page) and run
        // past the first VBL onset so the VBL latch is set -- the firmware's
        // interrupt-enable path samples $C019 before programming the IOU.
        core.RunCycles (60'000);

        // Driver 1 @ $0300: INITMOUSE, SETMOUSE(mode 1), CLI, spin.
        // Protocol: X = $Cn, Y = $n0 (n = 7), A = argument.
        const Byte kInit[] =
        {
            0xA2, 0xC7,          // LDX #$C7
            0xA0, 0x70,          // LDY #$70
            0x20, 0x40, 0xC7,    // JSR $C740   INITMOUSE
            0xA9, 0x01,          // LDA #$01    mode 1: mouse on, transparent
            0xA2, 0xC7,          // LDX #$C7
            0xA0, 0x70,          // LDY #$70
            0x20, 0x1C, 0xC7,    // JSR $C71C   SETMOUSE
            0x58,                // CLI
            0x4C, 0x11, 0x03,    // JMP $0311   spin, interrupts enabled
        };
        Word  addr = 0x0300;
        for (Byte b : kInit) { core.cpu->WriteByte (addr++, b); }

        core.cpu->SetPC (0x0300);
        core.RunCycles (150'000);

        // Inject host motion; the firmware services it one unit per IRQ.
        core.mouse->MoveBy (+5, +3);
        core.RunCycles (300'000);

        // Driver 2 @ $0320: READMOUSE, spin.
        const Byte kRead[] =
        {
            0xA2, 0xC7,          // LDX #$C7
            0xA0, 0x70,          // LDY #$70
            0x20, 0x28, 0xC7,    // JSR $C728   READMOUSE
            0x4C, 0x27, 0x03,    // JMP $0327   spin
        };
        addr = 0x0320;
        for (Byte b : kRead) { core.cpu->WriteByte (addr++, b); }

        core.cpu->SetPC (0x0320);
        core.RunCycles (100'000);

        // Slot-7 screen holes: $047F/$057F = X lo/hi, $04FF/$05FF = Y lo/hi.
        int  x = core.cpu->ReadByte (0x047F) | (core.cpu->ReadByte (0x057F) << 8);
        int  y = core.cpu->ReadByte (0x04FF) | (core.cpu->ReadByte (0x05FF) << 8);

        // Diagnostics for the firmware-oracle iteration loop.
        {
            char  diag[256];
            sprintf_s (diag,
                "DIAG: x=%d y=%d PC=%04X xyEn=%d vblEn=%d xInt=%02X yInt=%02X "
                "mode07FF=%02X status077F=%02X anyIrq=%d",
                x, y, core.cpu->GetPC(),
                core.mouse->XyInterruptsEnabled() ? 1 : 0,
                core.mouse->VblInterruptsEnabled() ? 1 : 0,
                core.mouse->ReadXInterruptStatus(),
                core.mouse->ReadYInterruptStatus(),
                core.cpu->ReadByte (0x07FF),
                core.cpu->ReadByte (0x077F),
                core.interruptController->IsAnyAsserted() ? 1 : 0);
            Logger::WriteMessage (diag);
        }

        Assert::AreEqual (5, x, L"firmware-tracked X after +5 units");
        Assert::AreEqual (3, y, L"firmware-tracked Y after +3 units");

        // Button: press, READMOUSE again, status hole $077F bit 7 = down.
        core.mouse->SetButton (true);
        core.cpu->SetPC (0x0320);
        core.RunCycles (100'000);

        Byte  status = core.cpu->ReadByte (0x077F);
        Assert::IsTrue ((status & 0x80) != 0, L"$077F bit 7: button currently down");

        // ---- Absolute targeting (the GUI path) ---------------------
        // Publish a mid-viewport fraction; the DEVICE must project it into
        // the firmware's live clamp window (read from the screen holes on
        // the CPU thread) and march the firmware there one interrupt per
        // unit. Default clamps are 0..1023, so 50%/50% ~= (511, 511).
        // Regression: the original UI-thread PeekByte mapping read stale
        // memory and silently no-oped in production (X/Y stuck at 0).
        core.mouse->SetHostTargetFraction (0x8000, 0x8000);
        core.RunCycles (8'000'000);

        core.cpu->SetPC (0x0320);                          // READMOUSE stub
        core.RunCycles (100'000);

        int  tx = core.cpu->ReadByte (0x047F) | (core.cpu->ReadByte (0x057F) << 8);
        int  ty = core.cpu->ReadByte (0x04FF) | (core.cpu->ReadByte (0x05FF) << 8);
        char  msg[96];
        sprintf_s (msg, "absolute target -> firmware position (%d, %d)", tx, ty);
        Logger::WriteMessage (msg);
        Assert::IsTrue (tx > 495 && tx < 528, L"absolute X lands near mid-clamp (~511)");
        Assert::IsTrue (ty > 495 && ty < 528, L"absolute Y lands near mid-clamp (~511)");
    }


    // DIAGNOSTIC (user repro): boot the user's writable DOS 3.3 disk, type
    // the corrected BASIC mouse program (PR#7 + CHR$(1) to turn the mouse
    // on, IN#7 to redirect input), inject host motion, and dump the screen.
    // Validates the BASIC IN#/PR# firmware hook path the protocol-entry
    // oracle test does not cover. Skips unless the local disk exists.
    TEST_METHOD (Diag_BasicInSevenHookTracksMotion)
    {
        const char *  kDiskPath = "C:\\Users\\relmer\\AppData\\Local\\Casso\\Disks\\DOS 3.3 Writable.woz";
        std::ifstream f (kDiskPath, std::ios::binary);
        if (!Apple2cRomAvailable() || !f.good())
        {
            Logger::WriteMessage ("SKIPPED: ROM or local DOS 3.3 disk absent");
            return;
        }
        std::vector<uint8_t>  bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char> ());

        HeadlessHost  host;
        EmulatorCore  core;
        Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)));
        core.PowerCycle();
        Assert::IsTrue (SUCCEEDED (core.diskStore->MountFromBytes (6, 0, kDiskPath, DiskFormat::Woz, bytes)));
        core.diskController->SetExternalDisk (0, core.diskStore->GetImage (6, 0));

        core.RunCycles (60'000'000);                       // boot DOS 3.3 to ]
        auto dump = [&] (const char * tag)
        {
            Logger::WriteMessage (tag);
            for (const std::string & row : TextScreenScraper::Scrape (core))
            {
                Logger::WriteMessage (row.c_str());
            }
        };

        KeystrokeInjector::InjectLine (core, "10 D$=CHR$(4)");
        KeystrokeInjector::InjectLine (core, "20 PRINT D$;\"PR#7\":PRINT CHR$(1):PRINT D$;\"PR#0\"");
        KeystrokeInjector::InjectLine (core, "30 PRINT D$;\"IN#7\"");
        KeystrokeInjector::InjectLine (core, "40 INPUT \"\";X,Y,B");
        KeystrokeInjector::InjectLine (core, "50 PRINT X;\" \";Y;\" \";B");
        KeystrokeInjector::InjectLine (core, "60 GOTO 40");
        KeystrokeInjector::InjectLine (core, "RUN", 2'000'000);

        core.mouse->MoveBy (+7, +4);                       // host motion
        core.RunCycles (4'000'000);
        core.mouse->SetButton (true);
        core.RunCycles (2'000'000);
        dump ("---- screen after RUN + motion ----");
        char  st[128];
        sprintf_s (st, "xyEn=%d mode07FF=%02X PC=%04X",
                   core.mouse->XyInterruptsEnabled() ? 1 : 0,
                   core.cpu->ReadByte (0x07FF), core.cpu->GetPC());
        Logger::WriteMessage (st);
    }


    // DIAGNOSTIC control (env-gated): the SAME DOS 3.3 disk + SAVE flow on a
    // //e with a plain Disk II controller (no IWM mode). Distinguishes an
    // IWM-mode-specific write bug from a general harness/DOS-save issue.
    TEST_METHOD (Diag_ControlSaveOnApple2e)
    {
        char    envBuf[8] = {};
        size_t  envLen    = 0;
        if (getenv_s (&envLen, envBuf, sizeof (envBuf), "CASSO_DIAG_SAVE_MOUSETEST") != 0
            || envLen == 0 || envBuf[0] != '1')
        {
            Logger::WriteMessage ("SKIPPED: env gate");
            return;
        }
        const char *  kDiskPath = "C:\\Users\\relmer\\AppData\\Local\\Casso\\Disks\\DOS 3.3 Writable.woz";
        std::ifstream f (kDiskPath, std::ios::binary);
        if (!f.good ()) { Logger::WriteMessage ("SKIPPED: disk absent"); return; }
        std::vector<uint8_t>  bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char> ());

        HeadlessHost  host;
        EmulatorCore  core;
        Assert::IsTrue (SUCCEEDED (host.BuildApple2eWithDisk2 (core)));
        core.diskController->SetIwmMode (true);   // discriminator: IWM vs 65C02
        core.PowerCycle();
        Assert::IsTrue (SUCCEEDED (core.diskStore->MountFromBytes (6, 0, "control.woz", DiskFormat::Woz, bytes)));
        core.diskController->SetExternalDisk (0, core.diskStore->GetImage (6, 0));
        core.RunCycles (60'000'000);

        KeystrokeInjector::InjectLine (core, "NEW");
        KeystrokeInjector::InjectLine (core, "10 PRINT \"HI\"");
        KeystrokeInjector::InjectLine (core, "SAVE CONTROL.TEST", 12'000'000);
        KeystrokeInjector::InjectLine (core, "LOAD CONTROL.TEST", 12'000'000);
        KeystrokeInjector::InjectLine (core, "LIST", 2'000'000);

        Logger::WriteMessage ("---- //e control: after SAVE/LOAD/LIST ----");
        bool  ok = false;
        for (const std::string & row : TextScreenScraper::Scrape (core))
        {
            Logger::WriteMessage (row.c_str());
            if (row.find ("PRINT \"HI\"") != std::string::npos) { ok = true; }
        }
        DiskImage *  img = core.diskStore->GetImage (6, 0);
        char  diag[96];
        sprintf_s (diag, "//e control: dirty=%d listOk=%d",
                   (img != nullptr && img->IsDirty()) ? 1 : 0, ok ? 1 : 0);
        Logger::WriteMessage (diag);
        Assert::IsTrue (ok, L"//e control SAVE/LOAD/LIST must round-trip");
    }


    // DIAGNOSTIC / UTILITY (deliberately env-gated: MUTATES a user disk).
    // Replaces MOUSE.TEST on the user's writable DOS 3.3 disk with the
    // corrected BASIC mouse program (DOS-chained IN#/PR# + CHR$(1) mouse-on),
    // flushes the WOZ back to the file, then re-mounts the written file in a
    // fresh core and LISTs it to verify the save round-tripped. Runs only
    // when CASSO_DIAG_SAVE_MOUSETEST=1 is set; skips otherwise.
    TEST_METHOD (Diag_SaveFixedMouseTestToDisk)
    {
        char    envBuf[8] = {};
        size_t  envLen    = 0;
        if (getenv_s (&envLen, envBuf, sizeof (envBuf), "CASSO_DIAG_SAVE_MOUSETEST") != 0
            || envLen == 0 || envBuf[0] != '1')
        {
            Logger::WriteMessage ("SKIPPED: set CASSO_DIAG_SAVE_MOUSETEST=1 to run (mutates a user disk)");
            return;
        }

        const char *  kDiskPath = "C:\\Users\\relmer\\AppData\\Local\\Casso\\Disks\\DOS 3.3 Writable.woz";
        std::ifstream f (kDiskPath, std::ios::binary);
        if (!Apple2cRomAvailable() || !f.good())
        {
            Logger::WriteMessage ("SKIPPED: ROM or local DOS 3.3 disk absent");
            return;
        }
        std::vector<uint8_t>  bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char> ());
        f.close();

        auto dump = [] (EmulatorCore & c, const char * tag)
        {
            Logger::WriteMessage (tag);
            for (const std::string & row : TextScreenScraper::Scrape (c))
            {
                Logger::WriteMessage (row.c_str());
            }
        };

        // ---- Pass 1: boot, type the fixed program, SAVE, flush ----------
        {
            HeadlessHost  host;
            EmulatorCore  core;
            Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)));
            core.PowerCycle();
            Assert::IsTrue (SUCCEEDED (core.diskStore->MountFromBytes (6, 0, kDiskPath, DiskFormat::Woz, bytes)));
            core.diskController->SetExternalDisk (0, core.diskStore->GetImage (6, 0));
            core.RunCycles (60'000'000);                   // boot DOS 3.3 to ]

            KeystrokeInjector::InjectLine (core, "NEW");
            KeystrokeInjector::InjectLine (core, "10 D$=CHR$(4)");
            KeystrokeInjector::InjectLine (core, "20 PRINT D$;\"PR#7\":PRINT CHR$(1):PRINT D$;\"PR#0\"");
            KeystrokeInjector::InjectLine (core, "30 PRINT D$;\"IN#7\"");
            KeystrokeInjector::InjectLine (core, "40 INPUT \"\";X,Y,B");
            KeystrokeInjector::InjectLine (core, "50 PRINT X;\" \";Y;\" \";B");
            KeystrokeInjector::InjectLine (core, "60 GOTO 40");
            KeystrokeInjector::InjectLine (core, "SAVE MOUSE.TEST", 12'000'000);   // DOS write
            KeystrokeInjector::InjectLine (core, "CATALOG", 6'000'000);

            dump (core, "---- after SAVE + CATALOG ----");

            DiskImage *  img = core.diskStore->GetImage (6, 0);
            char  diag[128];
            sprintf_s (diag, "image dirty=%d writeProtected=%d",
                       (img != nullptr && img->IsDirty()) ? 1 : 0,
                       (img != nullptr && img->IsWriteProtected()) ? 1 : 0);
            Logger::WriteMessage (diag);

            Assert::IsTrue (SUCCEEDED (core.diskStore->FlushAll ()), L"flush WOZ back to file");
        }

        // ---- Pass 2: fresh core, mount the WRITTEN file, LOAD + LIST ----
        {
            std::ifstream f2 (kDiskPath, std::ios::binary);
            Assert::IsTrue (f2.good (), L"written file must exist");
            std::vector<uint8_t>  bytes2 ((std::istreambuf_iterator<char> (f2)), std::istreambuf_iterator<char> ());

            HeadlessHost  host;
            EmulatorCore  core;
            Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)));
            core.PowerCycle();
            Assert::IsTrue (SUCCEEDED (core.diskStore->MountFromBytes (6, 0, kDiskPath, DiskFormat::Woz, bytes2)));
            core.diskController->SetExternalDisk (0, core.diskStore->GetImage (6, 0));
            core.RunCycles (60'000'000);

            KeystrokeInjector::InjectLine (core, "LOAD MOUSE.TEST", 12'000'000);
            KeystrokeInjector::InjectLine (core, "LIST", 3'000'000);
            dump (core, "---- LIST after reload from written file ----");

            bool  sawPr7 = false;
            for (const std::string & row : TextScreenScraper::Scrape (core))
            {
                if (row.find ("PR#7") != std::string::npos) { sawPr7 = true; }
            }
            Assert::IsTrue (sawPr7, L"reloaded MOUSE.TEST must contain the PR#7 mouse-on line");
        }
    }
};

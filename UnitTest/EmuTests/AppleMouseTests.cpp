#include "Pch.h"
#include "HeadlessHost.h"
#include "FixtureProvider.h"
#include "KeystrokeInjector.h"
#include "MachineIdle.h"
#include "TextScreenScraper.h"
#include "Devices/AppleMouse.h"
#include "Devices/Apple2eSoftSwitchBank.h"
#include "Core/InterruptController.h"
#include "Video/VideoTiming.h"


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

// Shared by both TEST_CLASSes below, so these live at file scope rather than
// on either one. `static` supplies the internal linkage the anonymous
// namespace was there for.
static constexpr size_t   s_kRomSize = 0x8000;

static bool Apple2cRomAvailable()
{
    FixtureProvider        fp;
    std::vector<uint8_t>   bytes;
    HRESULT                hrOpen = fp.OpenFixture ("Apple2c.rom", bytes);



    return SUCCEEDED (hrOpen) && bytes.size() == s_kRomSize;
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
//  AppleMouseDeviceTests
//
//  The //c's IOU mouse as a DEVICE: position latching, the button, and the two
//  interrupt sources.
//
//  Position is latched on the VBL EDGE rather than read live, which is the
//  behavior these pin -- guest software reads a coherent pair of coordinates
//  from one instant, and a live read would let X and Y come from different
//  moments mid-move.
//
//  The two interrupt sources are asserted independently, since VBL and movement
//  are separately enabled and a handler distinguishes them from the status
//  byte -- conflating them makes a mouse that moves generate phantom VBL
//  interrupts.
//
//  The mouse is ticked from the per-instruction cycle fan-out, so the tests
//  advance cycles rather than frames; that is what keeps interrupt pacing
//  locked to CPU progress.
//
////////////////////////////////////////////////////////////////////////////////

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

        AssertSucceeded (mouse.AttachInterruptController (&ic));
        EnableXyInterrupts (mouse);

        mouse.MoveBy (2, 0);
        mouse.Tick (AppleMouse::kSampleQuantum);
        Assert::IsTrue  (ic.IsAnyAsserted(), L"first unit must assert the line");
        Assert::AreEqual<Byte> (0x80, mouse.ReadXInterruptStatus(), L"$C015 bit 7 pending");

        // Level-held, not pulsed: many ticks later it is still asserted and
        // still the SAME single unit (no second latch before the ack).
        for (int i = 0; i < 100; ++i) { mouse.Tick (1); }
        Assert::IsTrue (ic.IsAnyAsserted(), L"line must hold until acknowledged");

        mouse.AccessRstXY();
        Assert::IsFalse (ic.IsAnyAsserted(),          L"$C048 ack must drop the line");
        Assert::AreEqual<Byte> (0x00, mouse.ReadXInterruptStatus(), L"ack clears the pending flag");

        mouse.Tick (AppleMouse::kSampleQuantum);
        Assert::IsTrue (ic.IsAnyAsserted(), L"second queued unit re-asserts after ack");

        mouse.AccessRstXY();
        mouse.Tick (AppleMouse::kSampleQuantum);
        Assert::IsFalse (ic.IsAnyAsserted(), L"queue drained: no third interrupt");
    }


    // $C015/$C017 are STATUS, not acknowledges, despite being named RSTXINT
    // and RSTYINT (and despite MAME lowering its mouse IRQ on these reads).
    // The real ROM 4 firmware polls them inside its service loop and acks with
    // $C048; making the read clear the latch cost it movement units -- the
    // firmware oracle below tracked 2 of 5. Repeated reads must therefore be
    // idempotent, and only $C048 may drop the line.
    TEST_METHOD (ReadingStatusRegisters_DoesNotAcknowledge_OnlyRstxyDoes)
    {
        InterruptController  ic;
        AppleMouse           mouse;

        AssertSucceeded (mouse.AttachInterruptController (&ic));
        EnableXyInterrupts (mouse);

        mouse.MoveBy (1, 1);
        mouse.Tick (AppleMouse::kSampleQuantum);
        Assert::IsTrue (ic.IsAnyAsserted(), L"movement must assert the line");

        for (int i = 0; i < 3; ++i)
        {
            Assert::AreEqual<Byte> (0x80, mouse.ReadXInterruptStatus(), L"$C015 keeps reporting X");
            Assert::AreEqual<Byte> (0x80, mouse.ReadYInterruptStatus(), L"$C017 keeps reporting Y");
        }

        Assert::IsTrue (ic.IsAnyAsserted(), L"polling status must not acknowledge");

        mouse.AccessRstXY();
        Assert::AreEqual<Byte> (0x00, mouse.ReadXInterruptStatus(), L"$C048 clears X");
        Assert::AreEqual<Byte> (0x00, mouse.ReadYInterruptStatus(), L"$C048 clears Y");
        Assert::IsFalse        (ic.IsAnyAsserted(),                 L"and drops the line");
    }


    // $C019 is NOT part of the read-to-acknowledge family, which is the whole
    // reason it gets its own case: on the //c it flags "a VBL IRQ fired" and
    // survives its own read, clearing only on a $C07X access (Apple //c
    // Technical Note #9). An I/O reference calling it "RSTVBL ... remains set
    // until software reads this location" prompted exactly that mistake here.
    TEST_METHOD (ReadingVblFlag_DoesNotAcknowledgeIt_OnlyPtrigDoes)
    {
        InterruptController  ic;
        AppleMouse           mouse;
        VideoTiming          vt;
        auto                 advance = [&] (uint32_t cyc) { vt.Tick (cyc); mouse.Tick (cyc); };

        AssertSucceeded (mouse.AttachInterruptController (&ic));
        mouse.SetVideoTiming (&vt);

        mouse.WriteIouAccess  (true);
        mouse.AccessIouSwitch (s_kEnbVbl);
        mouse.WriteIouAccess  (false);

        advance (VideoTiming::kVblankStartCycle + 1);
        Assert::IsTrue (ic.IsAnyAsserted(), L"VBL onset must assert the line");

        // Reading it twice must report the flag twice: the read has no side
        // effect, so a firmware poll cannot accidentally acknowledge.
        Assert::AreEqual<Byte> (0x80, mouse.ReadVblInterrupt(), L"$C019 reports the flag");
        Assert::AreEqual<Byte> (0x80, mouse.ReadVblInterrupt(), L"and still reports it after a read");
        Assert::IsTrue         (ic.IsAnyAsserted(),            L"the line survives the read");

        mouse.AccessPtrig();
        Assert::AreEqual<Byte> (0x00, mouse.ReadVblInterrupt(), L"$C07X is what clears it");
        Assert::IsFalse        (ic.IsAnyAsserted(),            L"and drops the line");
    }


    // $C066/$C067 direction-line polarity, exactly as the firmware's service
    // loop consumes them: MOUX1 bit 7 = 1 -> X increments; MOUY1 is inverted
    // by the firmware (EOR #$80), so bit 7 = 0 -> Y increments.
    TEST_METHOD (DirectionLines_MatchFirmwarePolarity)
    {
        InterruptController  ic;
        AppleMouse           mouse;

        AssertSucceeded (mouse.AttachInterruptController (&ic));
        EnableXyInterrupts (mouse);

        mouse.MoveBy (+1, +1);
        mouse.Tick (AppleMouse::kSampleQuantum);
        Assert::AreEqual<Byte> (0x80, mouse.ReadMouX1(), L"+X (right) -> MOUX1 bit 7 set");
        Assert::AreEqual<Byte> (0x00, mouse.ReadMouY1(), L"+Y (down)  -> MOUY1 bit 7 clear");

        mouse.AccessRstXY();
        mouse.MoveBy (-1, -1);
        mouse.Tick (AppleMouse::kSampleQuantum);
        Assert::AreEqual<Byte> (0x00, mouse.ReadMouX1(), L"-X (left) -> MOUX1 bit 7 clear");
        Assert::AreEqual<Byte> (0x80, mouse.ReadMouY1(), L"-Y (up)   -> MOUY1 bit 7 set");
    }


    // VBL: the latch sets at vblank onset regardless of ENVBL (the enable
    // masks only the IRQ line), reads at $C019 bit 7, and clears on the
    // $C070 access. The line is gated by ENVBL/DISVBL.
    TEST_METHOD (VblLatch_OnsetSetsFlag_EnvblGatesLine_PtrigClears)
    {
        InterruptController  ic;
        AppleMouse           mouse;
        VideoTiming          vt;

        AssertSucceeded (mouse.AttachInterruptController (&ic));
        mouse.SetVideoTiming (&vt);

        // Video timing and the mouse both receive the same cycle count from
        // AddCycles, so advance them in lockstep. The mouse samples the vblank
        // line on a coarse cadence (kSampleQuantum) rather than every tick, so
        // it must be fed realistic cycle counts -- a Tick(1) probe is not
        // guaranteed to land a sample. The vblank window (~4550 cycles) dwarfs
        // that cadence, so the onset edge is always caught.
        auto advance = [&] (uint32_t cyc) { vt.Tick (cyc); mouse.Tick (cyc); };

        // Masked VBL: tick into vblank -- flag latches, line stays low.
        advance (VideoTiming::kVblankStartCycle + 1);
        Assert::AreEqual<Byte> (0x80, mouse.ReadVblInterrupt(), L"latch sets at onset even when masked");
        Assert::IsFalse (ic.IsAnyAsserted(), L"DISVBL (default) masks the line");

        // Enable: pending latch surfaces on the line immediately.
        mouse.WriteIouAccess (true);
        mouse.AccessIouSwitch (s_kEnbVbl);
        mouse.WriteIouAccess (false);
        Assert::IsTrue (ic.IsAnyAsserted(), L"ENVBL with a pending latch asserts");

        // $C070 acknowledge.
        mouse.AccessPtrig();
        Assert::AreEqual<Byte> (0x00, mouse.ReadVblInterrupt(), L"$C070 clears the latch");
        Assert::IsFalse (ic.IsAnyAsserted(), L"ack drops the line");

        // Next frame's onset latches again (edge, not level): tick through
        // the display period (so the mouse observes not-vblank) and into the
        // following vblank.
        advance (VideoTiming::kCyclesPerFrame - 2000);   // wraps into display
        Assert::AreEqual<Byte> (0x00, mouse.ReadVblInterrupt(), L"still clear during display");
        advance (4000);                                   // next vblank onset
        Assert::AreEqual<Byte> (0x80, mouse.ReadVblInterrupt(), L"next onset re-latches");
        Assert::IsTrue (ic.IsAnyAsserted(), L"enabled + latched -> asserted");
    }


    // $C063: ACTIVE LOW. Idle high (0x80); a press pulls the line to 0.
    TEST_METHOD (Button_ActiveLow)
    {
        AppleMouse  mouse;

        Assert::AreEqual<Byte> (0x80, mouse.ReadButton(), L"released idles high");
        mouse.SetButton (true);
        Assert::AreEqual<Byte> (0x00, mouse.ReadButton(), L"pressed pulls low");
        mouse.SetButton (false);
        Assert::AreEqual<Byte> (0x80, mouse.ReadButton(), L"release restores high");
    }




    // DISXY masks the line without discarding the pending flags; re-enabling
    // surfaces them again (mask, not clear).
    TEST_METHOD (DisXy_MasksLineWithoutClearingFlags)
    {
        InterruptController  ic;
        AppleMouse           mouse;

        AssertSucceeded (mouse.AttachInterruptController (&ic));
        EnableXyInterrupts (mouse);

        mouse.MoveBy (1, 0);
        mouse.Tick (AppleMouse::kSampleQuantum);
        Assert::IsTrue (ic.IsAnyAsserted());

        mouse.WriteIouAccess (true);
        mouse.AccessIouSwitch (s_kDisXy);
        Assert::IsFalse (ic.IsAnyAsserted(),           L"DISXY masks the line");
        Assert::AreEqual<Byte> (0x80, mouse.ReadXInterruptStatus(), L"flag survives the mask");

        mouse.AccessIouSwitch (s_kEnbXy);
        mouse.WriteIouAccess (false);
        Assert::IsTrue (ic.IsAnyAsserted(), L"re-enable surfaces the pending flag");
    }


    // BOTH documented address pairs drive the one IOUDIS latch, through the
    // bank that decodes them: $C078/$C079 (the pair ROM 4's firmware uses) and
    // $C07E/$C07F (SETIOUDIS/CLRIOUDIS). Apple //c Technical Note #9 documents
    // the second pair for VBL polling -- "turn IOUDis off by writing to $C07F,
    // then access ENVBL at $C05B" -- so a program following it must reach the
    // mouse switches, not the annunciators.
    TEST_METHOD (BothIouDisAddressPairs_GateTheMouseSwitches)
    {
        Apple2eSoftSwitchBank  bank (nullptr);
        AppleMouse             mouse;

        bank.SetMouse (&mouse);

        bank.Read (0xC079);
        Assert::IsTrue  (mouse.IsIouAccessEnabled(), L"$C079 enables IOU access");
        bank.Read (0xC078);
        Assert::IsFalse (mouse.IsIouAccessEnabled(), L"$C078 disables it");

        bank.Read (0xC07F);
        Assert::IsTrue  (mouse.IsIouAccessEnabled(), L"$C07F (CLRIOUDIS) must enable it too");
        bank.Read (0xC07E);
        Assert::IsFalse (mouse.IsIouAccessEnabled(), L"$C07E (SETIOUDIS) must disable it");
    }
};





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

    // ROM 4 (Memory Expansion //c) identifies the mouse at phantom slot 7:
    // the Pascal pointing-device signature $C705=$38 $C707=$18 $C70B=$01
    // $C70C=$20 $C7FB=$D6, read through the live bus (no-slots $Cxxx
    // routing). This is how MousePaint-class software finds the mouse.
    TEST_METHOD (FirmwareIdentifiesMouseAtSlot7)
    {
        HeadlessHost   host;
        EmulatorCore   core;



        if (!Apple2cRomAvailable())
        {
            Logger::WriteMessage ("SKIPPED: no Apple2c.rom fixture");
            return;
        }


        AssertSucceeded (host.BuildApple2c (core));
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
        // The real ROM 4 firmware is the oracle here, so with no ROM fixture
        // there is nothing to test against -- skip rather than assert.
        if (!Apple2cRomAvailable())
        {
            Logger::WriteMessage ("SKIPPED: no Apple2c.rom fixture");
        }
        else
        {
            HeadlessHost  host;
            EmulatorCore  core;
            Word          addr    = 0;
            int           x       = 0;
            int           y       = 0;
            Byte          status  = 0;
            int           tx      = 0;
            int           ty      = 0;
            char          msg[96];

            AssertSucceeded (host.BuildApple2c (core));
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
            addr = 0x0300;
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
            x = core.cpu->ReadByte (0x047F) | (core.cpu->ReadByte (0x057F) << 8);
            y = core.cpu->ReadByte (0x04FF) | (core.cpu->ReadByte (0x05FF) << 8);

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

            status = core.cpu->ReadByte (0x077F);
            Assert::IsTrue ((status & 0x80) != 0, L"$077F bit 7: button currently down");

            // Absolute targeting (the GUI path)
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

            tx = core.cpu->ReadByte (0x047F) | (core.cpu->ReadByte (0x057F) << 8);
            ty = core.cpu->ReadByte (0x04FF) | (core.cpu->ReadByte (0x05FF) << 8);
            sprintf_s (msg, "absolute target -> firmware position (%d, %d)", tx, ty);
            Logger::WriteMessage (msg);
            Assert::IsTrue (tx > 495 && tx < 528, L"absolute X lands near mid-clamp (~511)");
            Assert::IsTrue (ty > 495 && ty < 528, L"absolute Y lands near mid-clamp (~511)");
        }
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
        // Needs both the ROM fixture and a machine-local DOS 3.3 disk, so this
        // one only runs on a developer box that has them.
        if (!Apple2cRomAvailable() || !f.good())
        {
            Logger::WriteMessage ("SKIPPED: ROM or local DOS 3.3 disk absent");
        }
        else
        {
            HeadlessHost  host;
            EmulatorCore  core;
            char          st[128];

            std::vector<uint8_t>  bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char> ());

            AssertSucceeded (host.BuildApple2c (core));
            core.PowerCycle();
            AssertSucceeded (core.diskStore->MountFromBytes (6, 0, kDiskPath, DiskFormat::Woz, bytes));
            core.diskController->SetExternalDisk (0, core.diskStore->GetImage (6, 0));

            MachineIdle::RunUntilIdle (core, 60'000'000);                       // boot DOS 3.3 to ]
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
            sprintf_s (st, "xyEn=%d mode07FF=%02X PC=%04X",
                       core.mouse->XyInterruptsEnabled() ? 1 : 0,
                       core.cpu->ReadByte (0x07FF), core.cpu->GetPC());
            Logger::WriteMessage (st);
        }
    }


    // DIAGNOSTIC control (env-gated): the SAME DOS 3.3 disk + SAVE flow on a
    // //e with a plain Disk II controller (no IWM mode). Distinguishes an
    // IWM-mode-specific write bug from a general harness/DOS-save issue.
    TEST_METHOD (Diag_ControlSaveOnApple2e)
    {
        size_t        envLen    = 0;
        const char *  skipWhy   = nullptr;
        char          envBuf[8] = {};



        const char *  kDiskPath = "C:\\Users\\relmer\\AppData\\Local\\Casso\\Disks\\DOS 3.3 Writable.woz";

        // Two separate reasons to sit this one out, reported separately so a
        // developer who set the env var still learns the disk is missing.
        if (getenv_s (&envLen, envBuf, sizeof (envBuf), "CASSO_DIAG_SAVE_MOUSETEST") != 0
            || envLen == 0 || envBuf[0] != '1')
        {
            skipWhy = "SKIPPED: env gate";
        }

        std::ifstream f (kDiskPath, std::ios::binary);

        if (skipWhy == nullptr && !f.good())
        {
            skipWhy = "SKIPPED: disk absent";
        }

        if (skipWhy != nullptr)
        {
            Logger::WriteMessage (skipWhy);
        }
        else
        {
            HeadlessHost    host;
            EmulatorCore    core;
            bool            ok       = false;
            DiskImage     * img      = nullptr;
            char            diag[96];

            std::vector<uint8_t>  bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char> ());

            AssertSucceeded (host.BuildApple2eWithDisk2 (core));
            core.diskController->SetIwmMode (true);   // discriminator: IWM vs 65C02
            core.PowerCycle();
            AssertSucceeded (core.diskStore->MountFromBytes (6, 0, "control.woz", DiskFormat::Woz, bytes));
            core.diskController->SetExternalDisk (0, core.diskStore->GetImage (6, 0));
            MachineIdle::RunUntilIdle (core, 60'000'000);

            KeystrokeInjector::InjectLine (core, "NEW");
            KeystrokeInjector::InjectLine (core, "10 PRINT \"HI\"");
            KeystrokeInjector::InjectLine (core, "SAVE CONTROL.TEST", 12'000'000);
            KeystrokeInjector::InjectLine (core, "LOAD CONTROL.TEST", 12'000'000);
            KeystrokeInjector::InjectLine (core, "LIST", 2'000'000);

            Logger::WriteMessage ("---- //e control: after SAVE/LOAD/LIST ----");
            for (const std::string & row : TextScreenScraper::Scrape (core))
            {
                Logger::WriteMessage (row.c_str());
                if (row.find ("PRINT \"HI\"") != std::string::npos) { ok = true; }
            }

            img = core.diskStore->GetImage (6, 0);
            sprintf_s (diag, "//e control: dirty=%d listOk=%d",
                       (img != nullptr && img->IsDirty()) ? 1 : 0, ok ? 1 : 0);
            Logger::WriteMessage (diag);
            Assert::IsTrue (ok, L"//e control SAVE/LOAD/LIST must round-trip");
        }
    }


    // DIAGNOSTIC / UTILITY (deliberately env-gated: MUTATES a user disk).
    // Replaces MOUSE.TEST on the user's writable DOS 3.3 disk with the
    // corrected BASIC mouse program (DOS-chained IN#/PR# + CHR$(1) mouse-on),
    // flushes the WOZ back to the file, then re-mounts the written file in a
    // fresh core and LISTs it to verify the save round-tripped. Runs only
    // when CASSO_DIAG_SAVE_MOUSETEST=1 is set; skips otherwise.
    TEST_METHOD (Diag_SaveFixedMouseTestToDisk)
    {
        size_t        envLen    = 0;
        const char *  skipWhy   = nullptr;
        char          envBuf[8] = {};



        const char *  kDiskPath = "C:\\Users\\relmer\\AppData\\Local\\Casso\\Disks\\DOS 3.3 Writable.woz";

        // The env gate is checked FIRST and reported on its own: this test
        // rewrites a file on the developer's disk, so "you did not opt in" has
        // to be distinguishable from "the disk is not there".
        if (getenv_s (&envLen, envBuf, sizeof (envBuf), "CASSO_DIAG_SAVE_MOUSETEST") != 0
            || envLen == 0 || envBuf[0] != '1')
        {
            skipWhy = "SKIPPED: set CASSO_DIAG_SAVE_MOUSETEST=1 to run (mutates a user disk)";
        }

        std::ifstream f (kDiskPath, std::ios::binary);

        if (skipWhy == nullptr && (!Apple2cRomAvailable() || !f.good()))
        {
            skipWhy = "SKIPPED: ROM or local DOS 3.3 disk absent";
        }

        if (skipWhy != nullptr)
        {
            Logger::WriteMessage (skipWhy);
        }
        else
        {
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

            // Pass 1: boot, type the fixed program, SAVE, flush
            {
                HeadlessHost    host;
                EmulatorCore    core;
                DiskImage     * img       = nullptr;
                char            diag[128];
                AssertSucceeded (host.BuildApple2c (core));
                core.PowerCycle();
                AssertSucceeded (core.diskStore->MountFromBytes (6, 0, kDiskPath, DiskFormat::Woz, bytes));
                core.diskController->SetExternalDisk (0, core.diskStore->GetImage (6, 0));
                MachineIdle::RunUntilIdle (core, 60'000'000);                   // boot DOS 3.3 to ]

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

                img = core.diskStore->GetImage (6, 0);
                sprintf_s (diag, "image dirty=%d writeProtected=%d",
                           (img != nullptr && img->IsDirty()) ? 1 : 0,
                           (img != nullptr && img->IsWriteProtected()) ? 1 : 0);
                Logger::WriteMessage (diag);

                AssertSucceeded (core.diskStore->FlushAll(), L"flush WOZ back to file");
            }

            // Pass 2: fresh core, mount the WRITTEN file, LOAD + LIST
            {
                HeadlessHost  host;
                EmulatorCore  core;
                bool          sawPr7 = false;

                std::ifstream f2 (kDiskPath, std::ios::binary);
                Assert::IsTrue (f2.good(), L"written file must exist");
                std::vector<uint8_t>  bytes2 ((std::istreambuf_iterator<char> (f2)), std::istreambuf_iterator<char> ());

                AssertSucceeded (host.BuildApple2c (core));
                core.PowerCycle();
                AssertSucceeded (core.diskStore->MountFromBytes (6, 0, kDiskPath, DiskFormat::Woz, bytes2));
                core.diskController->SetExternalDisk (0, core.diskStore->GetImage (6, 0));
                MachineIdle::RunUntilIdle (core, 60'000'000);

                KeystrokeInjector::InjectLine (core, "LOAD MOUSE.TEST", 12'000'000);
                KeystrokeInjector::InjectLine (core, "LIST", 3'000'000);
                dump (core, "---- LIST after reload from written file ----");

                for (const std::string & row : TextScreenScraper::Scrape (core))
                {
                    if (row.find ("PR#7") != std::string::npos) { sawPr7 = true; }
                }

                Assert::IsTrue (sawPr7, L"reloaded MOUSE.TEST must contain the PR#7 mouse-on line");
            }
        }
    }
};

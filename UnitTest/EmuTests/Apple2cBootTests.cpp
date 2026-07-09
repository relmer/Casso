#include "Pch.h"
#include "HeadlessHost.h"
#include "TextScreenScraper.h"
#include "FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cBootTests
//
//  Spec 016 / US2. These exercise the Apple //c wiring against the real ROM 4.
//
//  The //c now cold-boots its firmware end-to-end (ColdBootsToCheckDiskDrive):
//  with no disk it clears the screen, shows the "Apple //c" banner, and reaches
//  the correct "Check Disk Drive." no-disk state. The bring-up chain that got
//  here, in boot order:
//
//    1. Rockwell bit ops (RMB/SMB/BBR/BBS): Cpu65C02 models the Rockwell R65C02
//       (ROM 4 runs BBS0 at $D010).
//    2. //c $C100-$CFFF routing: the //c has no card slots, so the whole window
//       (incl. the $C800 expansion space) always reads internal firmware --
//       CxxxRomRouter::SetNoExternalSlots(true).
//    3. $C028 ROM-bank toggle: the Apple2eKeyboard front device owns $C000-$C063
//       and forwards sub-ranges to its siblings; it now forwards $C028 to the
//       soft-switch bank (which drives the ROM-bank flip-flop). Paired with the
//       CPU store no longer pre-reading its target (a dummy read + the write
//       would double-toggle the flip-flop back).
//    4. Slot-6 IWM: the //c's built-in drive is an Integrated Woz Machine, so
//       Disk2Controller gains an IWM mode/status register (SetIwmMode) that the
//       reset firmware writes then reads back to confirm the drive is present.
//
//  Booting actual software still needs a bootable disk image (the //c has no
//  cassette / BASIC-on-cold-boot; "Check Disk Drive." is its terminal no-disk
//  state). The ROM fixture is copyrighted + uncommitted; tests skip when it is
//  absent so CI never needs a machine ROM.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr size_t     kRomSize = 0x8000;      // 32K, two 16K banks
    static constexpr Word       kMonitorReset = 0xFA62; // ROM 4 RESET vector target

    bool Apple2cRomAvailable ()
    {
        FixtureProvider        fp;
        std::vector<uint8_t>   bytes;
        return SUCCEEDED (fp.OpenFixture ("Apple2c.rom", bytes)) && bytes.size () == kRomSize;
    }
}


TEST_CLASS (Apple2cBootTests)
{
public:

    // The //c cold-boots its firmware end-to-end. With no disk inserted it
    // clears the screen, prints the "Apple //c" banner, probes the built-in IWM
    // drive, and -- finding no bootable disk -- displays "Check Disk Drive.",
    // the correct no-disk terminal state on real hardware. This exercises the
    // whole bring-up chain: the Rockwell 65C02, the $C028 ROM-bank switch, the
    // no-slots $Cxxx routing, and the slot-6 IWM mode/status register.
    TEST_METHOD (ColdBootsToCheckDiskDrive)
    {
        if (!Apple2cRomAvailable ())
        {
            Logger::WriteMessage ("SKIPPED: no Apple2c.rom fixture");
            return;
        }

        HeadlessHost host; EmulatorCore core;
        Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)), L"BuildApple2c");
        core.PowerCycle ();
        core.RunCycles (15'000'000);

        std::string screen;
        for (const auto & row : TextScreenScraper::Scrape (core))
        {
            screen += row;
            screen += '\n';
        }

        Assert::IsTrue (screen.find ("Apple //c") != std::string::npos,
            L"boot screen must show the Apple //c banner");
        Assert::IsTrue (screen.find ("Check Disk Drive") != std::string::npos,
            L"no-disk boot must reach the Check Disk Drive prompt");
    }

    // Regression for the $C028 ROM-bank flip-flop end-to-end through the CPU
    // and the full bus dispatch. Guards two fixes that must hold together:
    //   (a) Apple2eKeyboard (the $C000-$C063 front device) forwards $C028 to
    //       the soft-switch bank that drives the flip-flop, and
    //   (b) a CPU store no longer pre-reads its target -- a dummy read plus the
    //       store's write would toggle the flop twice (net no switch).
    // If either regresses, the bank stays on 0 and this fails.
    TEST_METHOD (StaC028TogglesRomBankExactlyOnce)
    {
        if (!Apple2cRomAvailable ())
        {
            Logger::WriteMessage ("SKIPPED: no Apple2c.rom fixture");
            return;
        }

        HeadlessHost host; EmulatorCore core;
        Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)), L"BuildApple2c");
        core.PowerCycle ();

        Assert::AreEqual (0, core.romBank->CurrentBank (), L"reset selects bank 0");

        auto execAt = [&] (Word at, std::initializer_list<Byte> bytes)
        {
            Word a = at;
            for (Byte b : bytes) core.cpu->WriteByte (a++, b);
            core.cpu->SetPC (at);
            core.cpu->StepOne ();
        };

        execAt (0x0300, { 0x8D, 0x28, 0xC0 });   // STA $C028
        Assert::AreEqual (1, core.romBank->CurrentBank (), L"STA $C028 toggles once");

        execAt (0x0300, { 0x8D, 0x28, 0xC0 });   // STA $C028 again
        Assert::AreEqual (0, core.romBank->CurrentBank (), L"second STA toggles back");

        execAt (0x0300, { 0xAD, 0x28, 0xC0 });   // LDA $C028 (any access flips it)
        Assert::AreEqual (1, core.romBank->CurrentBank (), L"LDA $C028 toggles once");
    }

    // Verifies the parts of the //c that ARE working end-to-end: the 65C02 +
    // 32K two-bank firmware wire up, and a cold reset lands on the monitor
    // entry with the ROM correctly mapped through the language card.
    TEST_METHOD (BuildsAndResetsToMonitorEntry)
    {
        if (!Apple2cRomAvailable ())
        {
            Logger::WriteMessage (
                "SKIP: UnitTest/Fixtures/Apple2c.rom absent "
                "(copyrighted //c ROM 4, provisioned on demand).");
            return;
        }

        HeadlessHost   host;
        EmulatorCore   core;

        Assert::IsTrue (SUCCEEDED (host.BuildApple2c (core)),
            L"BuildApple2c must succeed when the ROM is present");
        Assert::IsTrue (core.HasApple2e (),
            L"//c wiring (65C02 + MMU) must be complete");

        // Reset vector (read through the bus -> language card ROM, bank 0)
        // must point at the ROM 4 monitor entry.
        Word resetVec = static_cast<Word> (core.cpu->ReadByte (0xFFFC)) |
                        (static_cast<Word> (core.cpu->ReadByte (0xFFFD)) << 8);
        Assert::AreEqual<Word> (kMonitorReset, resetVec,
            L"RESET vector must select the bank-0 monitor entry $FA62");

        // $FA62 must decode as CLD ($D8): the classic monitor reset preamble,
        // i.e. the LC is serving bank-0 ROM at $D000-$FFFF.
        Assert::AreEqual<Byte> (0xD8, core.cpu->ReadByte (0xFA62),
            L"$FA62 must read CLD from the mapped monitor ROM");

        // The CPU powers on at the reset entry.
        core.PowerCycle ();
        Assert::AreEqual<Word> (kMonitorReset, core.cpu->GetPC (),
            L"Cold reset must enter the monitor at $FA62");
    }
};

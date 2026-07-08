#include "Pch.h"
#include "HeadlessHost.h"
#include "FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cBootTests
//
//  Spec 016 / US2. These exercise the Apple //c wiring against the real ROM 4.
//
//  IMPORTANT -- the //c does NOT yet cold-boot to Applesoft. Bring-up so far
//  proves the banking + ROM mapping + 65C02 reset are correct (the CPU resets
//  to the monitor entry $FA62 and begins executing firmware), but a full boot
//  is blocked on three things found during bring-up:
//
//    1. Rockwell bit ops (RMB/SMB/BBR/BBS). ROM 4 executes BBS0 at $D010; with
//       the base-tier NOP decode the reset derails into RAM. Cpu65C02 has an
//       InstallBitOps() ready but not called (a parked base-tier decision).
//    2. //c $C800-$CFFF routing. With bit ops on, the firmware next jumps into
//       the $C800 expansion window, which the //e CxxxRomRouter leaves as
//       floating bus ($FF) until INTC8ROM latches. The //c has no slots, so
//       that window must always read the internal firmware.
//    3. Built-in peripherals (serial 6551 ACIA / IWM disk) the firmware probes.
//
//  So the "reaches BASIC" assertion is intentionally absent -- it would only
//  pass on garbage. The ROM fixture is copyrighted + uncommitted; tests skip
//  when it is absent so CI never needs a machine ROM.
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

#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "HeadlessHost.h"
#include "TextScreenScraper.h"
#include "KeystrokeInjector.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace
{
    static constexpr uint64_t   kColdBootCycles      = 5000000ULL;
    static constexpr uint64_t   kAfterCommandCycles  = 1000000ULL;
    static constexpr int        kBasicPromptRow      = 23;
    static constexpr Word       kRdVblBar            = 0xC019;
    static constexpr Word       kRd80Vid             = 0xC01F;
    static constexpr Word       kRd80Store           = 0xC018;
    static constexpr Byte       kBitSeven            = 0x80;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmuIntegrationTests (Phase 7)
//
//  User Story 1 (P1) MVP. A stock //e cold-boots through `apple2e.rom` to
//  the Applesoft `]` prompt; injected `HOME` / `PRINT "HELLO"` / `PR#3`
//  produce the expected text-screen state including the 80-column
//  transition. Each scenario powers the //e on via HeadlessHost +
//  IFixtureProvider only -- constitution §II compliant, no host I/O.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (EmuIntegrationTests)
{
public:

    ////////////////////////////////////////////////////////////////////////
    //
    //  Helper: Boot a //e to the Applesoft prompt.
    //
    ////////////////////////////////////////////////////////////////////////

    static void BootToPrompt (HeadlessHost & host, EmulatorCore & core)
    {
        HRESULT   hr;

        hr = host.BuildAppleIIe (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIe must succeed");
        Assert::IsTrue (core.HasAppleIIe (), L"//e wiring must be complete");

        core.PowerCycle ();
        core.RunCycles  (kColdBootCycles);
    }

    ////////////////////////////////////////////////////////////////////////
    //
    //  Helper: Find a row containing `needle`, return its index or -1.
    //
    ////////////////////////////////////////////////////////////////////////

    static int FindRowContaining (
        const std::vector<std::string>  &  rows,
        const std::string               &  needle)
    {
        size_t   i;

        for (i = 0; i < rows.size (); i++)
        {
            if (rows[i].find (needle) != std::string::npos)
            {
                return static_cast<int> (i);
            }
        }

        return -1;
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US1 / T069: Cold boot reaches the Applesoft `]` prompt.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase7_ColdBootReaches_BASIC_Prompt)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        int            promptRow;

        BootToPrompt (host, core);

        std::vector<std::string>   rows = TextScreenScraper::Scrape (core);

        Assert::AreEqual (size_t (TextScreenScraper::kRows), rows.size (),
            L"Scraper must produce 24 rows");

        promptRow = FindRowContaining (rows, "]");

        Assert::IsTrue (promptRow >= 0,
            L"Applesoft `]` prompt must appear somewhere on the text screen");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US1 / T069 deterministic: re-running cold boot produces the same
    //  scrape (FR-035 / SC-002). The pinned PRNG seed + identical ROM +
    //  identical cycle budget must yield identical screen state.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase7_ColdBoot_IsDeterministic)
    {
        HeadlessHost   hostA;
        HeadlessHost   hostB;
        EmulatorCore   coreA;
        EmulatorCore   coreB;

        BootToPrompt (hostA, coreA);
        BootToPrompt (hostB, coreB);

        std::vector<std::string>   rowsA = TextScreenScraper::Scrape (coreA);
        std::vector<std::string>   rowsB = TextScreenScraper::Scrape (coreB);

        Assert::AreEqual (rowsA.size (), rowsB.size ());

        for (size_t i = 0; i < rowsA.size (); i++)
        {
            Assert::IsTrue (rowsA[i] == rowsB[i],
                L"Two cold boots with the pinned seed must produce identical scrapes");
        }
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US1 / T070: HOME, PRINT "HELLO" appear on the screen above the
    //  next prompt row.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase7_HOME_PRINT_HELLO_Visible)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        size_t         consumed1;
        size_t         consumed2;
        int            helloRow;
        int            promptRow;

        BootToPrompt (host, core);

        consumed1 = KeystrokeInjector::InjectLine (core, "HOME");
        Assert::AreEqual (size_t (5), consumed1,
            L"HOME + Return should be fully consumed");

        consumed2 = KeystrokeInjector::InjectLine (core, "PRINT \"HELLO\"");
        Assert::AreEqual (size_t (14), consumed2,
            L"PRINT \"HELLO\" + Return should be fully consumed");

        core.RunCycles (kAfterCommandCycles);

        std::vector<std::string>   rows = TextScreenScraper::Scrape (core);

        helloRow = FindRowContaining (rows, "HELLO");

        Assert::IsTrue (helloRow >= 0,
            L"`HELLO` must appear on the text screen after PRINT \"HELLO\"");

        promptRow = -1;

        for (size_t i = static_cast<size_t> (helloRow + 1); i < rows.size (); i++)
        {
            if (rows[i].find (']') != std::string::npos)
            {
                promptRow = static_cast<int> (i);
                break;
            }
        }

        Assert::IsTrue (promptRow > helloRow,
            L"A new `]` prompt row must appear below the HELLO line");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US1 / T071: PR#3 activates 80-column mode (RD80VID + RD80STORE).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase7_PR3_Activates_80Column_Mode)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        size_t         consumed;
        Byte           rd80Vid;
        Byte           rd80Store;

        BootToPrompt (host, core);

        consumed = KeystrokeInjector::InjectLine (core, "PR#3");
        Assert::AreEqual (size_t (5), consumed,
            L"PR#3 + Return should be fully consumed");

        core.RunCycles (kAfterCommandCycles);

        Assert::IsTrue (core.softSwitches->Is80ColMode (),
            L"PR#3 must engage 80-col mode (Is80ColMode==true)");

        rd80Vid   = core.bus->ReadByte (kRd80Vid);
        rd80Store = core.bus->ReadByte (kRd80Store);

        Assert::IsTrue ((rd80Vid & kBitSeven) != 0,
            L"RD80VID ($C01F) bit 7 must be set with 80-col active");
        Assert::IsTrue ((rd80Store & kBitSeven) != 0,
            L"RD80STORE ($C018) bit 7 must be set when firmware engages 80STORE");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US1 / T072: PR#3 then PRINT "ABC..." renders correctly across the
    //  main+aux interleave on the 80-column scrape.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase7_PR3_Then_PRINT_LongString_80ColScrape)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        const char *   needle = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        size_t         consumed;
        int            row;

        BootToPrompt (host, core);

        consumed = KeystrokeInjector::InjectLine (core, "PR#3");
        Assert::AreEqual (size_t (5), consumed);

        core.RunCycles (kAfterCommandCycles);

        Assert::IsTrue (core.softSwitches->Is80ColMode (),
            L"80-col must be active before the long PRINT");

        consumed = KeystrokeInjector::InjectLine (
            core,
            "PRINT \"ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789012345678901234567890\"");

        Assert::IsTrue (consumed >= 60,
            L"Long PRINT line must be fully consumed");

        core.RunCycles (kAfterCommandCycles);

        std::vector<std::string>   rows = TextScreenScraper::Scrape (core);

        Assert::AreEqual (size_t (TextScreenScraper::kCols80), rows[0].size (),
            L"80-col scrape rows must be 80 chars wide");

        row = FindRowContaining (rows, needle);

        Assert::IsTrue (row >= 0,
            L"ABC..XYZ glyphs must be reconstructible from the aux/main interleave");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US1 / T072 acceptance scenario 4: Open Apple, Closed Apple, Shift
    //  status reads track the modifier-state setters across $C061-$C063.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Phase7_OpenClosedApple_Shift_StatusReads)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        BootToPrompt (host, core);

        Assert::AreEqual (Byte (0x00),
            static_cast<Byte> (core.bus->ReadByte (0xC061) & kBitSeven),
            L"$C061 bit 7 must read 0 with Open Apple released");
        Assert::AreEqual (Byte (0x00),
            static_cast<Byte> (core.bus->ReadByte (0xC062) & kBitSeven),
            L"$C062 bit 7 must read 0 with Closed Apple released");
        Assert::AreEqual (Byte (0x00),
            static_cast<Byte> (core.bus->ReadByte (0xC063) & kBitSeven),
            L"$C063 bit 7 must read 0 with Shift released");

        core.keyboard->SetOpenApple   (true);
        core.keyboard->SetClosedApple (true);
        core.keyboard->SetShift       (true);

        Assert::AreEqual (kBitSeven,
            static_cast<Byte> (core.bus->ReadByte (0xC061) & kBitSeven),
            L"$C061 bit 7 must read 1 with Open Apple pressed");
        Assert::AreEqual (kBitSeven,
            static_cast<Byte> (core.bus->ReadByte (0xC062) & kBitSeven),
            L"$C062 bit 7 must read 1 with Closed Apple pressed");
        Assert::AreEqual (kBitSeven,
            static_cast<Byte> (core.bus->ReadByte (0xC063) & kBitSeven),
            L"$C063 bit 7 must read 1 with Shift pressed");

        core.keyboard->SetOpenApple   (false);
        core.keyboard->SetClosedApple (false);
        core.keyboard->SetShift       (false);

        Assert::AreEqual (Byte (0x00),
            static_cast<Byte> (core.bus->ReadByte (0xC061) & kBitSeven));
        Assert::AreEqual (Byte (0x00),
            static_cast<Byte> (core.bus->ReadByte (0xC062) & kBitSeven));
        Assert::AreEqual (Byte (0x00),
            static_cast<Byte> (core.bus->ReadByte (0xC063) & kBitSeven));
    }
};

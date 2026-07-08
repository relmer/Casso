#include "Pch.h"
#include "HeadlessHost.h"
#include "TextScreenScraper.h"
#include "FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cBootTests
//
//  Spec 016 / US2. Cold-boots a full Apple //c (65C02 + 32K two-bank
//  firmware) through the real ROM 4 and verifies it reaches the Applesoft
//  `]` prompt -- the //c has no default boot disk, so a stock cold boot
//  drops to BASIC exactly like a //e with an empty drive.
//
//  The //c ROM is copyrighted and (per project policy) NOT committed: it is
//  provisioned on demand into UnitTest/Fixtures/Apple2c.rom. When that
//  fixture is absent (e.g. a clean CI checkout) the test SKIPS rather than
//  fails, so CI never depends on a machine ROM. The banking mechanism itself
//  is covered ROM-free by Apple2cRomBankTests.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr uint64_t   kColdBootCycles = 5'000'000ULL;
    static constexpr size_t     kRomSize        = 0x8000;      // 32K, two 16K banks

    // The //c ROM fixture is optional (uncommitted). Report presence so the
    // test can skip cleanly instead of failing on a clean checkout.
    bool Apple2cRomAvailable ()
    {
        FixtureProvider        fp;
        std::vector<uint8_t>   bytes;

        return SUCCEEDED (fp.OpenFixture ("Apple2c.rom", bytes)) && bytes.size () == kRomSize;
    }

    int FindRowContaining (const std::vector<std::string> & rows, const std::string & needle)
    {
        for (size_t i = 0; i < rows.size (); i++)
        {
            if (rows[i].find (needle) != std::string::npos)
            {
                return static_cast<int> (i);
            }
        }
        return -1;
    }
}


TEST_CLASS (Apple2cBootTests)
{
public:

    TEST_METHOD (ColdBootReaches_BASIC_Prompt)
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

        HRESULT   hr = host.BuildApple2c (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildApple2c must succeed when the ROM is present");
        Assert::IsTrue (core.HasApple2e (), L"//c wiring (65C02 + MMU) must be complete");

        core.PowerCycle ();
        core.RunCycles  (kColdBootCycles);

        std::vector<std::string>   rows = TextScreenScraper::Scrape (core);

        Assert::AreEqual (size_t (TextScreenScraper::kRows), rows.size (),
            L"Scraper must produce a full text screen");

        int   promptRow = FindRowContaining (rows, "]");

        Assert::IsTrue (promptRow >= 0,
            L"Applesoft `]` prompt must appear after //c cold boot");
    }
};

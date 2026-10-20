#include "Pch.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "Devices/RamDevice.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TextScreenScraperTests
//
//  The scraper reads the RAM the video scanner reads, not the bank the CPU's
//  switches map at the text page. The //e 80-column firmware sets PAGE2
//  around every even-column write, so with 80STORE on, a run can stop with
//  the bus serving aux at $0400 while the video scanner reads main there.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TextScreenScraperTests)
{
public:

    static constexpr Word  kSet80Store  = 0xC001;
    static constexpr Word  kSet80Col    = 0xC00D;
    static constexpr Word  kPage2On     = 0xC055;
    static constexpr Byte  kNormalGlyph = 0x80;

    static constexpr const char *  kText = "HELLO THERE";


    //  Writes `text` across the top row of text page 1 the way the 80-column
    //  firmware lays it out: even columns in aux, odd columns in main.
    static void Write80ColumnRow (TestMachine & machine, const char * text)
    {
        Byte  * mainRam = machine.GetRefs().mainRamDev->GetData();
        Byte  * auxRam  = machine.GetMmu()->GetAuxBuffer();
        size_t  length  = strlen (text);
        size_t  col;



        for (col = 0; col < length; col++)
        {
            Byte   * bank  = (col % 2 == 0) ? auxRam : mainRam;
            Word     addr  = static_cast<Word> (TextScreenScraper::kTextPage1 + col / 2);

            bank[addr] = static_cast<Byte> (text[col] | kNormalGlyph);
        }
    }


    TEST_METHOD (Scrape80_ReadsBothBanks_WhilePage2BanksTheCpuToAux)
    {
        TestMachine                machine ("Apple2e", TestMachine::Slots::Empty);
        MemoryBus                & bus     = machine.GetMemoryBus();
        std::vector<std::string>   rows;
        size_t                     length  = strlen (kText);



        machine.PowerCycle();
        Write80ColumnRow (machine, kText);

        bus.WriteByte (kSet80Col,   0);
        bus.WriteByte (kSet80Store, 0);
        bus.ReadByte  (kPage2On);

        Assert::AreEqual<Byte> (static_cast<Byte> ('H' | kNormalGlyph), bus.ReadByte (TextScreenScraper::kTextPage1),
            L"fixture: the bus serves aux at $0400, the bank with the even columns");

        rows = TextScreenScraper::Scrape (machine);

        Assert::AreEqual (size_t (TextScreenScraper::kCols80), rows[0].size(),
            L"an 80-column screen scrapes as 80-column rows");
        Assert::AreEqual (std::string (kText), rows[0].substr (0, length),
            L"the odd columns come from main, not from the bank PAGE2 maps for the CPU");
    }


    TEST_METHOD (Scrape40_ReadsMainPage1_While80StoreBanksTheCpuToAux)
    {
        TestMachine                machine ("Apple2e", TestMachine::Slots::Empty);
        MemoryBus                & bus     = machine.GetMemoryBus();
        Byte                     * mainRam = nullptr;
        Byte                     * auxRam  = nullptr;
        std::vector<std::string>   rows;
        size_t                     length  = strlen (kText);
        size_t                     col;



        machine.PowerCycle();
        mainRam = machine.GetRefs().mainRamDev->GetData();
        auxRam  = machine.GetMmu()->GetAuxBuffer();

        for (col = 0; col < length; col++)
        {
            mainRam[TextScreenScraper::kTextPage1 + col] = static_cast<Byte> (kText[col] | kNormalGlyph);
            auxRam [TextScreenScraper::kTextPage1 + col] = static_cast<Byte> (' ' | kNormalGlyph);
            mainRam[TextScreenScraper::kTextPage2 + col] = static_cast<Byte> (' ' | kNormalGlyph);
        }

        bus.WriteByte (kSet80Store, 0);
        bus.ReadByte  (kPage2On);

        rows = TextScreenScraper::Scrape (machine);

        Assert::AreEqual (size_t (TextScreenScraper::kCols40), rows[0].size(),
            L"with 80COL off the screen scrapes as 40-column rows");
        Assert::AreEqual (std::string (kText), rows[0].substr (0, length),
            L"under 80STORE, PAGE2 banks the CPU to aux and the display stays on main page 1");
    }
};

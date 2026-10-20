#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/Panes/SourcePane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatColumnTests
//
//  The heat columns' strip: a block per byte in the heat map's colors, r or
//  w for a byte read or written as data, and for a BASIC line's tokens a
//  read drawn as the line running.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatColumnTests)
    {
    public:

        static constexpr uint32_t  kCold    = 0xFF404040;
        static constexpr uint32_t  kExecute = 0xFF0000FF;
        static constexpr uint32_t  kRead    = 0xFF00FF00;
        static constexpr uint32_t  kWrite   = 0xFFFF0000;

        static HeatMapView::Palette MakePalette()
        {
            HeatMapView::Palette  palette;



            palette.cold    = kCold;
            palette.execute = kExecute;
            palette.read    = kRead;
            palette.write   = kWrite;
            return palette;
        }

        static DebuggerViewSnapshot::HeatColumnLevels MakeLevels()
        {
            DebuggerViewSnapshot::HeatColumnLevels  levels;



            levels.execute.assign (0x10000, 0);
            levels.read.assign    (0x10000, 0);
            levels.write.assign   (0x10000, 0);
            return levels;
        }



        TEST_METHOD (EachByteIsFilledWithWhatTouchedItMost)
        {
            DebuggerViewSnapshot::HeatColumnLevels  levels = MakeLevels();
            HeatStrip::Strip                        strip;



            levels.execute[0x0300] = 255;
            levels.read[0x0301]    = 255;
            levels.write[0x0302]   = 255;

            strip = HeatStrip::Get (levels, 0x0300, 4, MakePalette(), false);

            Assert::AreEqual (std::wstring (L"    "), strip.text, L"run, read, written, untouched, as fills");
            Assert::AreEqual (kExecute, strip.hottest, L"the first of the hottest");
            Assert::AreEqual ((size_t) 4, strip.colors.size());
            Assert::AreEqual (kExecute, std::get<2> (strip.colors[0]), L"the hottest is the hue itself");
            Assert::AreEqual (kRead,    std::get<2> (strip.colors[1]));
            Assert::AreEqual (kWrite,   std::get<2> (strip.colors[2]));
            Assert::AreEqual (kCold,    std::get<2> (strip.colors[3]), L"an untouched byte is the cold gray");

            //  Code written once when it was loaded, then run, is code.
            levels.write[0x0300] = 10;
            Assert::AreEqual (kExecute, std::get<2> (HeatStrip::Get (levels, 0x0300, 1, MakePalette(), false).colors.at (0)), L"run more than written");
            Assert::AreEqual (kExecute, HeatStrip::GetCell (levels, 0x0300, 1, MakePalette(), false).background, L"a cell's fill is the hottest");
        }



        TEST_METHOD (ALongRunIsFourBlocksEachItsHottest)
        {
            DebuggerViewSnapshot::HeatColumnLevels  levels = MakeLevels();
            HeatStrip::Strip                        strip;



            levels.write[0x2005] = 10;

            strip = HeatStrip::Get (levels, 0x2000, 16, MakePalette(), false);

            Assert::AreEqual ((size_t) HeatStrip::kMaxBlocks, strip.text.size());
            Assert::AreEqual (HeatMapView::GetColor (HeatMapView::Mode::Data, 0, 0, 10, MakePalette()), std::get<2> (strip.colors[1]), L"$2005 is in the second block of four bytes");
        }



        //  A source line's number has the strip of the first range it
        //  produced ahead of it, and a line with no code spaces, so the
        //  numbers stay in a column.
        TEST_METHOD (ASourceLinesNumberHasItsBytesStripAheadOfIt)
        {
            DebuggerViewSnapshot::HeatColumnLevels                levels = MakeLevels();
            std::vector<DxuiTextView::Row>                        rows   = SourcePane::BuildRows ({ L"  lda #1", L"; done" }, -1, {});
            std::map<std::pair<int, int>, std::pair<Word, Word>>  ranges = { { { 0, 1 }, { 0x0300, 0x0301 } } };



            levels.execute[0x0300] = 255;
            levels.read[0x0301]    = 255;

            SourcePane::AddHeatStrips (rows, 0, ranges, levels, MakePalette());

            Assert::AreEqual (std::wstring (L"     1"), rows[0].cells[1]);
            Assert::AreEqual (std::wstring (L"     2"), rows[1].cells[1]);
            Assert::IsTrue   (std::ranges::any_of (rows[0].spans, [] (const DxuiTextView::Span & span) { return span.cell == 1 && span.start == 1 && span.background == kRead; }));
        }



        //  A memory window's byte is tinted halfway from the page toward its
        //  heat map color, and a byte nothing touched is not tinted.
        TEST_METHOD (AMemoryBytesTintIsHalfwayToItsColor)
        {
            DebuggerViewSnapshot::HeatColumnLevels  levels  = MakeLevels();
            HeatMapView::Palette                    palette = MakePalette();



            palette.background   = 0xFF000000;
            levels.write[0x0400] = 255;

            Assert::AreEqual (DxuiColor::Mix (0xFF000000, kWrite, 0.5f) | 0xFF000000u, MemoryPane::GetHeatFill (levels, 0x0400, palette));
            Assert::AreEqual (0u, MemoryPane::GetHeatFill (levels, 0x0401, palette));
        }



        //  The interpreter reads a line's tokens as it runs it, so a read is a
        //  block in the code color, and only a write is a letter.
        TEST_METHOD (ATokensReadIsTheLineRunning)
        {
            DebuggerViewSnapshot::HeatColumnLevels  levels = MakeLevels();
            HeatStrip::Strip                        strip;



            levels.read[0x0801] = 255;

            strip = HeatStrip::Get (levels, 0x0801, 1, MakePalette(), true);

            Assert::AreEqual (std::wstring (L" "), strip.text);
            Assert::AreEqual (kExecute, std::get<2> (strip.colors[0]));
            Assert::AreEqual ((size_t) 0, HeatStrip::Get (DebuggerViewSnapshot::HeatColumnLevels(), 0x0801, 1, MakePalette(), true).text.size(),
                              L"no levels, no strip");
            Assert::AreEqual ((size_t) 3, HeatStrip::CountBytes ("BD 9A 1C"));
        }
    };
}

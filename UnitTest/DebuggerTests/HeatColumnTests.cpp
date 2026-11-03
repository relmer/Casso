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

        static inline const uint32_t  kRunTop   = HeatStrip::GetScaleStops (HeatStrip::Kind::Run)[2];
        static inline const uint32_t  kReadTop  = HeatStrip::GetScaleStops (HeatStrip::Kind::Read)[2];
        static inline const uint32_t  kWriteTop = HeatStrip::GetScaleStops (HeatStrip::Kind::Write)[2];

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
            Assert::AreEqual (kRunTop, strip.hottest, L"the first of the hottest");
            Assert::AreEqual ((size_t) 4, strip.colors.size());
            Assert::AreEqual (kRunTop,   std::get<2> (strip.colors[0]), L"the hottest is the top of its scale");
            Assert::AreEqual (kReadTop,  std::get<2> (strip.colors[1]));
            Assert::AreEqual (kWriteTop, std::get<2> (strip.colors[2]));
            Assert::AreEqual (kCold,     std::get<2> (strip.colors[3]), L"an untouched byte is the cold gray");

            //  Code written once when it was loaded, then run, is code.
            levels.write[0x0300] = 10;
            Assert::AreEqual (kRunTop, std::get<2> (HeatStrip::Get (levels, 0x0300, 1, MakePalette(), false).colors.at (0)), L"run more than written");
            Assert::AreEqual (kRunTop, HeatStrip::GetCell (levels, 0x0300, 1, MakePalette(), false).background, L"a cell's fill is the hottest");
        }



        TEST_METHOD (ALongRunIsFourBlocksEachItsHottest)
        {
            DebuggerViewSnapshot::HeatColumnLevels  levels = MakeLevels();
            HeatStrip::Strip                        strip;



            levels.write[0x2005] = 10;

            strip = HeatStrip::Get (levels, 0x2000, 16, MakePalette(), false);

            Assert::AreEqual ((size_t) HeatStrip::kMaxBlocks, strip.text.size());
            Assert::AreEqual (HeatStrip::GetScaleColor (HeatStrip::Kind::Write, 10, MakePalette()), std::get<2> (strip.colors[1]), L"$2005 is in the second block of four bytes");
        }



        //  A source line's number has a fill of its bytes' hottest color ahead
        //  of it, and a line with no code spaces, so the numbers stay in a
        //  column.
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
            uint32_t  hottest = HeatStrip::Get (levels, 0x0300, 2, MakePalette(), false).hottest;

            Assert::IsTrue   (std::ranges::any_of (rows[0].spans, [hottest] (const DxuiTextView::Span & span)
            {
                return span.cell == 1 && span.start == 0 && span.length == (int) HeatStrip::kMaxBlocks && span.background == hottest;
            }), L"one fill of the hottest byte's color, the same width on every line");
            Assert::IsFalse  (std::ranges::any_of (rows[1].spans, [] (const DxuiTextView::Span & span) { return span.background != 0; }), L"a line with no code has no fill");
        }



        //  While heat is on, a memory byte's tip has its counts and the scale
        //  of its tint, as a heat cell's does; with heat off, none.
        TEST_METHOD (AMemoryBytesTipExplainsItsTintWhileHeatIsOn)
        {
            auto                levels = std::make_shared<DebuggerViewSnapshot::HeatColumnLevels> (MakeLevels());
            DxuiHexView         view;
            MemoryPane          pane   (1, &view, [] (int, Word) {}, [] (const DebuggerActionBuilder &) {}, [] (const std::string &) {});
            HeatStrip::Tip      tip;



            levels->executeCount.assign (0x10000, 0);
            levels->readCount.assign    (0x10000, 0);
            levels->writeCount.assign   (0x10000, 0);

            pane.Configure (nullptr);
            pane.SetHeat   (levels, MakePalette());

            Assert::IsTrue (pane.GetHeatTip (0).text.empty(), L"heat is off");

            pane.SetHeatOn (true);
            tip = pane.GetHeatTip (0);

            Assert::IsTrue (tip.text.starts_with (L"Byte"), tip.text.c_str());
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



        //  Each kind has a separate scale: no color is on two scales, a scale
        //  starts at its first stop and ends at its last, and a hotter level
        //  is redder on the run scale and brighter on the others.
        TEST_METHOD (RunReadAndWriteHaveSeparateScales)
        {
            const HeatStrip::Kind  kinds[] = { HeatStrip::Kind::Run, HeatStrip::Kind::Read, HeatStrip::Kind::Write };
            std::set<uint32_t>     colors;
            auto                   bright  = [] (uint32_t argb) { return ((argb >> 16) & 0xFF) + ((argb >> 8) & 0xFF) + (argb & 0xFF); };
            auto                   green   = [] (uint32_t argb) { return (argb >> 8) & 0xFF; };



            for (HeatStrip::Kind kind : kinds)
            {
                std::array<uint32_t, 3>  stops = HeatStrip::GetScaleStops (kind);

                Assert::AreEqual (stops[0], HeatStrip::GetScaleColor (kind, 1,   MakePalette()));
                Assert::AreEqual (stops[2], HeatStrip::GetScaleColor (kind, 255, MakePalette()));
                Assert::AreEqual (kCold,    HeatStrip::GetScaleColor (kind, 0,   MakePalette()), L"an untouched byte");
                if (kind == HeatStrip::Kind::Run)
                {
                    Assert::IsTrue (green (HeatStrip::GetScaleColor (kind, 200, MakePalette())) < green (HeatStrip::GetScaleColor (kind, 20, MakePalette())), L"hotter is redder");
                }
                else
                {
                    Assert::IsTrue (bright (HeatStrip::GetScaleColor (kind, 200, MakePalette())) > bright (HeatStrip::GetScaleColor (kind, 20, MakePalette())));
                }

                colors.insert (stops.begin(), stops.end());
            }

            Assert::AreEqual ((size_t) 9, colors.size(), L"no stop is shared");
        }



        //  A heading's tip has a line for each scale with that scale's colors,
        //  under lines short enough for a tip that doesn't wrap; a BASIC
        //  program's has no read scale.
        TEST_METHOD (AHeadingsTipDrawsEachScale)
        {
            for (bool isTokens : { false, true })
            {
                HeatStrip::HeadingTip  tip   = HeatStrip::GetHeadingTip (isTokens);
                size_t                 lines = (size_t) std::ranges::count (tip.text, L'\n') + 1;
                size_t                 drawn = (size_t) std::ranges::count_if (tip.lineScales, [] (const std::vector<uint32_t> & stops) { return !stops.empty(); });



                Assert::AreEqual (lines, tip.lineScales.size(), L"an entry for every line");
                Assert::AreEqual (isTokens ? (size_t) 2 : (size_t) 3, drawn);
                Assert::AreEqual (HeatStrip::GetScaleStops (HeatStrip::Kind::Run)[2], tip.lineScales[lines - drawn].back(), L"the first scale is the run scale, ending at its hottest");
                Assert::IsTrue   (tip.text.find (L"gray") == std::wstring::npos, L"an untouched byte has no color");

                for (size_t from = 0, end = 0; from <= tip.text.size(); from = end + 1)
                {
                    end = std::min (tip.text.find (L'\n', from), tip.text.size());
                    Assert::IsTrue (end - from <= 64, tip.text.substr (from, end - from).c_str());
                }
            }
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
            Assert::AreEqual (kRunTop, std::get<2> (strip.colors[0]));
            Assert::AreEqual ((size_t) 0, HeatStrip::Get (DebuggerViewSnapshot::HeatColumnLevels(), 0x0801, 1, MakePalette(), true).text.size(),
                              L"no levels, no strip");
            Assert::AreEqual ((size_t) 3, HeatStrip::CountBytes ("BD 9A 1C"));
        }



        //  A heat cell's tip lists each byte's counts, then what its color
        //  means, with the scale drawn under it and a mark at the cell's level.
        TEST_METHOD (AHeatCellsTipHasEachBytesCountsAndItsScale)
        {
            DebuggerViewSnapshot::HeatColumnLevels  levels = MakeLevels();
            HeatStrip::Tip                          tip;
            DxuiListView::Cell                      cell;



            levels.executeCount.assign (0x10000, 0);
            levels.readCount.assign    (0x10000, 0);
            levels.writeCount.assign   (0x10000, 0);
            levels.execute[0x0300]      = 255;
            levels.executeCount[0x0300] = 12345;
            levels.readCount[0x0301]    = 7;

            tip = HeatStrip::GetTip (levels, 0x0300, 2, false);

            Assert::IsTrue   (tip.text.starts_with (L"Byte\tRun\tRead\tWritten\n$0300\t12,345\t0\t0\n$0301\t0\t7\t0\n\n"), tip.text.c_str());
            Assert::IsTrue   (tip.text.find (HeatStrip::GetScaleName (HeatStrip::Kind::Run)) != std::wstring::npos);
            Assert::AreEqual (1.0f, tip.mark);
            Assert::AreEqual ((size_t) 3, tip.gradient.size());
            Assert::AreEqual (HeatStrip::GetScaleStops (HeatStrip::Kind::Run)[0], tip.gradient[0]);

            cell = HeatStrip::GetCell (levels, 0x0300, 2, MakePalette(), false);
            Assert::AreEqual (tip.text, cell.tip, L"a list cell has the same tip");
            Assert::AreEqual (tip.mark, cell.tipGradientMark);

            tip = HeatStrip::GetTip (levels, 0x0300, HeatStrip::kMaxTipBytes + 1, false);
            Assert::IsTrue   (tip.text.find (L"$0300-$0308\t12,345\t7\t0") != std::wstring::npos, L"a long line has the sums");

            tip = HeatStrip::GetTip (levels, 0x0400, 1, false);
            Assert::IsTrue   (tip.gradient.empty(), L"an untouched byte has no scale");
            Assert::IsTrue   (HeatStrip::GetTip (MakeLevels(), 0x0300, 1, false).text.empty(), L"no counts, no tip");
        }



        //  The tip's scale runs through its stops evenly from left to right.
        TEST_METHOD (TheTipsScaleRunsThroughItsStops)
        {
            std::vector<uint32_t>  stops = { 0xFF000000, 0xFF808080, 0xFFFFFFFF };



            Assert::AreEqual (stops[0], DxuiTooltip::GetGradientColor (stops, 0.0f));
            Assert::AreEqual (stops[1], DxuiTooltip::GetGradientColor (stops, 0.5f));
            Assert::AreEqual (stops[2], DxuiTooltip::GetGradientColor (stops, 1.0f));
            Assert::AreEqual (stops[2], DxuiTooltip::GetGradientColor (stops, 2.0f), L"past the end is the last stop");
            Assert::AreEqual (0u,       DxuiTooltip::GetGradientColor ({}, 0.5f));
        }
    };
}
#include "Pch.h"

#include "Ui/Debugger/HistoryBand.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryBandLayoutTests
//
//  The history band's Go live link kept whole and inset from the pane's
//  right edge, the text shortened first when the pane is narrow, the band
//  one row tall, and the disks with writes not yet saved put in words.
//  Painted at 96 DPI into recording mocks, whose renderer measures a
//  character as 7 pixels wide.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HistoryBandLayoutTests)
    {
    public:

        static constexpr float  kMockCharDip = 7.0f;
        static constexpr float  kPad         = 4.0f;
        static constexpr int    kRowDip      = 16;



        static DxuiDpiScaler GetScaler96()
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            return scaler;
        }


        static HistoryStatus MakeBehind (int unsavedDisks)
        {
            HistoryStatus  status;



            status.isRecording        = true;
            status.isBehindLive       = true;
            status.instructionsBehind = 3;
            status.cyclesBehind       = 9;
            status.unsavedDisks       = unsavedDisks;
            return status;
        }


        static const RecordedTextCall * FindText (const MockDxuiTextRenderer & text, const std::wstring & string)
        {
            auto  found = std::find_if (text.Calls().begin(), text.Calls().end(), [&] (const RecordedTextCall & call)
            {
                return call.kind == RecordedTextKind::DrawString && call.text == string;
            });



            return (found != text.Calls().end()) ? &*found : nullptr;
        }


        static void PaintBand (HistoryBand & band, MockDxuiTextRenderer & text, LONG width)
        {
            MockDxuiPainter  painter;
            MockDxuiTheme    theme;



            band.Layout (RECT { 0, 0, width, 18 }, GetScaler96());
            band.Paint  (painter, text, theme);
        }



        //  The link is drawn at the weight it was measured at, right-aligned
        //  in a box that ends the pane's padding short of its right edge, so
        //  no part of it can run past that edge.
        TEST_METHOD (TheLinkEndsAPadInsideTheRightEdge)
        {
            constexpr LONG               kWidth = 600;
            HistoryBand                  band;
            MockDxuiTextRenderer         text;
            const RecordedTextCall     * link   = nullptr;



            band.SetStatus (MakeBehind (0));
            PaintBand (band, text, kWidth);

            link = FindText (text, HistoryBand::kGoLiveText);

            Assert::IsNotNull (link, L"the link is drawn");
            Assert::AreEqual  ((float) kWidth - kPad, link->x + link->width, 0.5f, L"its box ends a pad inside the edge");
            Assert::IsTrue    (link->hAlign == DxuiTextHAlign::Right, L"and it is set against that end");
            Assert::IsTrue    (link->weight == DxuiFontWeight::Normal, L"at the weight it was measured at, so it fits its box");
        }


        //  A pane too narrow for the full text takes the compact text, then
        //  the short one; the link stays whole and in place throughout.
        TEST_METHOD (ANarrowPaneShortensTheTextNotTheLink)
        {
            constexpr LONG               kWidth  = 160;
            HistoryBand                  band;
            MockDxuiTextRenderer         text;
            HistoryStatus                status  = MakeBehind (2);
            const RecordedTextCall     * link    = nullptr;
            const RecordedTextCall     * message = nullptr;



            band.SetStatus (status);
            PaintBand (band, text, kWidth);

            link    = FindText (text, HistoryBand::kGoLiveText);
            message = FindText (text, HistoryBand::GetShortText (status));

            Assert::IsNotNull (link,    L"the link is drawn whole");
            Assert::IsNotNull (message, L"the text is the short one");
            Assert::AreEqual  ((float) kWidth - kPad, link->x + link->width, 0.5f, L"the link has not moved");
            Assert::AreEqual  (std::wcslen (HistoryBand::kGoLiveText) * kMockCharDip, link->width, 0.5f, L"the link's box is its whole width");
            Assert::IsTrue    (message->x + message->width <= link->x - kPad + 0.5f, L"the text stops a pad before the link");
        }


        TEST_METHOD (PlacingPicksTheLongestTextThatFits)
        {
            HistoryBand::Placement  wide   = HistoryBand::Place (0.0f, 500.0f, kPad, 50.0f, { 300.0f, 200.0f, 80.0f });
            HistoryBand::Placement  middle = HistoryBand::Place (0.0f, 320.0f, kPad, 50.0f, { 300.0f, 200.0f, 80.0f });
            HistoryBand::Placement  narrow = HistoryBand::Place (0.0f, 100.0f, kPad, 50.0f, { 300.0f, 200.0f, 80.0f });



            Assert::AreEqual<size_t> (0, wide.textIndex,   L"room for the full text");
            Assert::AreEqual<size_t> (1, middle.textIndex, L"the compact text");
            Assert::AreEqual<size_t> (2, narrow.textIndex, L"the short text when nothing fits");
            Assert::AreEqual (500.0f - kPad - 50.0f, wide.linkLeft,  L"the link a pad inside the right edge");
            Assert::AreEqual (wide.linkLeft - kPad,  wide.textRight, L"the text a pad before the link");
        }


        //  One row and its edge: more pushes the registers out of a short
        //  pane.
        TEST_METHOD (TheBandIsOneRowTall)
        {
            Assert::IsTrue (HistoryBand::GetHeightDip (kRowDip) <= kRowDip + 2, L"no taller than a row and its edge");
            Assert::IsTrue (HistoryBand::GetHeightDip (kRowDip) >= kRowDip,     L"and tall enough for the row");
        }


        TEST_METHOD (UnsavedDisksArePutInWords)
        {
            HistoryStatus  none = MakeBehind (0);
            HistoryStatus  one  = MakeBehind (1);
            HistoryStatus  two  = MakeBehind (2);



            Assert::IsTrue (HistoryBand::GetText (none).find (L"saved") == std::wstring::npos, L"nothing said with nothing unsaved");
            Assert::IsTrue (HistoryBand::GetText (one).ends_with (L"1 disk with writes not saved."), HistoryBand::GetText (one).c_str());
            Assert::IsTrue (HistoryBand::GetText (two).ends_with (L"2 disks with writes not saved."), HistoryBand::GetText (two).c_str());
            Assert::IsTrue (HistoryBand::GetCompactText (two).ends_with (L"2 disks with writes not saved."), L"the compact text keeps it");
        }
    };
}

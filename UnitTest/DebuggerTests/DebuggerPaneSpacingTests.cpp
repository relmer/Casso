#include "Pch.h"

#include "Debugger/IDiagnosticsProvider.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Widgets/DxuiTextView.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneSpacingTests
//
//  Room around and between what the panes draw: the console's line spacing
//  and the gap between commands, the inset that keeps a device graphic off
//  the pane's edge, a meter's text box, the call stack's columns fitting the
//  frames shown, and a switch's value. Painted at 96 DPI into recording
//  mocks, whose renderer measures a line as 16 pixels tall.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebuggerPaneSpacingTests)
    {
    public:

        static constexpr float  kMockLineDip = 16.0f;



        static DxuiDpiScaler GetScaler96()
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            return scaler;
        }


        static const RecordedTextCall * FindText (const MockDxuiTextRenderer & text, const std::wstring & string)
        {
            auto  found = std::find_if (text.Calls().begin(), text.Calls().end(), [&] (const RecordedTextCall & call)
            {
                return call.kind == RecordedTextKind::DrawString && call.text == string;
            });



            return (found != text.Calls().end()) ? &*found : nullptr;
        }


        static CallStackData MakeFrames (const std::string & symbol)
        {
            CallStackData   data;
            CallStackFrame  frame;
            CallStackRow    row;



            frame.callSite = 0x0803;
            frame.target   = 0x0900;
            frame.symbol   = symbol;
            row.frame      = frame;

            data.rows.push_back (row);
            return data;
        }



        //  Line spacing puts the extra between lines rather than leaving the
        //  text set solid.
        TEST_METHOD (LineSpacingSpreadsTheConsoleLines)
        {
            DxuiTextView               view;
            MockDxuiPainter            painter;
            MockDxuiTextRenderer       text;
            MockDxuiTheme              theme;
            DxuiTextView::Row          first;
            DxuiTextView::Row          second;
            const RecordedTextCall   * a = nullptr;
            const RecordedTextCall   * b = nullptr;



            first.cells  = { L"alpha" };
            second.cells = { L"beta" };

            view.SetLineSpacing (1.5f);
            view.Layout         (RECT { 0, 0, 400, 300 }, GetScaler96());
            view.SetRows        ({ first, second });
            view.Paint          (painter, text, theme);
            text.Reset();
            view.Paint          (painter, text, theme);

            a = FindText (text, L"alpha");
            b = FindText (text, L"beta");

            Assert::IsNotNull (a);
            Assert::IsNotNull (b);
            Assert::AreEqual  (kMockLineDip * 1.5f, b->y - a->y, L"a line is half again the face's height");
        }


        //  Each command starts with a blank line, so it stands apart from the
        //  output of the one before; nothing to show gets nothing.
        TEST_METHOD (ACommandStartsWithAGap)
        {
            std::vector<std::string>  lines = { "> R", "A=00" };
            std::vector<std::string>  none;



            DebuggerViewState::AddCommandGap (lines);
            DebuggerViewState::AddCommandGap (none);

            Assert::AreEqual ((size_t) 3, lines.size());
            Assert::AreEqual (std::string(),      lines[0]);
            Assert::AreEqual (std::string ("> R"), lines[1]);
            Assert::IsTrue   (none.empty());
        }


        //  A graphic with an inset is kept off the pane's left, right and top
        //  edges, and the room it needs is added to its height.
        TEST_METHOD (AnInsetPartIsKeptOffThePaneEdges)
        {
            constexpr int      kInset  = 6;
            constexpr int      kHeight = 20;
            DebuggerPaneFrame  frame   (L"Pane");
            DxuiTextView       graphic;
            DxuiTextView       list;
            RECT               placed  = {};



            frame.AddPart         (&graphic, [] (int, const DxuiDpiScaler &) { return kHeight; });
            frame.AddPart         (&list);
            frame.SetPartInsetDip (&graphic, kInset);
            frame.Layout          (RECT { 0, 0, 200, 300 }, GetScaler96());

            placed = graphic.GetBounds();

            Assert::AreEqual ((LONG) kInset,             placed.left);
            Assert::AreEqual ((LONG) kInset,             placed.top);
            Assert::AreEqual ((LONG) (200 - kInset),     placed.right);
            Assert::AreEqual ((LONG) (kInset + kHeight), placed.bottom);
            Assert::AreEqual ((LONG) 0,                  list.GetBounds().left, L"a part without one keeps the edge");
        }


        //  A meter's status text gets a box a full line tall, so a descender
        //  is not clipped at the foot of the row.
        TEST_METHOD (AMeterStatusHasRoomForDescenders)
        {
            MeterBar                  bar;
            DiagnosticsMeters         meters;
            MockDxuiPainter           painter;
            MockDxuiTextRenderer      text;
            MockDxuiTheme             theme;
            const RecordedTextCall  * running = nullptr;



            meters.levels = { { "Timer 1", 0.0f, "running" } };
            bar.SetMeters (meters);
            bar.Layout    (RECT { 0, 0, 300, 100 }, GetScaler96());
            bar.Paint     (painter, text, theme);

            running = FindText (text, L"running");

            Assert::IsNotNull (running);
            Assert::IsTrue    (running->height >= kMockLineDip, L"the box is at least a line tall");
            Assert::AreEqual  (((float) MeterBar::kRowDip - running->height) * 0.5f, running->y, L"and centered on the row");
        }


        //  New frames fit the columns again, narrower as well as wider.
        TEST_METHOD (CallStackColumnsFitTheFramesShown)
        {
            DxuiListView          list;
            DxuiButton            button;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            CallStackPane         pane (&list, &button, [] (const DebuggerActionBuilder &) {}, [] (Word) {});
            int                   wide   = 0;
            int                   narrow = 0;



            pane.Configure();
            list.SetPreciseAutoFit (true);
            list.SetRefitOnSetRows (true);
            list.Layout            (RECT { 0, 0, 2000, 300 }, GetScaler96());

            pane.Apply (MakeFrames ("A_VERY_LONG_ROUTINE_NAME_INDEED"));
            list.Paint (painter, text, theme);
            wide = list.GetTotalMeasuredWidthPx();

            pane.Apply (MakeFrames ("SHORT"));
            list.Paint (painter, text, theme);
            narrow = list.GetTotalMeasuredWidthPx();

            Assert::IsTrue (narrow < wide, L"the routine column shrinks to the shorter name");
        }


        //  A switch reads ON at full strength and off dimmed; any other value
        //  is shown as published.
        TEST_METHOD (ASwitchShowsOnUppercaseAndOffDimmed)
        {
            DiagnosticsRow      on   = IDiagnosticsProvider::MakeFlagRow ("RAMRD", true);
            DiagnosticsRow      off  = IDiagnosticsProvider::MakeFlagRow ("RAMWRT", false);
            DiagnosticsRow      text = IDiagnosticsProvider::MakeTextRow ("Mode", "on");
            DxuiListView::Cell  cell;



            cell = DiagnosticsPane::MakeValueCell (on);
            Assert::AreEqual (std::wstring (L"ON"), cell.text);
            Assert::IsFalse  (cell.dim);

            cell = DiagnosticsPane::MakeValueCell (off);
            Assert::AreEqual (std::wstring (L"off"), cell.text);
            Assert::IsTrue   (cell.dim);

            cell = DiagnosticsPane::MakeValueCell (text);
            Assert::AreEqual (std::wstring (L"on"), cell.text);
            Assert::IsFalse  (cell.dim);
        }
    };
}






#include "Pch.h"

#include "Debugger/IDiagnosticsProvider.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/KeyHintLine.h"
#include "Ui/Debugger/MemoryAddressEntry.h"
#include "Ui/Debugger/ToolbarLabelEntry.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/Panes/MeterBar.h"
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
//  the pane's edge, the text inset every pane's text starts at, a meter's
//  text box, the call stack's columns fitting the frames shown, and a
//  switch's value. Painted at 96 DPI, or at each of kDpis where the text
//  inset is checked, into recording mocks, whose renderer measures a line as
//  16 pixels tall.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebuggerPaneSpacingTests)
    {
    public:

        static constexpr float  kMockLineDip = 16.0f;

        //  The DPIs the text inset is checked at. 106 is where the inset in
        //  pixels is not the sum of its parts converted one at a time.
        static constexpr int    kDpis[]      = { 96, 106, 120, 144, 168 };



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


        static std::shared_ptr<DxuiCommand> MakeCommand (int id, const wchar_t * label, const wchar_t * glyph)
        {
            auto  command = std::make_shared<DxuiCommand>();



            command->id    = id;
            command->label = label;
            command->glyph = glyph;
            return command;
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
            Assert::AreEqual (std::string (DebuggerViewState::kCommandGap), lines[0]);
            Assert::AreEqual (std::string ("> R"),                          lines[1]);
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


        //  A text-aligned part's sides move in to the pane's text inset, less
        //  the room the part keeps ahead of its own text; its top inset and
        //  its height are what they were.
        TEST_METHOD (ATextAlignedPartKeepsItsTopAndHeight)
        {
            constexpr int   kInset    = 6;
            constexpr int   kHeight   = 20;
            constexpr int   kInnerDip = 6;
            constexpr LONG  kWidth    = 200;



            for (int dpi : kDpis)
            {
                DebuggerPaneFrame  plainFrame   (L"Pane");
                DebuggerPaneFrame  alignedFrame (L"Pane");
                DxuiTextView       plainPart;
                DxuiTextView       alignedPart;
                DxuiTextView       plainList;
                DxuiTextView       alignedList;
                DxuiDpiScaler      scaler;
                RECT               plain        = {};
                RECT               aligned      = {};
                LONG               side         = 0;
                std::wstring       at           = std::format (L"at {} DPI", dpi);



                scaler.SetDpi (dpi);
                side = (LONG) std::max (0, DxuiPaneMetrics::GetContentTextInsetPx (scaler) - scaler.ToPx (kInnerDip));

                for (auto [frame, part, list] : { std::tuple { &plainFrame, &plainPart, &plainList }, std::tuple { &alignedFrame, &alignedPart, &alignedList } })
                {
                    frame->AddPart         (part, [] (int, const DxuiDpiScaler & each) { return each.ToPx (kHeight); });
                    frame->AddPart         (list);
                    frame->SetPartInsetDip (part, kInset);
                }

                alignedFrame.SetPartTextAligned (&alignedPart, kInnerDip);

                plainFrame.Layout   (RECT { 0, 0, kWidth, 300 }, scaler);
                alignedFrame.Layout (RECT { 0, 0, kWidth, 300 }, scaler);

                plain   = plainPart.GetBounds();
                aligned = alignedPart.GetBounds();

                Assert::AreEqual (side,                         aligned.left,                      (L"its left is the inset less its own pad " + at).c_str());
                Assert::AreEqual (kWidth - side,                aligned.right,                     (L"and its right the same in from the edge " + at).c_str());
                Assert::AreEqual (plain.top,                    aligned.top,                       (L"its top inset is unchanged " + at).c_str());
                Assert::AreEqual (plain.bottom - plain.top,     aligned.bottom - aligned.top,      (L"and so is its height " + at).c_str());
                Assert::AreEqual (plainList.GetBounds().top,    alignedList.GetBounds().top,       (L"the part after it is where it was " + at).c_str());
                Assert::AreEqual ((LONG) scaler.ToPx (kInset),  plain.left,                        (L"a part with only an inset keeps it " + at).c_str());
            }
        }


        //  The gap between one part and the next is in DIPs, like the parts.
        TEST_METHOD (ThePartsGapScalesWithTheDpi)
        {
            constexpr int  kHeight = 20;



            for (int dpi : kDpis)
            {
                DebuggerPaneFrame  frame (L"Pane");
                DxuiTextView       top;
                DxuiTextView       rest;
                DxuiDpiScaler      scaler;
                std::wstring       at    = std::format (L"at {} DPI", dpi);



                scaler.SetDpi (dpi);

                frame.AddPart (&top, [] (int, const DxuiDpiScaler &) { return kHeight; });
                frame.AddPart (&rest);
                frame.Layout  (RECT { 0, 0, 200, 300 }, scaler);

                Assert::AreEqual ((LONG) scaler.ToPx (DebuggerPaneFrame::kGapDip), rest.GetBounds().top - top.GetBounds().bottom, at.c_str());
            }
        }


        //  Every kind of text a pane starts with -- a dense list's first
        //  column, a pane toolbar's first label or icon, a key hint in a
        //  text-aligned part and a history band -- starts the same distance
        //  from the pane's outer edge as the pane's title, its body being one
        //  outline width inside that edge.
        TEST_METHOD (EveryPaneTextStartsOnTheInset)
        {
            constexpr LONG  kPaneLeft   = 40;
            constexpr LONG  kPaneTop    = 30;
            constexpr LONG  kPaneWidth  = 400;
            constexpr LONG  kPaneHeight = 300;
            constexpr int   kPanePadDip = 4;    // a debugger pane list's cell padding
            constexpr int   kHintDip    = 20;



            for (int dpi : kDpis)
            {
                DxuiDpiScaler                    scaler;
                MockDxuiPainter                  painter;
                MockDxuiTextRenderer             text;
                MockDxuiTextRenderer             bandText;
                MockDxuiTheme                    theme;
                DxuiListView                     list;
                DxuiToolbar                      labelBar;
                DxuiToolbar                      iconBar;
                std::vector<DxuiToolbar::Entry>  labelFirst (2);
                std::vector<DxuiToolbar::Entry>  iconFirst  (2);
                DebuggerPaneFrame                traceFrame (L"Trace");
                KeyHintLine                      hint;
                DxuiListView                     traceList;
                HistoryBand                      band;
                HistoryStatus                    status;
                RECT                             body       = {};
                RECT                             strip      = {};
                float                            title      = 0.0f;
                const RecordedTextCall         * found      = nullptr;
                std::wstring                     at         = std::format (L"at {} DPI", dpi);



                scaler.SetDpi (dpi);

                body  = RECT { kPaneLeft + DxuiPaneMetrics::GetLinePx (scaler), kPaneTop,
                               kPaneLeft + kPaneWidth - DxuiPaneMetrics::GetLinePx (scaler), kPaneTop + kPaneHeight };
                strip = RECT { body.left, body.top, body.right, body.top + scaler.ToPx (DxuiToolbar::kCompactBandDp) };
                title = (float) (kPaneLeft + DxuiPaneMetrics::GetTextInsetPx (scaler));

                //  A dense list.
                list.SetColumns        ({ DxuiListView::Column { L"Name", 0 } });
                list.SetShowHeader     (true);
                list.SetCellPaddingDip (kPanePadDip, kPanePadDip);
                list.SetPaneTextInset  (true);
                list.SetRows           ({ { DxuiListView::Cell { L"ABCD", false } } });
                list.Layout            (body, scaler);
                list.Paint             (painter, text, theme);

                //  A compact toolbar whose first entry is a label, and one whose
                //  first entry is an icon.
                labelFirst[0].command  = MakeCommand (1, L"Show", nullptr);
                labelFirst[0].kind     = DxuiToolbar::Kind::DropDown;
                labelFirst[1].command  = MakeCommand (2, L"Next", L"b");
                labelFirst[1].iconOnly = true;
                iconFirst[0].command   = MakeCommand (1, L"First", L"a");
                iconFirst[0].iconOnly  = true;
                iconFirst[1].command   = MakeCommand (2, L"Next", L"b");
                iconFirst[1].iconOnly  = true;

                for (auto [bar, entries] : { std::pair { &labelBar, &labelFirst }, std::pair { &iconBar, &iconFirst } })
                {
                    bar->SetCompact       (true);
                    bar->SetPaneTextInset (true);
                    bar->SetEntries       (std::move (*entries));
                    bar->Layout           (strip, scaler);
                    bar->Paint            (painter, text, theme);
                }

                //  The trace pane's key hint over its list.
                hint.SetPairs ({ { L"Space", L"step into" } });
                traceFrame.AddPart            (&hint, [] (int, const DxuiDpiScaler & each) { return each.ToPx (kHintDip); });
                traceFrame.AddPart            (&traceList);
                traceFrame.SetPartTextAligned (&hint);
                traceFrame.Layout             (body, scaler);
                hint.Paint                    (painter, text, theme);

                //  A history band, behind live.
                status.isRecording        = true;
                status.isBehindLive       = true;
                status.instructionsBehind = 3;
                status.cyclesBehind       = 9;
                band.SetStatus (status);
                band.Layout    (strip, scaler);
                band.Paint     (painter, bandText, theme);

                for (const wchar_t * first : { L"Name", L"ABCD", L"Show", L"Space" })
                {
                    found = FindText (text, first);

                    Assert::IsNotNull (found, (std::wstring (first) + L" is drawn " + at).c_str());
                    Assert::AreEqual  (title, found->x, (std::wstring (first) + L" starts where the title does " + at).c_str());
                }

                found = FindText (text, L"a");

                Assert::IsNotNull (found, at.c_str());
                Assert::AreEqual  (title, found->x, 0.5f, (L"the first icon starts where the title does " + at).c_str());

                //  The band draws its text, whichever length fits, and the link.
                found = nullptr;

                for (const RecordedTextCall & call : bandText.Calls())
                {
                    if (found == nullptr && call.kind == RecordedTextKind::DrawString && call.text != HistoryBand::kGoLiveText)
                    {
                        found = &call;
                    }
                }

                Assert::IsNotNull (found, at.c_str());
                Assert::AreEqual  (title, found->x, (L"the history band's text starts where the title does " + at).c_str());
            }
        }


        //  A pane strip that starts with a label, as the console's does, or
        //  with a text box, as a memory window's does, puts the label's ink
        //  and the box's text, rather than its border, exactly where the
        //  title starts, at every scale: the box moves back toward the pane's
        //  edge by its own padding.
        TEST_METHOD (ALabelOrATextBoxFirstOnAStripStartsOnTheInset)
        {
            constexpr LONG  kPaneLeft  = 40;
            constexpr LONG  kPaneTop   = 30;
            constexpr LONG  kPaneWidth = 400;



            for (int dpi : kDpis)
            {
                DxuiDpiScaler                    scaler;
                MockDxuiPainter                  painter;
                MockDxuiTextRenderer             text;
                MockDxuiTheme                    theme;
                ToolbarLabelEntry                mode (L"Mode:");
                DxuiTextInput                    box;
                MemoryAddressEntry               address (&box);
                DxuiToolbar                      labelBar;
                DxuiToolbar                      boxBar;
                std::vector<DxuiToolbar::Entry>  labelFirst (2);
                std::vector<DxuiToolbar::Entry>  boxFirst   (2);
                RECT                             strip      = {};
                float                            title      = 0.0f;
                const RecordedTextCall         * found      = nullptr;
                std::wstring                     at         = std::format (L"at {} DPI", dpi);



                scaler.SetDpi (dpi);

                strip = RECT { kPaneLeft + DxuiPaneMetrics::GetLinePx (scaler), kPaneTop,
                               kPaneLeft + kPaneWidth - DxuiPaneMetrics::GetLinePx (scaler), kPaneTop + scaler.ToPx (DxuiToolbar::kCompactBandDp) };
                title = (float) (kPaneLeft + DxuiPaneMetrics::GetTextInsetPx (scaler));

                labelFirst[0].command  = mode.GetCommand();
                labelFirst[0].custom   = &mode;
                labelFirst[1].command  = MakeCommand (2, L"Next", L"b");
                labelFirst[1].iconOnly = true;
                boxFirst[0].command    = MakeCommand (1, L"Address", nullptr);
                boxFirst[0].custom     = &address;
                boxFirst[1].command    = MakeCommand (2, L"Next", L"b");
                boxFirst[1].iconOnly   = true;

                for (auto [bar, entries] : { std::pair { &labelBar, &labelFirst }, std::pair { &boxBar, &boxFirst } })
                {
                    bar->SetCompact       (true);
                    bar->SetPaneTextInset (true);
                    bar->SetEntries       (std::move (*entries));
                    bar->Layout           (strip, scaler);
                    bar->Paint            (painter, text, theme);
                }

                box.SetText (L"0300");
                box.Paint   (painter, text, theme);

                found = FindText (text, L"Mode:");

                Assert::IsNotNull (found, at.c_str());
                Assert::AreEqual  (title, found->x, (L"the label starts where the title does " + at).c_str());
                Assert::AreEqual  ((LONG) title - scaler.ToPx (DxuiTextInput::kPadLeftDip), box.GetBounds().left,
                                   (L"the box starts its own padding short of it " + at).c_str());

                found = FindText (text, L"0300");

                Assert::IsNotNull (found, at.c_str());
                Assert::AreEqual  (title, found->x, (L"so the box's text starts where the title does " + at).c_str());
            }
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


        //  Where recording began is a note across the whole row: the columns
        //  fit the frames alone, as though the note were not there.
        TEST_METHOD (CallStackNoteLeavesTheColumnsToTheFrames)
        {
            DxuiListView          list;
            DxuiButton            button;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            CallStackPane         pane (&list, &button, [] (const DebuggerActionBuilder &) {}, [] (Word) {});
            CallStackData         noted = MakeFrames ("SHORT");
            CallStackRow          began;
            int                   plain = 0;



            began.chainBreak = CallStackBreak { CallBreakKind::TrackingBegan, 0x0803, 0 };
            noted.rows.push_back (began);

            pane.Configure();
            list.SetPreciseAutoFit (true);
            list.SetRefitOnSetRows (true);
            list.Layout            (RECT { 0, 0, 2000, 300 }, GetScaler96());

            pane.Apply (MakeFrames ("SHORT"));
            list.Paint (painter, text, theme);
            plain = list.GetTotalMeasuredWidthPx();

            pane.Apply (noted);
            list.Paint (painter, text, theme);

            Assert::AreEqual (2, list.GetRowCount(), L"the note is still a row");
            Assert::AreEqual (plain, list.GetTotalMeasuredWidthPx(), L"the note widens no column");
        }


        //  The note is one dimmed cell spanning the row, in a sentence of its
        //  own; every other break stays a separator in the columns.
        TEST_METHOD (CallStackNoteIsOneDimmedSpanningCell)
        {
            CallStackData                     data;
            CallStackRow                      txs;
            CallStackRow                      began;
            std::vector<CallStackPane::Row>   rows;
            std::vector<DxuiListView::Cell>   cells;
            DebuggerTextColors::Set           colors;



            txs.chainBreak   = CallStackBreak { CallBreakKind::Txs,           0x0910, 0x9A };
            began.chainBreak = CallStackBreak { CallBreakKind::TrackingBegan, 0x0803, 0 };
            data.rows.push_back (txs);
            data.rows.push_back (began);

            rows = CallStackPane::GetRows (data);

            Assert::AreEqual ((size_t) 2, rows.size());
            Assert::IsFalse  (rows[0].isNote, L"a TXS is a separator in the columns");
            Assert::IsTrue   (rows[1].isNote, L"where recording began is a note");
            Assert::AreEqual (std::wstring (L"Earlier calls weren't recorded (debugger opened at $0803)"), rows[1].routine);
            Assert::AreEqual ((Word) 0x0803, rows[1].address, L"activating it still shows where the debugger opened");

            cells = CallStackPane::GetCells (rows[1], colors);

            Assert::AreEqual ((size_t) 1, cells.size(), L"one cell for the whole row");
            Assert::AreEqual (rows[1].routine, cells[0].text);
            Assert::IsTrue   (cells[0].spansRow, L"across every column");
            Assert::IsTrue   (cells[0].dim,      L"in the muted color");

            cells = CallStackPane::GetCells (rows[0], colors);

            Assert::AreEqual ((size_t) 2, cells.size(), L"a separator is its address and what broke the chain");
            Assert::IsTrue   (cells[1].spansRow, L"which runs across the row and widens no column");
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






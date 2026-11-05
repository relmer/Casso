#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Core/TextEncoding.h"
#include "Debugger/CommandModeNames.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/ConsoleLinks.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/KeyHintLine.h"
#include "Ui/Debugger/Panes/TracePane.h"
#include "Ui/Debugger/WholeWordButton.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

#include "CppUnitTest.h"
#include "Ui/Debugger/ToolbarLabelEntry.h"
#include "Ui/Debugger/Panes/WatchesPane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerReviewPolishTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PolishHost
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PolishHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}

        void  RunDebuggerCommandInMode (const std::string &, CommandMode)        override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PolishWindow
    //
    //  The debugger window with no HWND, driven by the keys as its message
    //  handling drives it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PolishWindow : public DebuggerWindow
    {
    public:
        PolishWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::AppendConsole;
        using DebuggerWindow::GetConsolePane;
        using DebuggerWindow::GetFindWidgets;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetModeChoices;
        using DebuggerWindow::GetModeLabel;
        using DebuggerWindow::ModeChoice;
        using DebuggerWindow::GetWatchesPane;
        using DebuggerWindow::TakeSnapshot;

        void  Build()
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1100, 840 }, scaler);
            ApplyKeyScheme (DebuggerKeyScheme::VisualStudio);
            AppendConsole ({ "LDA #$00", "STA $C030" });
        }

        void  Type (const std::wstring & text)
        {
            for (wchar_t ch : text)
            {
                DxuiKeyEvent  down  = { DxuiKeyEventKind::Down, (WPARAM) (ch == L' ' ? VK_SPACE : towupper (ch)), false, false, false, false };
                DxuiKeyEvent  typed = { DxuiKeyEventKind::Char, (WPARAM) ch, false, false, false, false };

                (void) (OnKey (down) || RouteMappedKey (down));
                (void) OnKey (typed);
            }
        }

        void  Chord (WPARAM vk, bool ctrl, bool shift)
        {
            DxuiKeyEvent  ev = { DxuiKeyEventKind::Down, vk, false, shift, ctrl, false };

            (void) (OnKey (ev) || RouteMappedKey (ev));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ConsoleGapTests
    //
    //  The gap between one command and the next is three quarters of a line.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ConsoleGapTests)
    {
    public:

        TEST_METHOD (AShortRowMovesTheNextLineUpByItsFraction)
        {
            constexpr float       kMockLineDip = 16.0f;
            DxuiTextView          view;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            DxuiDpiScaler         scaler;
            DxuiTextView::Row     first;
            DxuiTextView::Row     gap;
            DxuiTextView::Row     second;
            float                 firstY       = -1.0f;
            float                 secondY      = -1.0f;



            scaler.SetDpi (96);
            first.cells  = { L"alpha" };
            gap.cells    = { L"" };
            gap.height   = 0.75f;
            second.cells = { L"beta" };

            view.Layout (RECT { 0, 0, 400, 300 }, scaler);
            view.SetRows ({ first, gap, second });
            view.Paint   (painter, text, theme);
            text.Reset();
            view.Paint   (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.text == L"alpha") { firstY  = call.y; }
                if (call.text == L"beta")  { secondY = call.y; }
            }

            Assert::AreEqual (kMockLineDip * 1.75f, secondY - firstY, L"a line, then three quarters of one");
        }


        TEST_METHOD (TheConsoleDrawsACommandGapAtThreeQuartersOfALine)
        {
            CassoTheme                       theme  = CassoTheme::MakeSkeuomorphic();
            PolishHost                       host;
            PolishWindow                     window (theme, host);
            std::vector<std::string>         lines  = { "> R", "A=00" };



            window.Build();
            DebuggerViewState::AddCommandGap (lines);
            window.AppendConsole (lines);

            const std::vector<DxuiTextView::Row> & rows = window.GetConsolePane().GetOutput().GetView()->GetRows();

            Assert::AreEqual ((size_t) 5, rows.size());
            Assert::AreEqual (0.75f,          rows[2].height, L"the gap row is short");
            Assert::AreEqual (std::wstring(), rows[2].cells[0], L"and blank");
            Assert::AreEqual (1.0f,           rows[3].height, L"the command's own line is whole");
        }


        //  Each line the console adds, the command typed with the rest,
        //  starts with the time it was added, in the first column. The gap
        //  between commands doesn't have a time.
        TEST_METHOD (EachConsoleLineStartsWithTheTimeItWasAdded)
        {
            CassoTheme                 theme  = CassoTheme::MakeSkeuomorphic();
            PolishHost                 host;
            PolishWindow               window (theme, host);
            std::vector<std::string>   lines  = { "> R", "A=00" };
            SYSTEMTIME                 time   = { 2026, 11, 2, 3, 14, 5, 9, 0 };
            ConsoleOutput            * output = nullptr;



            window.Build();
            output = &window.GetConsolePane().GetOutput();
            DebuggerViewState::AddCommandGap (lines);
            output->Append (lines, time);

            const std::vector<DxuiTextView::Row> & rows = output->GetView()->GetRows();

            Assert::AreEqual ((size_t) 5, rows.size());
            Assert::AreEqual (ConsoleOutput::GetTimeStamp (time), rows[3].cells[0], L"the command typed");
            Assert::AreEqual (std::wstring (L"> R"),              rows[3].cells[1]);
            Assert::AreEqual (ConsoleOutput::GetTimeStamp (time), rows[4].cells[0], L"its output");
            Assert::AreEqual ((size_t) 1,                         rows[2].cells.size(), L"the gap");
        }


        //  The console's text, each line with its time, is saved to the
        //  project once output stops for a moment, and comes back with the
        //  project: on opening ahead of what was written since, on a
        //  project's switch in place of it.
        TEST_METHOD (TheConsolesTextIsSavedAndComesBack)
        {
            class TextHost : public PolishHost
            {
            public:
                void  SetDebuggerWindowState (const std::string & name, const std::string & text) override { state[name] = text; }

                std::map<std::string, std::string>  state;
            };

            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            TextHost         host;
            PolishWindow     window (theme, host);
            SYSTEMTIME       time   = { 2026, 11, 2, 3, 14, 5, 9, 0 };
            ConsoleOutput  * output = nullptr;
            std::string      saved;



            window.Build();
            output = &window.GetConsolePane().GetOutput();
            output->Clear();
            output->Append ({ "> R", "A=00\tX=01" }, time);

            output->SaveIfChanged ((int64_t) GetTickCount64());
            Assert::IsFalse (host.state.contains ("consoleText"), L"not right after the output");

            output->SaveIfChanged ((int64_t) GetTickCount64() + 60000);
            saved = host.state["consoleText"];
            Assert::AreEqual (std::string ("2026-11-03 14:05:09\t> R\n2026-11-03 14:05:09\tA=00\tX=01\n"), saved);

            output->Clear();
            output->Append ({ "notice" }, time);
            output->RestoreSaved (saved, true);
            Assert::AreEqual (std::string ("2026-11-03 14:05:09\t> R\n2026-11-03 14:05:09\tA=00\tX=01\n2026-11-03 14:05:09\tnotice\n"), output->FormatSaved(), L"ahead of what was written");

            output->RestoreSaved (saved, false);
            Assert::AreEqual (saved, output->FormatSaved(), L"in place of it");
            Assert::AreEqual (ConsoleOutput::GetTimeStamp (time), output->GetView()->GetRows().front().cells[0], L"with its time stamp");
        }


        //  The stamp is the short time in brackets; the tip over it has the
        //  long date and the time with its seconds.
        TEST_METHOD (TheTimeStampIsTheShortTimeAndItsTipTheFullDateAndTime)
        {
            SYSTEMTIME  time = { 2026, 11, 2, 3, 14, 5, 9, 0 };



            Assert::AreEqual (std::wstring (L"[2:05 PM]"),                          ConsoleOutput::GetTimeStamp (time, L"en-US"));
            Assert::AreEqual (std::wstring (L"[14:05]"),                            ConsoleOutput::GetTimeStamp (time, L"de-DE"));
            Assert::AreEqual (std::wstring (L"Tuesday, November 3, 2026 2:05:09 PM"), ConsoleOutput::GetFullTime (time, L"en-US"));
        }


        //  The pointer over a line's stamp has the full time; over its text,
        //  none.
        TEST_METHOD (TheTipOverALinesStampHasItsFullTime)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            PolishHost       host;
            PolishWindow     window (theme, host);
            SYSTEMTIME       time   = { 2026, 11, 2, 3, 14, 5, 9, 0 };
            ConsoleOutput  * output = nullptr;
            DxuiDpiScaler    scaler;
            RECT             anchor = {};
            std::wstring     text;
            float            stampX = 0.0f;
            float            textX  = 0.0f;
            float            middle = 0.0f;
            int              row    = 0;



            scaler.SetDpi (96);
            window.Build();
            output = &window.GetConsolePane().GetOutput();
            output->GetView()->SetCellSize (8, 16);
            output->GetView()->Layout (RECT { 0, 0, 400, 300 }, scaler);
            output->Append ({ "> R" }, time);
            row = (int) output->GetView()->GetRows().size() - 1;

            Assert::IsTrue (output->GetView()->GetCellAnchorPx (row, 0, stampX, middle), L"the last row is drawn");
            Assert::IsTrue (output->GetView()->GetCellAnchorPx (row, 1, textX,  middle));

            Assert::IsTrue   (output->TryGetTimeTip (POINT { (LONG) stampX + 2, (LONG) middle }, anchor, text), L"over the stamp");
            Assert::AreEqual (ConsoleOutput::GetFullTime (time), text);
            Assert::IsFalse  (output->TryGetTimeTip (POINT { (LONG) textX + 4, (LONG) middle }, anchor, text), L"over the text");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ConsoleLinkTests
    //
    //  A console line's link to a file: drawn in the link color, its tip the
    //  file's full path, and a click on it opens the file through the host.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ConsoleLinkTests)
    {
    public:

        class LinkHost : public PolishHost
        {
        public:
            void  OpenDebuggerFile (const std::wstring & path) override { opened.push_back (path); }

            std::vector<std::wstring>  opened;
        };


        //  The console with one line, "from prog.sym", whose file's name is
        //  linked, laid out in 8 by 16 cells; and the middle of a character
        //  of that line's text.
        class LinkRig
        {
        public:
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            LinkHost          host;
            PolishWindow      window { theme, host };
            ConsoleOutput   * output = nullptr;
            int               row    = 0;

            LinkRig()
            {
                DxuiDpiScaler  scaler;
                SYSTEMTIME     time   = { 2026, 11, 2, 3, 14, 5, 9, 0 };



                scaler.SetDpi (96);
                window.Build();
                output = &window.GetConsolePane().GetOutput();
                output->Clear();
                output->GetView()->SetCellSize (8, 16);
                output->GetView()->Layout (RECT { 0, 0, 400, 300 }, scaler);
                output->Append ({ ConsoleLinks::LinkFiles ("from C:\\Work\\prog.sym", { "C:\\Work\\prog.sym" }) }, time);
                row = (int) output->GetView()->GetRows().size() - 1;
            }

            POINT GetCharPoint (int offset)
            {
                float  left   = 0.0f;
                float  middle = 0.0f;



                Assert::IsTrue (output->GetView()->GetCellAnchorPx (row, 1, left, middle), L"the line is drawn");
                return POINT { (LONG) (left + (float) offset * 8.0f + 4.0f), (LONG) middle };
            }

            void Click (POINT point)
            {
                DxuiMouseEvent  down = { DxuiMouseEventKind::Down, DxuiMouseButton::Left, point };
                DxuiMouseEvent  up   = { DxuiMouseEventKind::Up,   DxuiMouseButton::Left, point };



                (void) output->RouteMouse (down, window.GetConsolePane().GetCommandLine().GetBox());
                (void) output->RouteMouse (up,   window.GetConsolePane().GetCommandLine().GetBox());
            }
        };


        //  The line is drawn without its marks, with a link span over the
        //  file's name; the text the project saves has the marks, so the link
        //  comes back with the console's text.
        TEST_METHOD (ALinkIsASpanOverTheFilesName)
        {
            LinkRig                          rig;
            const DxuiTextView::Row        & row   = rig.output->GetView()->GetRows()[(size_t) rig.row];
            std::vector<ConsoleLinks::Link>  links;



            Assert::AreEqual (std::wstring (L"from prog.sym"), row.cells.at (1));
            Assert::AreEqual ((size_t) 1, row.spans.size());
            Assert::IsTrue   (row.spans[0].isLink);
            Assert::AreEqual (5, row.spans[0].start);
            Assert::AreEqual (8, row.spans[0].length);
            Assert::IsTrue   (rig.output->FormatSaved().find (ConsoleLinks::kOpen) != std::string::npos, L"the saved text has the link");
        }


        TEST_METHOD (TheTipOverALinkIsTheFilesFullPath)
        {
            LinkRig       rig;
            RECT          anchor = {};
            std::wstring  text;



            Assert::IsTrue   (rig.output->TryGetLinkTip (rig.GetCharPoint (7), anchor, text), L"over the file's name");
            Assert::AreEqual (std::wstring (L"C:\\Work\\prog.sym"), text);
            Assert::IsFalse  (rig.output->TryGetLinkTip (rig.GetCharPoint (1), anchor, text), L"over the word before it");
        }


        TEST_METHOD (AClickOnALinkOpensItsFile)
        {
            LinkRig  rig;



            rig.Click (rig.GetCharPoint (1));
            Assert::AreEqual ((size_t) 0, rig.host.opened.size(), L"a click off the link opens nothing");

            rig.Click (rig.GetCharPoint (6));
            Assert::AreEqual ((size_t) 1, rig.host.opened.size(), L"a click on it opens it");
            Assert::AreEqual (std::wstring (L"C:\\Work\\prog.sym"), rig.host.opened[0]);
        }


        //  A drag from the link selects text, as it does anywhere else, and
        //  opens nothing.
        TEST_METHOD (ADragFromALinkSelectsRatherThanOpens)
        {
            LinkRig         rig;
            DxuiMouseEvent  down = { DxuiMouseEventKind::Down, DxuiMouseButton::Left, rig.GetCharPoint (6) };
            DxuiMouseEvent  move = { DxuiMouseEventKind::Move, DxuiMouseButton::None, rig.GetCharPoint (1) };
            DxuiMouseEvent  up   = { DxuiMouseEventKind::Up,   DxuiMouseButton::Left, rig.GetCharPoint (6) };



            (void) rig.output->RouteMouse (down, rig.window.GetConsolePane().GetCommandLine().GetBox());
            (void) rig.output->RouteMouse (move, rig.window.GetConsolePane().GetCommandLine().GetBox());
            (void) rig.output->RouteMouse (up,   rig.window.GetConsolePane().GetCommandLine().GetBox());

            Assert::IsTrue   (rig.output->GetView()->HasSelection());
            Assert::AreEqual ((size_t) 0, rig.host.opened.size());
        }


        //  The link is in the theme's link color, the pointer over it is
        //  IDC_HAND, and the link is underlined while the pointer is there.
        TEST_METHOD (ALinkIsDrawnInTheLinkColorAndUnderlinedUnderThePointer)
        {
            LinkRig                   rig;
            MockDxuiPainter           painter;
            MockDxuiTextRenderer      text;
            MockDxuiTheme             theme;
            DxuiTextView            * view       = rig.output->GetView();
            uint32_t                  link       = theme.TextColor (DxuiTextRole::Link);
            const RecordedTextCall  * name       = nullptr;
            bool                      underlined = false;



            view->Paint (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                name = (call.text == L"prog.sym") ? &call : name;
            }

            Assert::IsNotNull (name, L"the file's name is drawn as one run");
            Assert::AreEqual  (link, name->argb);
            Assert::IsTrue    (view->GetCursorForPoint (rig.GetCharPoint (6)) == IDC_HAND);
            Assert::IsTrue    (view->GetCursorForPoint (rig.GetCharPoint (1)) == IDC_IBEAM);

            Assert::IsTrue (view->SetHoverPoint (rig.GetCharPoint (6)), L"the pointer moved onto the link");
            painter.Reset();
            view->Paint (painter, text, theme);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                underlined = underlined || (call.kind == RecordedPaintKind::FillRect && call.argb == link && call.width == 64.0f);
            }

            Assert::IsTrue  (underlined, L"under the file's eight characters");
            Assert::IsTrue  (view->SetHoverPoint (POINT { -1, -1 }), L"and off it");
            Assert::IsFalse (view->SetHoverPoint (POINT { -1, -1 }), L"which changes nothing more");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TraceKeyHintTests
    //
    //  The trace pane's key line draws each key in the accent and what it does
    //  in the text color, with a wide space between pairs.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (TraceKeyHintTests)
    {
    public:

        TEST_METHOD (KeysAreAccentedAndPairsSpacedApart)
        {
            KeyHintLine               line;
            MockDxuiPainter           painter;
            MockDxuiTextRenderer      text;
            MockDxuiTheme             theme;
            DxuiDpiScaler             scaler;
            const RecordedTextCall  * key     = nullptr;
            const RecordedTextCall  * desc    = nullptr;
            const RecordedTextCall  * next    = nullptr;



            scaler.SetDpi (96);
            line.SetPairs (TracePane::GetKeyPairs());
            line.Layout   (RECT { 0, 0, 2000, 20 }, scaler);
            line.Paint    (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.text == L"Space")     { key  = &call; }
                if (call.text == L"step into") { desc = &call; }
                if (call.text == L"O")         { next = &call; }
            }

            Assert::IsNotNull (key);
            Assert::IsNotNull (desc);
            Assert::IsNotNull (next);
            Assert::AreEqual  ((uint32_t) theme.Accent(),     key->argb,  L"the key in the accent");
            Assert::AreEqual  ((uint32_t) theme.Foreground(), desc->argb, L"what it does in the text color");
            Assert::IsTrue    (next->x - (desc->x + 9.0f * 7.0f) >= KeyHintLine::kPairGapDip, L"a wide space before the next pair");
            Assert::AreEqual  (TracePane::GetKeyHint(), line.GetText(), L"the label's text is the whole line");
        }


        //  Each pair is a button: a press and release over it picks its
        //  action, and a release over another pair picks nothing.
        TEST_METHOD (APairClickedPicksItsAction)
        {
            KeyHintLine               line;
            MockDxuiPainter           painter;
            MockDxuiTextRenderer      text;
            MockDxuiTheme             theme;
            DxuiDpiScaler             scaler;
            std::vector<int>          picked;
            const RecordedTextCall  * over   = nullptr;
            const RecordedTextCall  * run    = nullptr;
            DxuiMouseEvent            ev;



            scaler.SetDpi (96);
            line.SetPairs  (TracePane::GetKeyPairs());
            line.SetOnPick ([&picked] (int id) { picked.push_back (id); });
            line.Layout    (RECT { 0, 0, 2000, 20 }, scaler);
            line.Paint     (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.text == L"step over") { over = &call; }
                if (call.text == L"run")       { run  = &call; }
            }

            Assert::IsNotNull (over);
            Assert::IsNotNull (run);

            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { (LONG) over->x + 2, 10 };

            ev.kind = DxuiMouseEventKind::Down;
            Assert::IsTrue (line.OnMouse (ev), L"a press on a pair is the line's");

            ev.kind = DxuiMouseEventKind::Up;
            Assert::IsTrue (line.OnMouse (ev));
            Assert::IsTrue (picked == std::vector<int> { (int) TracePane::KeyAction::StepOver }, L"O's action, as the key's");

            ev.kind = DxuiMouseEventKind::Down;
            (void) line.OnMouse (ev);

            ev.kind        = DxuiMouseEventKind::Up;
            ev.positionDip = POINT { (LONG) run->x + 2, 10 };
            (void) line.OnMouse (ev);
            Assert::AreEqual ((size_t) 1, picked.size(), L"released over another pair, nothing");

            ev.kind        = DxuiMouseEventKind::Down;
            ev.positionDip = POINT { 1990, 10 };
            Assert::IsFalse (line.OnMouse (ev), L"a press past the pairs is not the line's");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ConsoleModeLabelTests
    //
    //  The console toolbar shows "Mode:" as a label of its own, so the
    //  drop-down reads the dialect alone.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ConsoleModeLabelTests)
    {
    public:

        TEST_METHOD (TheDropDownReadsTheDialectAlone)
        {
            ToolbarLabelEntry  label (L"Mode:");



            Assert::AreEqual (std::wstring (L"AppleWin"), PolishWindow::GetModeLabel (L"AppleWin"));
            Assert::AreEqual (std::wstring (L"Mode:"),    label.GetCommand()->label);
            Assert::IsTrue   (label.OnClick (0, 0), L"a click on the label runs nothing");
        }


        TEST_METHOD (TheDropDownListsTheModesAlphabetically)
        {
            const std::vector<PolishWindow::ModeChoice> &  choices = PolishWindow::GetModeChoices();
            std::wstring                                   labels;



            for (const auto & [mode, label] : choices)
            {
                labels += (labels.empty() ? L"" : L", ") + std::wstring (label);
            }

            Assert::AreEqual (std::wstring (L"AppleWin, Casso, GSSquared, Monitor, WinDbg"), labels);

            for (size_t i = 1; i < choices.size(); i++)
            {
                Assert::IsTrue (_wcsicmp (choices[i - 1].second, choices[i].second) < 0, choices[i].second);
            }
        }


        TEST_METHOD (EachLabelSetsItsOwnMode)
        {
            const std::vector<PolishWindow::ModeChoice> &  choices = PolishWindow::GetModeChoices();
            std::string                                    name;



            Assert::AreEqual ((size_t) 5, choices.size(), L"one row for each mode");

            for (const auto & [mode, label] : choices)
            {
                name = CommandModeNames::GetName (mode);
                Assert::AreEqual (0, _wcsicmp (TextEncoding::NarrowToWide (name).c_str(), label), label);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FindBoxEditingTests
    //
    //  The find box edits as a text box does, and Space on one of the find
    //  widget's option buttons toggles it without typing into the box.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (FindBoxEditingTests)
    {
    public:

        TEST_METHOD (SpaceOnAnOptionTogglesItWithoutTypingASpace)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            PolishHost     host;
            PolishWindow   window (theme, host);
            std::wstring   before;



            window.Build();
            window.GetFindWidgets().Open (DebuggerLayout::kConsole);
            window.Type (L"ld");
            before = window.GetFindWidgets().GetStatus();

            window.FocusControl (window.GetFindWidgets().GetActiveControls()[2]);
            window.Type (L" ");

            Assert::AreEqual (std::wstring (L"ld"),         window.GetFindWidgets().GetBox()->GetText(), L"no space typed into the box");
            Assert::AreEqual (std::wstring (L"1 of 1"),     before);
            Assert::AreEqual (std::wstring (L"No results"), window.GetFindWidgets().GetStatus(), L"whole word is on, so ld no longer matches");
        }


        TEST_METHOD (TheBoxSelectsAllAndUndoesAndRedoes)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            PolishHost       host;
            PolishWindow     window (theme, host);
            DxuiTextInput  * box    = nullptr;



            window.Build();
            window.GetFindWidgets().Open (DebuggerLayout::kConsole);
            window.Type (L"lda");
            box = window.GetFindWidgets().GetBox();

            window.Chord ('A', true, false);
            Assert::AreEqual ((size_t) 0, box->GetSelectionStart(), L"Ctrl+A selects from the start");
            Assert::AreEqual ((size_t) 3, box->GetSelectionEnd(),   L"to the end");

            window.Type (L"sta");
            Assert::AreEqual (std::wstring (L"sta"), box->GetText(), L"typing replaces the selection");

            window.Chord (VK_HOME, false, true);
            Assert::AreEqual ((size_t) 0, box->GetSelectionStart(), L"Shift+Home selects back to the start");
            Assert::AreEqual ((size_t) 3, box->GetSelectionEnd());

            window.Chord ('Z', true, false);
            Assert::AreEqual (std::wstring (L"lda"), box->GetText(), L"Ctrl+Z takes the typing back as one step");

            window.Chord ('Y', true, false);
            Assert::AreEqual (std::wstring (L"sta"), box->GetText(), L"Ctrl+Y puts it back");
        }


        TEST_METHOD (TheWholeWordButtonDrawsATrayUnderItsLabel)
        {
            WholeWordButton       button;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            DxuiDpiScaler         scaler;
            int                   strokes = 0;



            scaler.SetDpi (96);
            button.Layout (RECT { 0, 0, 22, 22 }, scaler);
            button.Paint  (painter, text, theme);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                if (call.kind == RecordedPaintKind::FillRect && call.argb == (uint32_t) theme.ButtonText())
                {
                    strokes++;
                }
            }

            Assert::AreEqual (3, strokes, L"the tray's bottom and both ends");
            Assert::AreEqual (std::wstring (L"ab"), button.GetAccessibleName());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WatchRowStyleTests
    //
    //  The watch pane's heading and add rows are in the proportional face of a
    //  list's column headings, the add row in italic, and the headings sit on a
    //  subtle fill.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WatchRowStyleTests)
    {
    public:

        TEST_METHOD (HeadingsAndTheAddRowUseTheBodyFace)
        {
            CassoTheme                                theme    = CassoTheme::MakeSkeuomorphic();
            PolishHost                                host;
            PolishWindow                              window   (theme, host);
            auto                                      snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::AutoWatchLine       line;
            int                                       last     = 0;



            line.label = "A";
            line.value = "00";
            line.key   = "A";
            snapshot->autoWatches      = { line };
            snapshot->zeroPagePointers = { { 1, 0x0006, "$0300: A9 41 8D 00", true } };

            window.Build();
            window.TakeSnapshot (snapshot);

            Assert::AreEqual (std::wstring (L"Auto"), window.GetWatchesPane().GetList()->GetCellsOfRow (0)[1].text, L"an automatic watch has no heading, but Auto as its ID");

            const DxuiListView::Cell & heading = window.GetWatchesPane().GetList()->GetCellsOfRow (1)[1];

            last = window.GetWatchesPane().GetList()->GetRowCount() - 1;

            const DxuiListView::Cell & add = window.GetWatchesPane().GetList()->GetCellsOfRow (last)[2];

            Assert::AreEqual (std::wstring (L"Zero-page pointers"), heading.text);
            Assert::IsTrue   (heading.face != nullptr && std::wstring (heading.face) == DxuiTheme::kBodyFace, L"the heading in the body face");
            Assert::AreNotEqual ((uint32_t) theme.HoverBackground(), heading.background, L"not the strong hover band");
            Assert::IsTrue   (DxuiColor::ComputeContrastRatio (heading.background, theme.ContentBackground()) <
                              DxuiColor::ComputeContrastRatio (theme.HoverBackground(), theme.ContentBackground()) ||
                              theme.HoverBackground() == theme.ContentBackground(), L"closer to the pane than the hover color is");
            Assert::AreEqual (std::wstring (L"Add new watch"), add.text);
            Assert::IsTrue   (add.face != nullptr && std::wstring (add.face) == DxuiTheme::kBodyFace, L"the add row in the body face");
            Assert::IsTrue   (add.weight == DxuiFontWeight::Italic, L"in italic");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CheckboxOutlineTests
    //
    //  In a light theme an enabled check box has an outline.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CheckboxOutlineTests)
    {
    public:

        TEST_METHOD (AnEnabledLightBoxIsOutlined)
        {
            DxuiLightTheme        light;
            DxuiCheckbox          box (L"Show bytes");
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler;
            bool                  outlined = false;



            scaler.SetDpi (96);
            box.Layout (RECT { 0, 0, 200, 20 }, scaler);
            box.Paint  (painter, text, light);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                outlined = outlined || call.kind == RecordedPaintKind::OutlineRoundedRect;
            }

            Assert::IsTrue (outlined, L"the box's edge is drawn");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PaneListKeyTests
    //
    //  The panes' lists take the arrow keys, so a row is chosen without the
    //  mouse.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PaneListKeyTests)
    {
    public:

        TEST_METHOD (DownMovesTheSelectionInAFocusedList)
        {
            CassoTheme                                theme    = CassoTheme::MakeSkeuomorphic();
            PolishHost                                host;
            PolishWindow                              window   (theme, host);
            auto                                      snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::AutoWatchLine       a;
            DebuggerViewSnapshot::AutoWatchLine       x;
            int                                       before   = 0;



            a.label = "A";
            a.value = "00";
            a.key   = "A";
            x.label = "X";
            x.value = "01";
            x.key   = "X";
            snapshot->autoWatches = { a, x };

            window.Build();
            window.TakeSnapshot (snapshot);
            window.FocusControl (window.GetWatchesPane().GetList());
            window.GetWatchesPane().GetList()->SetSelectedRow (1);

            before = window.GetWatchesPane().GetList()->GetSelectedRow();
            window.Chord (VK_DOWN, false, false);

            Assert::AreEqual (before + 1, window.GetWatchesPane().GetList()->GetSelectedRow(), L"Down selects the next row");

            window.Chord (VK_UP, false, false);

            Assert::AreEqual (before, window.GetWatchesPane().GetList()->GetSelectedRow(), L"Up selects the row above");
        }
    };
}

#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Core/TextEncoding.h"
#include "Debugger/CommandModeNames.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/KeyHintLine.h"
#include "Ui/Debugger/Panes/TracePane.h"
#include "Ui/Debugger/WholeWordButton.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

#include "CppUnitTest.h"

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
        using DebuggerWindow::GetConsoleView;
        using DebuggerWindow::GetFindBox;
        using DebuggerWindow::GetFindControls;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetFindStatus;
        using DebuggerWindow::GetModeChoices;
        using DebuggerWindow::GetModeLabel;
        using DebuggerWindow::ModeChoice;
        using DebuggerWindow::GetWatchList;
        using DebuggerWindow::OpenFindIn;
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

            const std::vector<DxuiTextView::Row> & rows = window.GetConsoleView()->GetRows();

            Assert::AreEqual ((size_t) 5, rows.size());
            Assert::AreEqual (0.75f,          rows[2].height, L"the gap row is short");
            Assert::AreEqual (std::wstring(), rows[2].cells[0], L"and blank");
            Assert::AreEqual (1.0f,           rows[3].height, L"the command's own line is whole");
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
            window.OpenFindIn (DebuggerLayout::kConsole);
            window.Type (L"ld");
            before = window.GetFindStatus();

            window.FocusControl (window.GetFindControls()[2]);
            window.Type (L" ");

            Assert::AreEqual (std::wstring (L"ld"),         window.GetFindBox()->GetText(), L"no space typed into the box");
            Assert::AreEqual (std::wstring (L"1 of 1"),     before);
            Assert::AreEqual (std::wstring (L"No results"), window.GetFindStatus(), L"whole word is on, so ld no longer matches");
        }


        TEST_METHOD (TheBoxSelectsAllAndUndoesAndRedoes)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            PolishHost       host;
            PolishWindow     window (theme, host);
            DxuiTextInput  * box    = nullptr;



            window.Build();
            window.OpenFindIn (DebuggerLayout::kConsole);
            window.Type (L"lda");
            box = window.GetFindBox();

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
            snapshot->autoWatches = { line };

            window.Build();
            window.TakeSnapshot (snapshot);

            const DxuiListView::Cell & heading = window.GetWatchList()->GetCellsOfRow (0)[0];

            last = window.GetWatchList()->GetRowCount() - 1;

            const DxuiListView::Cell & add = window.GetWatchList()->GetCellsOfRow (last)[0];

            Assert::AreEqual (std::wstring (L"Automatic"), heading.text);
            Assert::IsTrue   (heading.face != nullptr && std::wstring (heading.face) == DxuiTheme::kBodyFace, L"the heading in the body face");
            Assert::AreNotEqual ((uint32_t) theme.HoverBackground(), heading.background, L"not the strong hover band");
            Assert::IsTrue   (DxuiColor::ComputeContrastRatio (heading.background, theme.ContentBackground()) <
                              DxuiColor::ComputeContrastRatio (theme.HoverBackground(), theme.ContentBackground()) ||
                              theme.HoverBackground() == theme.ContentBackground(), L"closer to the pane than the hover color is");
            Assert::AreEqual (std::wstring (L"Add item to watch"), add.text);
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
}

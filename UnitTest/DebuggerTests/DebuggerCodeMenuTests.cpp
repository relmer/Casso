#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerCodeMenuTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  QuietDebuggerHost
    //
    //  A host that adds the echo of every action the window runs to commands.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class QuietDebuggerHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string & line)                 override { commands.push_back (line); }
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string & text)            override { goTos.push_back (text); }
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}
        void  RunDebuggerAction       (const DebuggerAction & action)   override { commands.push_back (action.echo); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>  goTos;
        std::vector<std::string>  commands;

    private:
        FakeHostDialogs  m_dialogs;
    };






    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MenuWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MenuWindow : public DebuggerWindow
    {
    public:
        MenuWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::ApplyCodeSnapshot;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::GetCodeSelection;
        using DebuggerWindow::AddCodeEditItems;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CodeMenuTests
    //
    //  The code pane's menu marks data, skips routines, toggles bookmarks,
    //  times with the stopwatch and calls a routine, each as a typed action
    //  (FR-182, FR-184, FR-189, FR-197).
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CodeMenuTests)
    {
    public:

        using Items = std::vector<std::pair<std::wstring, std::function<void()>>>;

        static std::shared_ptr<DebuggerViewSnapshot> MakeSnapshot()
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  lda;
            DebuggerViewSnapshot::CodeLine  jsr;
            DebuggerViewSnapshot::CodeLine  rts;



            lda.address      = 0x0300;
            lda.bytes        = "A9 41";
            lda.instruction  = "LDA #$41";
            lda.isCurrent    = true;

            jsr.address      = 0x0302;
            jsr.bytes        = "20 ED FD";
            jsr.instruction  = "JSR COUT";
            jsr.shownOperand = "COUT";
            jsr.target       = 0xFDED;

            rts.address      = 0x0305;
            rts.bytes        = "60";
            rts.instruction  = "RTS";

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { lda, jsr, rts };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->bookmarks    = { { 0, 0x0305, std::string(), true } };
            return snapshot;
        }

        static void Run (const Items & items, const std::wstring & label)
        {
            auto  found = std::ranges::find_if (items, [&label] (const auto & item) { return item.first == label; });



            Assert::IsTrue (found != items.end(), label.c_str());
            found->second();
        }

        static bool Has (const Items & items, const std::wstring & label)
        {
            return std::ranges::any_of (items, [&label] (const auto & item) { return item.first == label; });
        }



        TEST_METHOD (TheMenuRunsEachEntryAsATypedAction)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            MenuWindow         window   (theme, host);
            DxuiDpiScaler      scaler;
            auto               snapshot = MakeSnapshot();
            Items              items;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (snapshot);

            window.AddCodeEditItems (0, snapshot->code[1], 1, items);

            Run (items, L"Mark as words");
            Run (items, L"Clear data marking");
            Run (items, L"Never step into this routine");
            Run (items, L"Add bookmark");
            Run (items, L"Start stopwatch here");
            Run (items, L"Call subroutine here");

            Assert::IsTrue (host.commands == std::vector<std::string> { "DW 0302:0304", "X 0302:0304", "SKIP COUT", "BMA 0302", "JSR 0302" },
                            L"the JSR's three bytes, the routine it calls by its symbol");

            items.clear();
            window.AddCodeEditItems (0, snapshot->code[2], 2, items);

            Assert::IsTrue (Has (items, L"Remove bookmark"), L"the RTS has a bookmark");
            Run (items, L"Stop stopwatch here");
            Assert::AreEqual (std::string ("STOPWATCH 0302 0305"), host.commands.back());
        }



        TEST_METHOD (ASelectionMarksFromItsFirstLineToItsLastByte)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            MenuWindow         window   (theme, host);
            DxuiDpiScaler      scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (MakeSnapshot());

            window.ApplyCodeSnapshot (MakeSnapshot(), 0);

            Assert::IsTrue (window.GetCodeSelection (0, 0) == std::pair<Word, Word> (0x0300, 0x0301), L"the clicked row alone");
        }
    };
}

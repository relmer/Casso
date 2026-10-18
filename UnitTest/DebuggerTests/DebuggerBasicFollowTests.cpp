#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerBasicFollowTests
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
    //  FollowWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FollowWindow : public DebuggerWindow
    {
    public:
        FollowWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetConsolePane;
        using DebuggerWindow::GetPaneOfFocus;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BasicFollowTests
    //
    //  The BASIC program pane comes forward when the machine stops in the
    //  interpreter while a program runs, and when a BASIC line breakpoint is
    //  set.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BasicFollowTests)
    {
    public:

        static std::shared_ptr<DebuggerViewSnapshot> MakeSnapshot (bool isPaused, Word pc, std::optional<Word> line)
        {
            auto                              snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::BasicState  basic;



            basic.isApplesoft  = true;
            basic.hasProgram   = true;
            basic.line         = line;
            snapshot->isPaused = isPaused;
            snapshot->pc       = pc;
            snapshot->basic    = basic;
            return snapshot;
        }



        static DebuggerViewSnapshot::BreakpointLine MakeBasicBreakpoint (int id, Word line)
        {
            DebuggerViewSnapshot::BreakpointLine  bp;



            bp.id           = id;
            bp.address      = line;
            bp.info.kind    = BreakpointKind::BasicLine;
            bp.info.address = line;
            return bp;
        }



        TEST_METHOD (AStopInTheInterpreterBringsTheProgramForward)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            FollowWindow       window (theme, host);
            DxuiDpiScaler      scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (MakeSnapshot (false, 0xD81D, 10));

            Assert::IsFalse (window.IsPaneShown (DebuggerLayout::kBasicProgram), L"closed while the program runs");

            window.TakeSnapshot (MakeSnapshot (true, 0xD81D, 10));

            Assert::IsTrue (window.IsPaneShown (DebuggerLayout::kBasicProgram), L"the stop opens it");
        }



        //  At a stop in the interpreter, the focus the console had moves to
        //  the program pane, where the steps are BASIC's, not the disassembly.
        TEST_METHOD (AStopInTheInterpreterFocusesTheProgram)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            FollowWindow       window (theme, host);
            DxuiDpiScaler      scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (MakeSnapshot (false, 0xD81D, 10));
            window.FocusControl (window.GetConsolePane().GetCommandLine().GetBox());
            window.TakeSnapshot (MakeSnapshot (true, 0xD81D, 10));

            Assert::AreEqual (std::wstring (DebuggerLayout::kBasicProgram), window.GetPaneOfFocus());
        }



        TEST_METHOD (AStopInMachineCodeLeavesTheProgramClosed)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            FollowWindow       window (theme, host);
            DxuiDpiScaler      scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (MakeSnapshot (false, 0x0300, 10));
            window.TakeSnapshot (MakeSnapshot (true,  0x0300, 10));

            Assert::IsFalse (window.IsPaneShown (DebuggerLayout::kBasicProgram), L"a CALLed routine's stop leaves the code view in front");
        }



        TEST_METHOD (ANewBasicLineBreakpointBringsTheProgramForward)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            FollowWindow       window (theme, host);
            DxuiDpiScaler      scaler;
            auto               set    = MakeSnapshot (false, 0x0300, std::nullopt);



            set->breakpoints = { MakeBasicBreakpoint (0, 100) };

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (MakeSnapshot (false, 0x0300, std::nullopt));

            Assert::IsFalse (window.IsPaneShown (DebuggerLayout::kBasicProgram));

            window.TakeSnapshot (set);

            Assert::IsTrue (window.IsPaneShown (DebuggerLayout::kBasicProgram), L"the new breakpoint opens it");
        }



        TEST_METHOD (ABreakpointInTheFirstSnapshotLeavesTheProgramClosed)
        {
            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            FollowWindow       window (theme, host);
            DxuiDpiScaler      scaler;
            auto               first  = MakeSnapshot (false, 0x0300, std::nullopt);



            first->breakpoints = { MakeBasicBreakpoint (0, 100) };

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (first);

            Assert::IsFalse (window.IsPaneShown (DebuggerLayout::kBasicProgram), L"a project's breakpoints are not new");
        }
    };
}

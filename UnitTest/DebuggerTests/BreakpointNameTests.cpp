#include "Pch.h"

#include "ControllerRig.h"
#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/ReplyJson.h"
#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/BreakpointColumns.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/DebuggerWindow.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointNameTests
//
//  FR-172: a breakpoint's or a watchpoint's name, set by BPNAME or renamed in
//  the Breakpoints pane, in every listing but the scripts BPSAVE writes,
//  surviving BPEDIT, and undone as the pane's other steps are.
//
////////////////////////////////////////////////////////////////////////////////

namespace BreakpointNameTests
{
    using Rig = HandlerRig<BreakpointHandlers>;



    static BreakpointInfo GetInfo (Rig & rig, size_t index)
    {
        BreakpointListData  list;



        BreakpointHandlers::ListAll (rig.session, list);
        return list.breakpoints.at (index);
    }





    TEST_CLASS (CommandTests)
    {
    public:

        TEST_METHOD (BpnameSetsAndClearsAName)
        {
            Rig    rig;
            Reply  reply;



            rig.RunOk ("BP 300");
            rig.RunOk ("BPM 400");

            reply = rig.RunOk ("BPNAME 0 \"Loop top\"");
            Assert::AreEqual (std::string ("Breakpoint #0: name set to \"Loop top\"."), reply.text.at (0));
            Assert::AreEqual (std::string ("Loop top"), GetInfo (rig, 0).name, L"the case and the space as typed");

            rig.RunOk ("bpname 1 Screen");
            Assert::AreEqual (std::string ("Screen"), GetInfo (rig, 1).name, L"a watchpoint, unquoted");

            reply = rig.RunOk ("BPNAME 0");
            Assert::AreEqual (std::string ("Breakpoint #0: name cleared."), reply.text.at (0));
            Assert::IsTrue   (GetInfo (rig, 0).name.empty());

            rig.RunFails ("BPNAME 7 \"X\"", "no such breakpoint");
            rig.RunFails ("BPNAME * \"X\"", "invalid arguments");
        }

        TEST_METHOD (TheNameIsListedButNeverSaved)
        {
            Rig          rig;
            Reply        reply;
            std::string  json;



            rig.RunOk ("BP 300");
            rig.RunOk ("BPNAME 0 \"Loop top\"");

            reply = rig.RunOk ("BPL");
            Assert::AreEqual (std::string ("#0 enabled  at $0300, \"Loop top\", hits 0"), reply.text.at (0));

            json = ReplyJson::WriteReply (reply, 1);
            Assert::IsTrue (json.find ("\"name\":\"Loop top\"") != std::string::npos, TextEncoding::NarrowToWide (json).c_str());

            Assert::AreEqual (std::string ("BPC *\nBP 0300\n"), BreakpointHandlers::MakeScript (rig.session), L"BPSAVE writes AppleWin's script");
        }

        TEST_METHOD (BpeditKeepsTheName)
        {
            Rig  rig;



            rig.RunOk ("BP 300");
            rig.RunOk ("BPNAME 0 \"Loop top\"");
            rig.RunOk ("BPEDIT 0 BP 302");

            Assert::AreEqual ((Word) 0x0302,             GetInfo (rig, 0).address);
            Assert::AreEqual (std::string ("Loop top"), GetInfo (rig, 0).name);
        }

        TEST_METHOD (ThePaneShowsTheNameInPlaceOfTheDefault)
        {
            DebuggerViewSnapshot                  snapshot;
            DebuggerViewSnapshot::BreakpointLine  bp;



            bp.id           = 0;
            bp.address      = 0x0300;
            bp.enabled      = true;
            bp.info.address = 0x0300;
            bp.info.last    = 0x0300;

            Assert::AreNotEqual (std::string ("Loop top"), BreakpointColumns::GetCells (snapshot, bp)[(size_t) BreakpointColumns::Column::Name]);

            bp.info.name = "Loop top";
            Assert::AreEqual (std::string ("Loop top"), BreakpointColumns::GetCells (snapshot, bp)[(size_t) BreakpointColumns::Column::Name]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UndoTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    class HistoryRig : public ControllerRig
    {
    public:
        void  Step (DebuggerAction action)
        {
            BreakpointStep  step;

            step.actions = { std::move (action) };
            view.ExecuteBreakpointStep (controller.GetSession(), step);
        }

        void  Undo()
        {
            BreakpointStep  step;

            step.kind = BreakpointStep::Kind::Undo;
            view.ExecuteBreakpointStep (controller.GetSession(), step);
        }

        std::string  Name()
        {
            BreakpointListData  list;

            BreakpointHandlers::ListAll (controller.GetSession(), list);
            return list.breakpoints.at (0).name;
        }
    };


    TEST_CLASS (UndoTests)
    {
    public:

        TEST_METHOD (ARenameIsOneStepUndone)
        {
            HistoryRig  rig;



            rig.Run  ("BP 0300");
            rig.Step (DebuggerActions::GetNameBreakpoint (0, "First", CommandMode::AppleWin));
            rig.Step (DebuggerActions::GetNameBreakpoint (0, "Second", CommandMode::AppleWin));
            Assert::AreEqual (std::string ("Second"), rig.Name());

            rig.Undo();
            Assert::AreEqual (std::string ("First"), rig.Name());

            rig.Undo();
            Assert::IsTrue (rig.Name().empty(), L"back to the default");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EditorTests
    //
    //  The rename box, in the window with no HWND: what was typed becomes
    //  one BPNAME step; Escape and the name as it opened make none.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class StepHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}
        void  RunDebuggerAction       (const DebuggerAction & action)   override { actions.push_back (action); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<DebuggerAction>  actions;

    private:
        FakeHostDialogs  m_dialogs;
    };


    class NameWindow : public DebuggerWindow
    {
    public:
        NameWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::BeginNameEdit;
        using DebuggerWindow::EndNameEdit;
        using DebuggerWindow::TakeSnapshot;

        DxuiListView * GetList() const { return GetBreakpointList(); }

        void  Type (const std::wstring & text)
        {
            for (wchar_t ch : text)
            {
                DxuiKeyEvent  typed = { DxuiKeyEventKind::Char, (WPARAM) ch, false, false, false, false };

                (void) OnKey (typed);
            }
        }
    };


    TEST_CLASS (EditorTests)
    {
    public:

        TEST_METHOD (WhatIsTypedBecomesOneRenameStep)
        {
            CassoTheme                            theme    = CassoTheme::MakeSkeuomorphic();
            StepHost                              host;
            NameWindow                            window   (theme, host);
            DxuiDpiScaler                         scaler;
            auto                                  snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::BreakpointLine  bp;



            bp.id           = 4;
            bp.address      = 0x0300;
            bp.enabled      = true;
            bp.info.id      = 4;
            bp.info.address = 0x0300;
            bp.info.last    = 0x0300;
            snapshot->breakpoints = { bp };

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (snapshot);
            window.GetList()->SetColumns ({ { L"Name", 160, false, DxuiTextHAlign::Left } });

            window.BeginNameEdit (0);
            window.EndNameEdit (true);
            Assert::IsTrue (host.actions.empty(), L"the name as it opened renames nothing");

            window.BeginNameEdit (0);
            window.Type (L"Loop");
            window.EndNameEdit (false);
            Assert::IsTrue (host.actions.empty(), L"Escape renames nothing");

            window.BeginNameEdit (0);
            window.Type (L"Loop top");
            window.EndNameEdit (true);

            Assert::AreEqual ((size_t) 1, host.actions.size());
            Assert::IsTrue   (host.actions[0].breakpointStep.has_value(), L"a step the pane can undo");
            Assert::AreEqual (std::string ("BPNAME 4 \"Loop top\""), host.actions[0].breakpointStep->actions.at (0).echo);
        }
    };
}

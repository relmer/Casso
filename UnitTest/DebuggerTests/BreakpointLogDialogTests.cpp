#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/Panes/BreakpointsPane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointLogDialogTests
//
//  The breakpoint dialog's log message and Don't break check: the fields an
//  existing breakpoint fills in, and the BPLOG or BPCHANGE line OK adds
//  after the definition, on a new breakpoint's id or the edited one's, only
//  when the message or whether a hit stops changed.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (BreakpointLogDialogTests)
    {
    public:

        static BreakpointDialog::Result  MakeResult (const char * log, bool isGo)
        {
            return BreakpointDialog::Result { BreakpointDefinition::MakeExecute ("0300"), log, isGo };
        }


        static std::vector<std::string>  GetEchoes (const std::vector<DebuggerAction> & actions)
        {
            std::vector<std::string>  echoes;

            for (const DebuggerAction & action : actions)
            {
                echoes.push_back (action.echo);
            }

            return echoes;
        }


        TEST_METHOD (ABreakpointsLogFillsTheFields)
        {
            BreakpointInfo  info;



            info.log   = "A={A}";
            info.stops = false;

            Assert::AreEqual (std::string ("A={A}"), BreakpointDialog::GetFields (info).log);
            Assert::IsTrue   (BreakpointDialog::GetFields (info).isLogGo, L"a logging breakpoint that runs on");

            info.log = "";

            Assert::IsTrue   (BreakpointDialog::GetFields (info).isLogGo, L"a breakpoint that doesn't break, with no message, has Don't break checked");
            Assert::IsTrue   (BreakpointDialog::GetVisibleFields (BreakpointDialog::Type::Disk).log);
            Assert::IsTrue   (BreakpointDialog::GetVisibleFields (BreakpointDialog::Type::Read).log);
        }


        //  A new breakpoint with a message: its definition, then BPLOG on the
        //  id it takes; with none, the definition alone.
        TEST_METHOD (ANewBreakpointsMessageIsSetOnTheIdItTakes)
        {
            std::vector<std::string>  echoes = GetEchoes (BreakpointsPane::GetDialogActions (MakeResult ("A={A}", true), std::nullopt, BreakpointInfo {}, 7, CommandMode::AppleWin));



            Assert::AreEqual ((size_t) 2, echoes.size());
            Assert::AreEqual (std::string ("BP 0300"), echoes[0]);
            Assert::AreEqual (std::string ("BPLOG 7 GO \"A={A}\""), echoes[1]);

            echoes = GetEchoes (BreakpointsPane::GetDialogActions (MakeResult ("", false), std::nullopt, BreakpointInfo {}, 7, CommandMode::AppleWin));

            Assert::AreEqual ((size_t) 1, echoes.size(), L"no message, no BPLOG");
        }


        //  An edit adds BPLOG only for a change: a new message, Don't break
        //  switched, or the message taken off, which is BPLOG # CLEAR.
        TEST_METHOD (AnEditAddsBplogOnlyForAChange)
        {
            BreakpointInfo  original;



            original.log   = "hit";
            original.stops = true;

            Assert::AreEqual ((size_t) 1, BreakpointsPane::GetDialogActions (MakeResult ("hit", false), 3, original, 9, CommandMode::AppleWin).size(), L"the same message and choice");
            Assert::AreEqual (std::string ("BPLOG 3 GO \"hit\""),
                              GetEchoes (BreakpointsPane::GetDialogActions (MakeResult ("hit", true), 3, original, 9, CommandMode::AppleWin)).at (1));
            Assert::AreEqual (std::string ("BPLOG 3 \"other\""),
                              GetEchoes (BreakpointsPane::GetDialogActions (MakeResult ("other", false), 3, original, 9, CommandMode::AppleWin)).at (1));
            Assert::AreEqual (std::string ("BPLOG 3 CLEAR"),
                              GetEchoes (BreakpointsPane::GetDialogActions (MakeResult ("", false), 3, original, 9, CommandMode::AppleWin)).at (1));
        }


        //  Don't break with no message is the breakpoint's flag, set by
        //  BPCHANGE: on a new breakpoint's id, on an edit that switches it,
        //  and after BPLOG CLEAR, after which a breakpoint stops.
        TEST_METHOD (DontBreakWithNoMessageIsSetByBpchange)
        {
            BreakpointInfo  original;



            Assert::AreEqual (std::string ("BPCHANGE 7 Ets"),
                              GetEchoes (BreakpointsPane::GetDialogActions (MakeResult ("", true), std::nullopt, BreakpointInfo {}, 7, CommandMode::AppleWin)).at (1));

            original.stops = true;

            Assert::AreEqual (std::string ("BPCHANGE 3 Ets"),
                              GetEchoes (BreakpointsPane::GetDialogActions (MakeResult ("", true), 3, original, 9, CommandMode::AppleWin)).at (1));
            Assert::AreEqual ((size_t) 1, BreakpointsPane::GetDialogActions (MakeResult ("", false), 3, original, 9, CommandMode::AppleWin).size(), L"no change");

            original.log   = "hit";
            original.stops = false;

            Assert::AreEqual ((size_t) 3, BreakpointsPane::GetDialogActions (MakeResult ("", true), 3, original, 9, CommandMode::AppleWin).size(), L"BPLOG CLEAR, then BPCHANGE");
        }


        //  The action's command is what the handler reads after the id.
        TEST_METHOD (TheLogActionsCommandIsWhatTheHandlerReads)
        {
            Assert::AreEqual (std::string ("GO \"x={X}\""), DebuggerActions::GetLogBreakpoint (2, "x={X}", true,  CommandMode::AppleWin).command.text);
            Assert::AreEqual (std::string ("x={X}"),        DebuggerActions::GetLogBreakpoint (2, "x={X}", false, CommandMode::AppleWin).command.text);
            Assert::AreEqual (std::string ("CLEAR"),        DebuggerActions::GetLogBreakpoint (2, "",      false, CommandMode::AppleWin).command.text);
            Assert::AreEqual (2,                            (int) DebuggerActions::GetLogBreakpoint (2, "", false, CommandMode::AppleWin).command.count);
        }


        //  OK's actions run on a real session as the CPU thread runs them: the
        //  new breakpoint has the message, and a hit runs on.
        TEST_METHOD (OksActionsSetTheNewBreakpointsMessage)
        {
            ControllerRig             rig;
            BreakpointStep            step;
            BreakpointListData        list;
            std::vector<std::string>  lines;



            step.actions = BreakpointsPane::GetDialogActions (MakeResult ("A={A}", true), std::nullopt, BreakpointInfo {},
                                                              rig.controller.GetSession().GetNextId(), CommandMode::AppleWin);
            lines = rig.view.ExecuteBreakpointStep (rig.controller.GetSession(), step);

            BreakpointHandlers::ListAll (rig.controller.GetSession(), list);

            for (const std::string & line : lines)
            {
                Logger::WriteMessage (line.c_str());
            }

            Assert::AreEqual ((size_t) 1, list.breakpoints.size());
            Assert::AreEqual (std::string ("A={A}"), list.breakpoints[0].log);
            Assert::IsFalse  (list.breakpoints[0].stops);
        }


        //  The panes are built on a copy of the session, which has the id the
        //  next breakpoint takes, so a new breakpoint's BPLOG is set on it.
        TEST_METHOD (TheSessionCopyHasTheNextId)
        {
            ControllerRig     rig;
            DebugSessionView  view;



            rig.controller.GetSession().SetNextId (9);
            rig.controller.GetSession().TakeView (view);
            rig.controller.GetSession().SetNextId (0);
            rig.controller.GetSession().InstallView (view);

            Assert::AreEqual (9, rig.controller.GetSession().GetNextId());
        }
    };
}

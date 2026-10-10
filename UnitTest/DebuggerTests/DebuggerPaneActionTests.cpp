#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/AppleWinParser.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneActionTests
//
//  The memory pane's edits, the source pane's breakpoint toggle, the call
//  stack's mode button and the trace's save run directly (FR-135): each
//  action is the command its line parses to, and the line is its echo.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DebuggerPaneActionTests)
{
public:

    static void AssertParsesTo (ControllerRig & rig, const std::string & line, const DebuggerAction & action)
    {
        AppleWinParseResult  parsed = AppleWinParser::Parse (line, rig.controller.GetSession());
        std::wstring         what   = TextEncoding::NarrowToWide (line);



        Assert::IsTrue   (parsed.status == ParseStatus::Ok, what.c_str());
        Assert::AreEqual ((int) parsed.command.verb,  (int) action.command.verb, what.c_str());
        Assert::AreEqual (parsed.command.sourceName,  action.command.sourceName, what.c_str());
        Assert::AreEqual (parsed.command.a1,          action.command.a1,         what.c_str());
        Assert::AreEqual (parsed.command.hasA1,       action.command.hasA1,      what.c_str());
        Assert::IsTrue   (parsed.command.values == action.command.values,        what.c_str());
        Assert::AreEqual (parsed.command.text,        action.command.text,       what.c_str());
        Assert::AreEqual (parsed.command.count,       action.command.count,      what.c_str());
        Assert::AreEqual (line,                       action.echo,               what.c_str());
    }



    TEST_METHOD (EachPaneActionIsTheCommandItsLineParsesTo)
    {
        ControllerRig  rig;
        CommandMode    aw      = CommandMode::AppleWin;
        const Byte     bytes[] = { 0x41, 0x42 };



        AssertParsesTo (rig, "PATCH 0400 41 42",       DebuggerActions::GetPatch            (0x0400, bytes, aw));
        AssertParsesTo (rig, "BP main.a65:4",          DebuggerActions::GetSourceBreakpoint ("main.a65", 4, aw));
        AssertParsesTo (rig, "CALLS MODE WALK",        DebuggerActions::GetCallStackMode    ("WALK", aw));
        AssertParsesTo (rig, "HISTORY SAVE \"trace.txt\"", DebuggerActions::GetSaveHistory      ("trace.txt", aw));
        AssertParsesTo (rig, "W 0300",                 DebuggerActions::GetAddWatch         (0x0300, aw));
    }



    //  A memory edit runs with no parser and writes the bytes.
    TEST_METHOD (AMemoryEditWritesItsBytesDirectly)
    {
        ControllerRig  rig;
        const Byte     bytes[] = { 0x12, 0x34 };



        rig.view.ExecuteAction (rig.controller.GetSession(), DebuggerActions::GetPatch (0x0400, bytes, CommandMode::AppleWin));

        Assert::AreEqual ((Byte) 0x12, rig.machine.GetMemoryBus().ReadByte (0x0400));
        Assert::AreEqual ((Byte) 0x34, rig.machine.GetMemoryBus().ReadByte (0x0401));
    }



    TEST_METHOD (TheCallStackButtonCyclesTheMechanisms)
    {
        Assert::AreEqual (std::string ("RECORDED"), CallStackPane::GetNextMechanism (CallStackMechanism::Hybrid));
        Assert::AreEqual (std::string ("WALK"),     CallStackPane::GetNextMechanism (CallStackMechanism::Recorded));
        Assert::AreEqual (std::string ("HYBRID"),   CallStackPane::GetNextMechanism (CallStackMechanism::Walk));
    }
};
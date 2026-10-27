#include "Pch.h"

#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebugSession.h"
#include "HandlerTestRig.h"
#include "MockDebugTarget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryGuardTests
//
//  Behind live, a debugger command that changes registers or memory goes to
//  the session's history guard first. A guard that defers the edit until
//  the user answers gets the command itself, whether it came from a typed
//  line, a window action or a pipe client, and the edit is not made;
//  commands that only read go ahead without the guard.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistoryGuardTests)
{
public:

    TEST_METHOD (AnEditTheGuardHoldsBackIsNotMade)
    {
        MockDebugTarget            target;
        RecordingNotificationSink  sink;
        DebugSession               session (target, sink, RunState::Paused);
        DebugHandlerSet            handlers;
        std::vector<DebugCommand>  asked;
        Reply                      reply;



        handlers.Attach (session);

        session.SetHistoryGuard ([&asked] (const DebugCommand & command)
        {
            asked.push_back (command);
            return false;
        });

        target.memory[0x0310] = 0x11;

        reply = session.ExecuteLine ("MEB 310 A5", CommandMode::AppleWin);

        Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, L"held back");
        Assert::AreEqual (std::string ("history kept"), reply.error.label);
        Assert::AreEqual<size_t> (1, asked.size(), L"the guard was asked once");
        Assert::AreEqual<int> (0x0310, asked[0].a1, L"with the command it came from");
        Assert::IsTrue   (asked[0].values == std::vector<Byte> { 0xA5 });
        Assert::AreEqual<int> (0x11, target.memory[0x0310], L"the byte is unchanged");
    }


    TEST_METHOD (AnEditTheGuardAllowsIsMade)
    {
        MockDebugTarget            target;
        RecordingNotificationSink  sink;
        DebugSession               session (target, sink, RunState::Paused);
        DebugHandlerSet            handlers;
        Reply                      reply;



        handlers.Attach (session);

        session.SetHistoryGuard ([] (const DebugCommand &) { return true; });

        reply = session.ExecuteLine ("MEB 310 A5", CommandMode::AppleWin);

        Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
        Assert::AreEqual<int> (0xA5, target.memory[0x0310], L"written");
    }


    TEST_METHOD (AReadIsNeverAskedAbout)
    {
        MockDebugTarget            target;
        RecordingNotificationSink  sink;
        DebugSession               session (target, sink, RunState::Paused);
        DebugHandlerSet            handlers;
        int                        asked   = 0;
        Reply                      reply;



        handlers.Attach (session);

        session.SetHistoryGuard ([&asked] (const DebugCommand &)
        {
            asked++;
            return false;
        });

        reply = session.ExecuteLine ("R", CommandMode::AppleWin);

        Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
        Assert::AreEqual (0, asked, L"reading registers changes nothing");
    }


    //  A window action runs a command with no line behind it, as a memory
    //  pane's patch does; the guard gets that command to run again.
    TEST_METHOD (AWindowActionReachesTheGuardAsItsCommand)
    {
        MockDebugTarget            target;
        RecordingNotificationSink  sink;
        DebugSession               session (target, sink, RunState::Paused);
        DebugHandlerSet            handlers;
        std::vector<DebugCommand>  asked;
        DebugCommand               patch;



        handlers.Attach (session);

        session.SetHistoryGuard ([&asked] (const DebugCommand & command)
        {
            asked.push_back (command);
            return false;
        });

        patch.verb   = DebugVerb::PatchBytes;
        patch.a1     = 0x0310;
        patch.hasA1  = true;
        patch.values = { 0xA5 };

        (void) session.Execute (patch);

        Assert::AreEqual<size_t> (1, asked.size());
        Assert::IsTrue   (asked[0].verb == DebugVerb::PatchBytes);
        Assert::AreEqual<int> (0x0310, asked[0].a1);
    }
};

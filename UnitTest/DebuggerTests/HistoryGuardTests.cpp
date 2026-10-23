#include "Pch.h"

#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Reverse/DeferredHistoryEdits.h"
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


    //  Every edit deferred behind live shares one question, and each runs, in
    //  order, on a yes.
    TEST_METHOD (EveryDeferredEditRunsInOrderAfterOneQuestion)
    {
        DeferredHistoryEdits       deferred;
        DebugCommand               first;
        DebugCommand               second;
        std::vector<DebugCommand>  edits;



        first.a1  = 0x0300;
        second.a1 = 0x0400;

        Assert::IsTrue  (deferred.Defer (first),  L"the first edit posts the question");
        Assert::IsFalse (deferred.Defer (second), L"the second shares its answer");
        Assert::IsTrue  (deferred.HasAny());

        edits = deferred.TakeAll();
        Assert::AreEqual<size_t> (2, edits.size());
        Assert::AreEqual<int> (0x0300, edits[0].a1);
        Assert::AreEqual<int> (0x0400, edits[1].a1);
        Assert::IsFalse (deferred.HasAny(), L"taken");

        (void) deferred.Defer (first);
        deferred.Clear();
        Assert::IsFalse (deferred.HasAny(), L"a no drops them");
    }
};

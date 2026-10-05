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
//  Behind live, a debugger command that changes registers or memory asks
//  the session's history guard first. A guard that holds the edit back for
//  the user's answer gets the line it came from, and the edit is not made;
//  commands that only read go ahead without asking.
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
        std::vector<std::string>   asked;
        Reply                      reply;



        handlers.Attach (session);

        session.SetHistoryGuard ([&asked] (const std::string & line, CommandMode)
        {
            asked.push_back (line);
            return false;
        });

        target.memory[0x0310] = 0x11;

        reply = session.ExecuteLine ("MEB 310 A5", CommandMode::AppleWin);

        Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, L"held back");
        Assert::AreEqual (std::string ("history kept"), reply.error.label);
        Assert::AreEqual<size_t> (1, asked.size(), L"the guard was asked once");
        Assert::AreEqual (std::string ("MEB 310 A5"), asked[0], L"with the line it came from");
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

        session.SetHistoryGuard ([] (const std::string &, CommandMode) { return true; });

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

        session.SetHistoryGuard ([&asked] (const std::string &, CommandMode)
        {
            asked++;
            return false;
        });

        reply = session.ExecuteLine ("R", CommandMode::AppleWin);

        Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
        Assert::AreEqual (0, asked, L"reading registers changes nothing");
    }
};

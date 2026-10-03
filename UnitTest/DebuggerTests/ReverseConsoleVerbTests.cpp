#include "Pch.h"

#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/Handlers/ExecutionHandlers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseConsoleVerbTests
//
//  The reverse verbs t-, p-, gu- and g- in each dialect that has them, and
//  LIVE, reach the host's reverse requester as the matching reverse command,
//  only while the machine is paused and only where the host records history.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseConsoleVerbTests)
{
public:

    struct Case
    {
        const char      * line;
        CommandMode       mode;
        ReverseCommand    expected;
    };


    TEST_METHOD (EachDialectsVerbsReachTheRequester)
    {
        static constexpr Case  kCases[] =
        {
            { "T-",   CommandMode::AppleWin,  ReverseCommand::StepBack        },
            { "P-",   CommandMode::AppleWin,  ReverseCommand::StepBackOver    },
            { "GU-",  CommandMode::AppleWin,  ReverseCommand::StepBackOut     },
            { "G-",   CommandMode::AppleWin,  ReverseCommand::ReverseContinue },
            { "LIVE", CommandMode::AppleWin,  ReverseCommand::GoLive          },
            { "t-",   CommandMode::WinDbg,    ReverseCommand::StepBack        },
            { "p-",   CommandMode::WinDbg,    ReverseCommand::StepBackOver    },
            { "gu-",  CommandMode::WinDbg,    ReverseCommand::StepBackOut     },
            { "g-",   CommandMode::WinDbg,    ReverseCommand::ReverseContinue },
            { "t-",   CommandMode::GSSquared, ReverseCommand::StepBack        },
            { "p-",   CommandMode::GSSquared, ReverseCommand::StepBackOver    },
            { "gu-",  CommandMode::GSSquared, ReverseCommand::StepBackOut     },
            { "g-",   CommandMode::GSSquared, ReverseCommand::ReverseContinue },
        };

        HandlerRig<ExecutionHandlers>  rig;
        std::vector<ReverseCommand>    taken;
        Reply                          reply;



        rig.session.SetReverseRequester ([&taken] (ReverseCommand command) { taken.push_back (command); return true; });

        for (const Case & each : kCases)
        {
            taken.clear();
            reply = rig.session.ExecuteLine (each.line, each.mode);

            Assert::IsTrue (reply.status == CommandStatus::Ok, std::format (L"{} ran", ToWide (each.line)).c_str());
            Assert::AreEqual<size_t> (1, taken.size(), std::format (L"{} reached the requester once", ToWide (each.line)).c_str());
            Assert::IsTrue (taken.front() == each.expected, std::format (L"{} is the matching reverse command", ToWide (each.line)).c_str());
        }
    }


    TEST_METHOD (ARunningMachineIsNotMovedThroughHistory)
    {
        HandlerRig<ExecutionHandlers>  rig;
        int                            calls = 0;
        Reply                          reply;



        rig.session.SetReverseRequester ([&calls] (ReverseCommand) { ++calls; return true; });
        rig.session.OnUserResumed();

        reply = rig.Run ("T-");

        Assert::IsTrue  (reply.status == CommandStatus::Error, L"refused while running");
        Assert::AreEqual (0, calls, L"the requester was not asked");
        Assert::IsTrue  (reply.error.detail.find ("Pause the machine first") != std::string::npos, ToWide (reply.error.detail).c_str());
    }


    TEST_METHOD (WithoutRecordingTheVerbsSaySoRatherThanSucceed)
    {
        HandlerRig<ExecutionHandlers>  rig;
        Reply                          reply;



        reply = rig.Run ("P-");
        Assert::IsTrue (reply.status == CommandStatus::NotAvailable, L"no requester: not available");

        rig.session.SetReverseRequester ([] (ReverseCommand) { return false; });

        reply = rig.Run ("G-");
        Assert::IsTrue (reply.status == CommandStatus::NotAvailable, L"a requester that took nothing: not available");
        Assert::IsTrue (reply.error.detail.find ("Reverse execution is off") != std::string::npos, ToWide (reply.error.detail).c_str());
    }


private:

    static std::wstring ToWide (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }
};

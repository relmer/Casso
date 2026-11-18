#include "Pch.h"

#include "Debugger/Reverse/ReverseHost.h"

#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "Core/TextEncoding.h"

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



        rig.session.SetReverseRequester ([&taken] (ReverseCommand command, uint64_t) { taken.push_back (command); return true; });

        for (const Case & each : kCases)
        {
            taken.clear();
            reply = rig.session.ExecuteLine (each.line, each.mode);

            Assert::IsTrue (reply.status == CommandStatus::Ok, std::format (L"{} ran", ToWide (each.line)).c_str());
            Assert::AreEqual<size_t> (1, taken.size(), std::format (L"{} reached the requester once", ToWide (each.line)).c_str());
            Assert::IsTrue (taken.front() == each.expected, std::format (L"{} is the matching reverse command", ToWide (each.line)).c_str());
        }
    }


    //  SEEK goes to a time back from live, a cycle or an instruction, as a
    //  click on the history band does, with counts written as they appear in
    //  the debugger; anything else is an error.
    TEST_METHOD (SeekGoesToATimeACycleOrAPosition)
    {
        struct Case
        {
            const char      * line;
            ReverseCommand    expected;
            uint64_t          argument;
        };

        static constexpr Case  kCases[] =
        {
            { "SEEK -5s",                          ReverseCommand::SeekBack,  5000       },
            { "seek -250ms",                       ReverseCommand::SeekBack,  250        },
            { "SEEK CYCLE 646218",                 ReverseCommand::SeekCycle, 646218     },
            { "SEEK pos 17",                       ReverseCommand::Seek,      17         },
            { "SEEK INSTR 17",                     ReverseCommand::Seek,      17         },
            { "SEEK CYCLE 1,309,300,000",          ReverseCommand::SeekCycle, 1309300000 },
            { "SEEK CYCLE 0n646218",               ReverseCommand::SeekCycle, 646218     },
            { "SEEK CYCLE Cycles 1,309,300,000",   ReverseCommand::SeekCycle, 1309300000 },
        };

        HandlerRig<ExecutionHandlers>                       rig;
        std::vector<std::pair<ReverseCommand, uint64_t>>    taken;
        Reply                                               reply;



        rig.session.SetReverseRequester ([&taken] (ReverseCommand command, uint64_t argument) { taken.push_back ({ command, argument }); return true; });

        for (const Case & each : kCases)
        {
            taken.clear();
            reply = rig.session.ExecuteLine (each.line, CommandMode::AppleWin);

            Assert::IsTrue (reply.status == CommandStatus::Ok, ToWide (each.line).c_str());
            Assert::AreEqual<size_t> (1, taken.size(), ToWide (each.line).c_str());
            Assert::IsTrue (taken.front().first == each.expected, ToWide (each.line).c_str());
            Assert::AreEqual (each.argument, taken.front().second, ToWide (each.line).c_str());
        }

        for (const char * bad : { "SEEK", "SEEK 5s", "SEEK -5m", "SEEK CYCLE", "SEEK POS x1", "SEEK INSTR",
                                  "SEEK CYCLE 99999999999999999999999", "SEEK -99999999999999999999s", "SEEK -18446744073709552s" })
        {
            taken.clear();
            reply = rig.session.ExecuteLine (bad, CommandMode::AppleWin);

            Assert::IsTrue (reply.status != CommandStatus::Ok, ToWide (bad).c_str());
            Assert::IsTrue (taken.empty(), ToWide (bad).c_str());
        }

        Assert::AreEqual ((uint64_t) (2'000'000 - 1'020'484), ReverseHost::GetCycleBack (2'000'000, 1000), L"a second back");
        Assert::AreEqual ((uint64_t) 0, ReverseHost::GetCycleBack (1000, 1000), L"past the start");
        Assert::AreEqual ((uint64_t) 0, ReverseHost::GetCycleBack (1000, UINT64_MAX), L"longer than 64 bits of cycles");
    }


    TEST_METHOD (ARunningMachineIsNotMovedThroughHistory)
    {
        HandlerRig<ExecutionHandlers>  rig;
        int                            calls = 0;
        Reply                          reply;



        rig.session.SetReverseRequester ([&calls] (ReverseCommand, uint64_t) { ++calls; return true; });
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

        rig.session.SetReverseRequester ([] (ReverseCommand, uint64_t) { return false; });

        reply = rig.Run ("G-");
        Assert::IsTrue (reply.status == CommandStatus::NotAvailable, L"a requester that took nothing: not available");
        Assert::IsTrue (reply.error.detail.find ("Reverse execution is off") != std::string::npos, ToWide (reply.error.detail).c_str());
    }


private:

    static std::wstring ToWide (const std::string & text)
    {
        return TextEncoding::NarrowToWide (text);
    }
};

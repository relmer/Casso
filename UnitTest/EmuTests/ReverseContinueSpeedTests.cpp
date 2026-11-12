#include "Pch.h"

#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Reverse/ReverseStopTest.h"
#include "EmuTests/ReverseSessionRig.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseContinueSpeedTests
//
//  How long a reverse continue takes to go back through history to a
//  breakpoint hit only at the start of the recording, at the default
//  keyframe interval.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseContinueSpeedTests)
{
public:

    TEST_METHOD (MeasureReverseContinueThroughHistory)
    {
        static constexpr uint64_t  kSeconds           = 10;
        static constexpr uint64_t  kCyclesPerSecond   = 1'023'000;
        static constexpr uint64_t  kFramesPerKeyframe = 10;

        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        ReverseProbe       probe;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           startedAt  = 0;
        uint64_t           elapsed    = 0;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (kFramesPerKeyframe));
        AssertSucceeded (hr, L"Start");

        machine.GetRefs().iieSoftSwitches->SetPaddle (0, 0);
        machine.RunCycles (kSeconds * kCyclesPerSecond);

        probe.breakPc = ReverseSessionRig::kProgramStart + 3;

        startedAt = GetTickCount64();
        hr        = controller.ReverseContinue (probe, result);
        elapsed   = GetTickCount64() - startedAt;
        AssertSucceeded (hr, L"ReverseContinue");

        Logger::WriteMessage (std::format ("{} s of history: reverse continue took {} ms, outcome {}, landed at position {}\n",
                                           kSeconds, elapsed, (int) result.outcome, result.position).c_str());
    }


    //  The guest writes to the disk once, then counts in a loop; reverse
    //  continue from ten seconds later goes back to the write.
    TEST_METHOD (MeasureReverseContinueToADiskWrite)
    {
        static constexpr uint64_t  kSeconds           = 10;
        static constexpr uint64_t  kCyclesPerSecond   = 1'023'000;
        static constexpr uint64_t  kFramesPerKeyframe = 10;
        static constexpr Byte      kWriteOnce[] =
        {
            0xAD, 0xE9, 0xC0,       // 0800  LDA $C0E9    motor on
            0xAD, 0xEA, 0xC0,       // 0803  LDA $C0EA    drive 1
            0x20, 0x00, 0x09,       // 0806  JSR $0900    the rig's write routine
            0xEE, 0x04, 0x03,       // 0809  INC $0304
            0x4C, 0x09, 0x08,       // 080C  JMP $0809
        };

        MachineHandlerRig<BreakpointHandlers>  rig        { "Apple2e", TestMachine::Slots::AsShipped };
        ReverseController                      controller { rig.machine };
        ReverseStopTest                        stopTest   { rig.session };
        ReverseResult                          result;
        HRESULT                                hr         = S_OK;
        uint64_t                               startedAt  = 0;
        uint64_t                               elapsed    = 0;



        ReverseSessionRig::Prepare (rig.machine);

        for (size_t i = 0; i < sizeof (kWriteOnce); i++)
        {
            rig.machine.GetMemoryBus().WriteByte (static_cast<Word> (ReverseSessionRig::kProgramStart + i), kWriteOnce[i]);
        }

        hr = controller.Start (ReverseSessionRig::MakeSettings (kFramesPerKeyframe));
        AssertSucceeded (hr, L"Start");

        rig.machine.RunCycles (kSeconds * kCyclesPerSecond);
        rig.RunOk ("BPDISK WRITE");

        startedAt = GetTickCount64();
        hr        = controller.ReverseContinue (stopTest, result);
        elapsed   = GetTickCount64() - startedAt;
        AssertSucceeded (hr, L"ReverseContinue");

        Logger::WriteMessage (std::format ("{} s of history: reverse continue to a disk write took {} ms, outcome {}, PC ${:04X}\n",
                                           kSeconds, elapsed, (int) result.outcome, rig.machine.GetCpu()->GetPC()).c_str());

        Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"the write is found");
    }

    //  The routine at $0900 runs once at the start and the guest then loops
    //  on page 8, so every stretch after the first is passed over unreplayed.
    TEST_METHOD (ReverseContinuePassesOverStretchesThatCannotStop)
    {
        static constexpr uint64_t  kSeconds           = 10;
        static constexpr uint64_t  kCyclesPerSecond   = 1'023'000;
        static constexpr uint64_t  kFramesPerKeyframe = 10;
        static constexpr Byte      kCallOnce[] =
        {
            0x20, 0x00, 0x09,       // 0800  JSR $0900
            0xEE, 0x04, 0x03,       // 0803  INC $0304
            0x4C, 0x03, 0x08,       // 0806  JMP $0803
        };

        MachineHandlerRig<BreakpointHandlers>  rig        { "Apple2e", TestMachine::Slots::AsShipped };
        ReverseController                      controller { rig.machine };
        ReverseStopTest                        stopTest   { rig.session };
        ReverseResult                          result;
        HRESULT                                hr         = S_OK;
        uint64_t                               startedAt  = 0;
        uint64_t                               elapsed    = 0;



        ReverseSessionRig::Prepare (rig.machine);

        for (size_t i = 0; i < sizeof (kCallOnce); i++)
        {
            rig.machine.GetMemoryBus().WriteByte (static_cast<Word> (ReverseSessionRig::kProgramStart + i), kCallOnce[i]);
        }

        hr = controller.Start (ReverseSessionRig::MakeSettings (kFramesPerKeyframe));
        AssertSucceeded (hr, L"Start");

        rig.machine.RunCycles (kSeconds * kCyclesPerSecond);
        rig.RunOk ("BP 0900");

        startedAt = GetTickCount64();
        hr        = controller.ReverseContinue (stopTest, result);
        elapsed   = GetTickCount64() - startedAt;
        AssertSucceeded (hr, L"ReverseContinue");

        Logger::WriteMessage (std::format ("{} s of history: reverse continue past idle stretches took {} ms\n", kSeconds, elapsed).c_str());

        Assert::IsTrue   (result.outcome == ReverseOutcome::Moved, L"the call is found");
        Assert::AreEqual ((Word) 0x0900, rig.machine.GetCpu()->GetPC(), L"at the routine");
    }
};

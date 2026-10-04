#include "Pch.h"

#include "Shell/FrameCycleBudget.h"
#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FrameTimingTests
//
//  The beam position against the CPU's cycle count, and the frame loop's
//  cycle budget against the nominal cycles per frame.
//
//  The beam is the cycle count broken into a frame: 17,030 cycles, 262
//  scanlines of 65. Anything that moves one without the other puts the beam
//  the debugger shows somewhere the machine is not.
//
//  The frame loop runs whole instructions, so each pass ends a few cycles past
//  its target. Dropped rather than repaid, those cycles made every frame run
//  long and the beam at each frame boundary creep forward. Each pass now runs
//  to the next frame boundary, which repays them and also finishes a frame a
//  stop cut short.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FrameTimingTests)
{
public:

    //  Every shipped machine, run in uneven chunks with a soft reset partway,
    //  shows the beam the cycle count puts it at.
    TEST_METHOD (BeamMatchesTheCycleCountOnEveryMachine)
    {
        static constexpr const char *  kMachines[] = { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c" };
        static constexpr uint64_t      kChunks[]   = { 1, 17029, 65, 40000, 3, 123457, 17030, 999 };
        static constexpr size_t        kResetAt    = 4;

        size_t  checked = 0;



        for (const char * machineId : kMachines)
        {
            TestMachine   machine (machineId, TestMachine::Slots::Empty);
            std::wstring  wideId  (machineId, machineId + strlen (machineId));



            Assert::IsNotNull (machine.GetVideoTiming(), wideId.c_str());

            for (size_t i = 0; i < std::size (kChunks); i++)
            {
                if (i == kResetAt)
                {
                    machine.SoftReset();
                }

                machine.RunCycles (kChunks[i]);

                AssertBeamMatchesCycles (machine, wideId);
                checked++;
            }
        }

        Assert::AreEqual (std::size (kMachines) * std::size (kChunks), checked);
    }



    //  Many passes of the frame loop, each in the loop's own slices, spend
    //  exactly the nominal cycles per pass between them, give or take the one
    //  instruction the last pass ran over, and each ends at the top of a frame.
    TEST_METHOD (PassesSpendTheNominalCyclesInTotal)
    {
        static constexpr uint32_t  kPasses = 600;

        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);
        uint64_t     start   = 0;
        uint64_t     spent   = 0;
        uint64_t     nominal = (uint64_t) kNominal * kPasses;



        RunPass (machine, kNominal);
        start = machine.GetCpu()->GetTotalCycles();

        for (uint32_t pass = 0; pass < kPasses; pass++)
        {
            RunPass (machine, kNominal);
            AssertAtFrameTop (machine, std::format (L"pass {}", pass));
        }

        spent = machine.GetCpu()->GetTotalCycles() - start;

        Assert::IsTrue (spent + kLongestInstruction > nominal, std::format (L"{} cycles over {} passes, short of {}", spent, kPasses, nominal).c_str());
        Assert::IsTrue (spent < nominal + kLongestInstruction, std::format (L"{} cycles over {} passes, {} past {}", spent, kPasses, spent - nominal, nominal).c_str());
    }



    //  A stop partway through a frame -- a breakpoint, a pause, a step --
    //  leaves the next pass to finish that frame rather than run a whole one
    //  from wherever the machine stopped.
    TEST_METHOD (PassAfterAStopFinishesTheFrame)
    {
        static constexpr uint64_t  kStoppedAfter = 5000;

        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);
        uint64_t     frame   = 0;



        RunPass (machine, kNominal);
        frame = machine.GetCpu()->GetTotalCycles() / kNominal;

        machine.RunCycles (kStoppedAfter);
        RunPass (machine, kNominal);

        AssertAtFrameTop (machine, L"after the stop");
        Assert::AreEqual (frame + 1, machine.GetCpu()->GetTotalCycles() / kNominal, L"the pass finished the frame the stop was in");
    }



    //  Double speed passes two frames at once, and its passes still end on a
    //  frame boundary.
    TEST_METHOD (DoubleSpeedPassesEndAtTheTopOfAFrame)
    {
        static constexpr uint32_t  kPasses = 30;

        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);



        machine.RunCycles (kNominal / 3);

        for (uint32_t pass = 0; pass < kPasses; pass++)
        {
            RunPass (machine, kNominal * 2);
            AssertAtFrameTop (machine, std::format (L"double-speed pass {}", pass));
        }
    }



private:

    static constexpr uint32_t  kNominal            = VideoTiming::kCyclesPerFrame;
    static constexpr uint64_t  kLongestInstruction = 7;

    static void AssertBeamMatchesCycles (TestMachine & machine, const std::wstring & machineId)
    {
        uint64_t  total   = machine.GetCpu()->GetTotalCycles();
        uint32_t  inFrame = (uint32_t) (total % VideoTiming::kCyclesPerFrame);



        Assert::AreEqual (inFrame,                                     machine.GetVideoTiming()->GetCycleInFrame(),    std::format (L"{}: cycle in frame at {} cycles", machineId, total).c_str());
        Assert::AreEqual (inFrame / VideoTiming::kCyclesPerScanline,   machine.GetVideoTiming()->GetCurrentScanline(), std::format (L"{}: scanline at {} cycles",       machineId, total).c_str());
        Assert::AreEqual (inFrame % VideoTiming::kCyclesPerScanline,   machine.GetVideoTiming()->GetHorizontalPos(),   std::format (L"{}: cycle in line at {} cycles",  machineId, total).c_str());
    }



    static void AssertAtFrameTop (TestMachine & machine, const std::wstring & when)
    {
        uint32_t  intoFrame = machine.GetVideoTiming()->GetCycleInFrame();



        Assert::IsTrue (intoFrame < kLongestInstruction, std::format (L"{}: {} cycles into the frame", when, intoFrame).c_str());
    }



    //  One pass as ExecuteCpuSlices runs it: the budget's target, in slices.
    static void RunPass (TestMachine & machine, uint32_t nominal)
    {
        uint32_t  target   = FrameCycleBudget::GetTarget (nominal, machine.GetCpu()->GetTotalCycles());
        uint32_t  executed = 0;
        uint32_t  slice    = 0;



        while (executed < target)
        {
            slice     = std::min (target - executed, FrameCycleBudget::kSliceCycles);
            executed += (uint32_t) machine.RunCycles (slice);
        }
    }
};
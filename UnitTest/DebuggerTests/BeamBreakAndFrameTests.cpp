#include "Pch.h"

#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "HandlerTestRig.h"
#include "Machines/Apple2/Common/VideoTiming.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakAndFrameTests
//
//  BPBEAM stops where the beam reaches a scanline and cycle, or where
//  vertical blank starts; FRAME runs one whole frame, or a count of them.
//  The break is checked against the mock's beam and FRAME against a real
//  machine run synchronously.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (BeamBreakAndFrameTests)
    {
    public:

        using Rig        = HandlerRig<ExecutionHandlers>;
        using MachineRig = MachineHandlerRig<ExecutionHandlers>;



        TEST_METHOD (BPBEAM_SetsABreakAndSaysWhere)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Breakpoint set at video scanline $10, cycle $20. It clears after it fires."), rig.RunOk ("BPBEAM 10 20").text.at (0));
            Assert::IsTrue   (rig.session.HasBeamBreak());
            Assert::IsTrue   (rig.target.hookInstalled);

            Assert::AreEqual (std::string ("Breakpoint set at the start of vertical blank, scanline $C0. It clears after it fires."), rig.RunOk ("BPBEAM vbl").text.at (0));
        }



        TEST_METHOD (BPBEAM_RejectsAPlaceOutsideTheFrame)
        {
            Rig  rig;



            rig.RunFails ("BPBEAM 106 0", "invalid arguments");
            rig.RunFails ("BPBEAM 10 41", "invalid arguments");
            Assert::IsFalse (rig.session.HasBeamBreak());
            Assert::AreNotEqual ((int) CommandStatus::Ok, (int) rig.Run ("BPBEAM 10").status, L"a scanline needs its cycle");
            Assert::AreNotEqual ((int) CommandStatus::Ok, (int) rig.Run ("BPBEAM").status);
        }



        //  An instruction moves the beam several cycles, so passing the
        //  target stops as reaching it exactly does.
        TEST_METHOD (BPBEAM_StopsWhenTheBeamPassesTheTarget)
        {
            Rig  rig;



            rig.target.videoPosition = { 0x10, 0 };
            rig.RunOk ("BPBEAM 10 20");

            Assert::IsFalse (rig.session.ShouldStopBefore (0x0300), L"the beam has not moved");
            rig.target.videoPosition = { 0x10, 0x1F };
            Assert::IsFalse (rig.session.ShouldStopBefore (0x0300), L"one cycle short");
            rig.target.videoPosition = { 0x10, 0x22 };
            Assert::IsTrue  (rig.session.ShouldStopBefore (0x0300), L"past the target");

            rig.target.Stop (StopEvent { StopReason::Breakpoint, 0x0300 });
            Assert::IsFalse (rig.session.HasBeamBreak(), L"a beam break clears when it fires");
            Assert::IsFalse (rig.target.hookInstalled);
        }



        //  A target behind the beam is reached only after the frame wraps.
        TEST_METHOD (BPBEAM_ReachesATargetBehindTheBeamAcrossTheWrap)
        {
            Rig  rig;



            rig.target.videoPosition = { 200, 0 };
            rig.RunOk ("BPBEAM 5 0");

            rig.target.videoPosition = { 261, 64 };
            Assert::IsFalse (rig.session.ShouldStopBefore (0x0300));
            rig.target.videoPosition = { 0, 3 };
            Assert::IsFalse (rig.session.ShouldStopBefore (0x0300), L"wrapped, but not yet at scanline 5");
            rig.target.videoPosition = { 5, 1 };
            Assert::IsTrue  (rig.session.ShouldStopBefore (0x0300));
        }



        TEST_METHOD (BPBEAM_VblStopsWhereBlankingStarts)
        {
            Rig  rig;



            rig.target.videoPosition = { 191, 60 };
            rig.RunOk ("BPBEAM VBL");

            rig.target.videoPosition = { 191, 64 };
            Assert::IsFalse (rig.session.ShouldStopBefore (0x0300));
            rig.target.videoPosition = { 192, 2 };
            Assert::IsTrue  (rig.session.ShouldStopBefore (0x0300));
        }



        TEST_METHOD (BPBEAM_ClearedByClearAll)
        {
            Rig  rig;



            rig.RunOk ("BPBEAM 10 0");
            rig.session.ClearAllBreakpoints();
            Assert::IsFalse (rig.session.HasBeamBreak());
            Assert::IsFalse (rig.target.hookInstalled);
        }



        //  One frame is 17,030 cycles; the stop is on the first instruction
        //  at or past the place the beam started, so it overshoots by less
        //  than an instruction.
        TEST_METHOD (FRAME_RunsOneWholeFrame)
        {
            static constexpr uint64_t  kLongestInstruction = 7;
            MachineRig                 rig;
            uint64_t                   before              = 0;
            uint64_t                   ran                 = 0;



            // $0300: INX / JMP $0300
            rig.Load (0x0300, { 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);

            rig.RunOk ("BUDGET 100000");
            before = rig.target.GetCycleCount();
            rig.RunOk ("FRAME");
            ran = rig.target.GetCycleCount() - before;

            Assert::IsTrue  (ran >= VideoTiming::kCyclesPerFrame, std::to_wstring (ran).c_str());
            Assert::IsTrue  (ran <  VideoTiming::kCyclesPerFrame + kLongestInstruction, std::to_wstring (ran).c_str());
            Assert::IsFalse (rig.session.HasBeamBreak(), L"the frame break goes with the stop");
        }



        TEST_METHOD (FRAME_CountRunsThatManyFrames)
        {
            static constexpr uint64_t  kLongestInstruction = 7;
            MachineRig                 rig;
            uint64_t                   before              = 0;
            uint64_t                   ran                 = 0;



            rig.Load (0x0300, { 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);

            rig.RunOk ("BUDGET 100000");
            before = rig.target.GetCycleCount();
            rig.RunOk ("FRAME 3");
            ran = rig.target.GetCycleCount() - before;

            Assert::IsTrue (ran >= 3 * VideoTiming::kCyclesPerFrame, std::to_wstring (ran).c_str());
            Assert::IsTrue (ran <  3 * VideoTiming::kCyclesPerFrame + kLongestInstruction, std::to_wstring (ran).c_str());
            Assert::AreNotEqual ((int) CommandStatus::Ok, (int) rig.Run ("FRAME 0").status, L"a count of zero runs nothing");
        }



        //  A breakpoint met on the way stops the frame early.
        TEST_METHOD (FRAME_StopsEarlyAtABreakpoint)
        {
            MachineRig          rig;
            BreakpointHandlers  breakpoints;
            uint64_t            before = 0;



            rig.session.AddHandler (&breakpoints);

            // $0300: INX / BNE $0300 / JMP $0310    $0310: NOP
            rig.Load (0x0300, { 0xE8, 0xD0, 0xFD, 0x4C, 0x10, 0x03 }, 0x0300);
            rig.Load (0x0310, { 0xEA }, 0x0300);
            rig.RunOk ("BP 310");

            rig.RunOk ("BUDGET 100000");
            before = rig.target.GetCycleCount();
            rig.RunOk ("FRAME");

            Assert::AreEqual ((int) 0x0310, (int) rig.target.GetRegisters().pc);
            Assert::IsTrue   (rig.target.GetCycleCount() - before < VideoTiming::kCyclesPerFrame);
            Assert::IsFalse  (rig.session.HasBeamBreak(), L"the frame break goes with the stop");
        }
    };
}

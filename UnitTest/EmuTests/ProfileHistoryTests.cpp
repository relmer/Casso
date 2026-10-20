#include "Pch.h"

#include "HResultAssert.h"
#include "TestMachine.h"
#include "Core/TextEncoding.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Shell/DebugEventLogs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ProfileHistoryTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ProfileRig
    //
    //  An enhanced //e recording history from its start, run long enough for
    //  history to have many stretches; the machine counts as stopped, so the
    //  stretch since the newest keyframe is replayed too.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ProfileRig
    {
    public:
        static constexpr uint64_t  kRunCycles = 3'000'000;

        TestMachine        machine    { "Apple2eEnhanced" };
        ReverseController  controller { machine };
        DebugEventLogs     logs       { machine };



        ProfileRig()
        {
            HRESULT  hr = S_OK;



            hr = controller.Start (ReverseSettings());
            AssertSucceeded (hr, L"Start");

            logs.Start (&controller);
            logs.GetProfileHistory().SetIsStopped ([] { return true; });

            machine.RunCycles (kRunCycles);

            hr = controller.GetKeyframes().WaitForPending();
            AssertSucceeded (hr, L"WaitForPending");
        }



        ~ProfileRig()
        {
            logs.Stop();
            controller.Stop();
        }



        //  The finished counts of a build from fromCycle.
        ProfileCountsPtr  BuildFrom (uint64_t fromCycle)
        {
            ProfileCountsPtr  counts;
            std::string       error;
            HRESULT           hr     = S_OK;



            hr = logs.GetProfileHistory().Build (fromCycle, error);
            AssertSucceeded (hr, TextEncoding::NarrowToWide (error).c_str());

            logs.GetProfileHistory().WaitForWork();
            Assert::IsTrue (logs.GetProfileHistory().Pump (counts), L"the build did not finish");
            return counts;
        }
    };





    TEST_CLASS (ProfileHistoryTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////////////
        //
        //  AllOfHistoryIsProfiled
        //
        //  A build from the first cycle replays every stretch to the machine's
        //  position: its cycles are the whole run's, within the cycles of the
        //  instruction under way, and it has routines.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AllOfHistoryIsProfiled)
        {
            ProfileRig        rig;
            ProfileCountsPtr  counts = rig.BuildFrom (0);
            uint64_t          first  = rig.controller.GetKeyframes().GetInfo (0).cycle;
            uint64_t          span   = rig.machine.GetCpu()->GetTotalCycles() - first;



            Assert::IsTrue (counts != nullptr);
            Assert::IsTrue (counts->table.GetInstructionCount() > 0, L"no instructions");
            Assert::IsTrue (counts->table.GetTotalCycles() + 8 >= span && counts->table.GetTotalCycles() <= span + 8, std::format (L"{} cycles counted of {}", counts->table.GetTotalCycles(), span).c_str());
            Assert::IsFalse (counts->routines.GetRoutines().empty(), L"no routines");
            Assert::IsFalse (rig.logs.GetProfileHistory().GetProgress().has_value(), L"still building");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  APeriodCountsOnlyItsOwnInstructions
        //
        //  A build of the last stretch of cycles counts only the instructions
        //  that start in it, though the replay starts at an earlier keyframe.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (APeriodCountsOnlyItsOwnInstructions)
        {
            constexpr uint64_t  kPeriod = 500'000;
            ProfileRig          rig;
            uint64_t            now     = rig.machine.GetCpu()->GetTotalCycles();
            ProfileCountsPtr    counts  = rig.BuildFrom (now - kPeriod);



            Assert::IsTrue (counts->table.GetTotalCycles() + 8 >= kPeriod && counts->table.GetTotalCycles() <= kPeriod + 8,
                            std::format (L"{} cycles counted of {}", counts->table.GetTotalCycles(), kPeriod).c_str());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BlocksAddUpAndAreReused
        //
        //  Cut into many blocks, replayed at once, the history's counts add up
        //  to the whole run's, the frames begin at its start, and a second
        //  build replays only the blocks not in the cache, to the same counts.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BlocksAddUpAndAreReused)
        {
            ProfileRig                          rig;
            std::vector<ProfileHistory::Block>  blocks;
            ProfileCountsPtr                    counts;
            ProfileCountsPtr                    again;
            uint64_t                            first  = rig.controller.GetKeyframes().GetInfo (0).cycle;
            uint64_t                            span   = rig.machine.GetCpu()->GetTotalCycles() - first;



            rig.logs.GetProfileHistory().SetBlockPositions (100'000);
            rig.logs.GetProfileHistory().GetBlocks (0, blocks);
            Assert::IsTrue (blocks.size() > 2, std::format (L"{} blocks", blocks.size()).c_str());

            counts = rig.BuildFrom (0);
            Assert::IsTrue (counts->table.GetTotalCycles() + 8 >= span && counts->table.GetTotalCycles() <= span + 8, std::format (L"{} cycles counted of {}", counts->table.GetTotalCycles(), span).c_str());
            Assert::IsTrue (counts->routines.GetFrames().front().index <= first / RoutineProfile::kCyclesPerFrame + 1, L"the frames begin at history's start");

            again = rig.BuildFrom (0);
            Assert::IsTrue   (rig.logs.GetProfileHistory().GetReusedCount() > 0, L"no block was cached");
            Assert::AreEqual (counts->table.GetTotalCycles(),      again->table.GetTotalCycles());
            Assert::AreEqual (counts->table.GetInstructionCount(), again->table.GetInstructionCount());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  NoHistoryIsAnError
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (NoHistoryIsAnError)
        {
            TestMachine     machine { "Apple2eEnhanced" };
            DebugEventLogs  logs    { machine };
            std::string     error;
            HRESULT         hr      = S_OK;



            hr = logs.GetProfileHistory().Build (0, error);

            Assert::IsTrue  (FAILED (hr));
            Assert::IsFalse (error.empty());
        }
    };
}

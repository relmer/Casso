#include "Pch.h"

#include "Debugger/CountedStepRunner.h"
#include "MockDebugTarget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CountedStepRunnerTests
//
//  A counted step over runs as single steps, each started as the last one
//  ends, and the stop that ends the count has every step's cycles.
//
////////////////////////////////////////////////////////////////////////////////

namespace CountedStepRunnerTests
{
    class FakeOwner : public ICountedStepOwner
    {
    public:
        int                     pauses    = 0;
        std::vector<uint32_t>   started;
        std::vector<StopEvent>  delivered;
        bool                    isStartOk = true;

        void  PauseForNextStep() override                          { pauses++; }
        bool  StartStep        (const DebugCommand & step) override { started.push_back (step.count); return isStartOk; }
        void  DeliverStop      (const StopEvent & stop) override    { delivered.push_back (stop); }
    };


    static StopEvent MakeStep (uint64_t cycles)
    {
        StopEvent  stop;



        stop.reason = StopReason::Step;
        stop.cycles = cycles;
        return stop;
    }



    TEST_CLASS (CountedStepRunnerTests)
    {
    public:
        //  A step over with a count of three runs one step now, and two more.
        TEST_METHOD (Arm_TakesACountedStepOver)
        {
            FakeOwner          owner;
            CountedStepRunner  runner (owner);
            DebugCommand       command;
            RunRequest         request;
            StopEvent          stop;



            request.kind  = RunKind::StepOver;
            request.count = 3;
            runner.Arm (command, request);
            Assert::AreEqual (1u, request.count, L"the request runs one step");

            stop = MakeStep (10);
            Assert::IsTrue  (runner.TryContinue (stop), L"the second step starts");
            stop = MakeStep (20);
            Assert::IsTrue  (runner.TryContinue (stop), L"the third step starts");
            stop = MakeStep (30);
            Assert::IsFalse (runner.TryContinue (stop), L"the count is over");

            Assert::AreEqual ((size_t) 2, owner.started.size());
            Assert::AreEqual (1u, owner.started[0], L"each step is a single step");
            Assert::AreEqual (2, owner.pauses);
            Assert::AreEqual ((uint64_t) 60, stop.cycles, L"the last stop has every step's cycles");
        }


        //  Instructions counted by the run stop hook aren't taken.
        TEST_METHOD (Arm_LeavesAnInstructionCount)
        {
            FakeOwner          owner;
            CountedStepRunner  runner (owner);
            DebugCommand       command;
            RunRequest         request;
            StopEvent          stop  = MakeStep (5);



            request.kind  = RunKind::StepInto;
            request.count = 4;
            runner.Arm (command, request);

            Assert::AreEqual (4u, request.count);
            Assert::IsFalse  (runner.TryContinue (stop));
            Assert::IsTrue   (owner.started.empty());
        }


        //  A step's stop that comes while its run is starting is pending,
        //  and Begin starts the next step once the run returns.
        TEST_METHOD (Begin_RunsAPendingStep)
        {
            FakeOwner          owner;
            CountedStepRunner  runner (owner);
            MockDebugTarget    target;
            DebugCommand       command;
            RunRequest         request;
            StopEvent          stop  = MakeStep (7);
            HRESULT            hr    = S_OK;



            request.kind  = RunKind::StepOver;
            request.count = 2;
            runner.Arm (command, request);

            target.onStartRun = [&] (const RunRequest &) { Assert::IsTrue (runner.TryContinue (stop), L"pending"); };

            hr = runner.Begin (target, request);

            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual ((size_t) 1, owner.started.size(), L"the pending step started after the run returned");
            Assert::IsTrue   (owner.delivered.empty());
        }


        //  A next step that doesn't start ends the count with the last
        //  step's stop.
        TEST_METHOD (Begin_DeliversTheStopWhenTheNextStepFails)
        {
            FakeOwner          owner;
            CountedStepRunner  runner (owner);
            MockDebugTarget    target;
            DebugCommand       command;
            RunRequest         request;
            StopEvent          stop  = MakeStep (7);



            owner.isStartOk = false;
            request.kind    = RunKind::StepOver;
            request.count   = 2;
            runner.Arm (command, request);

            target.onStartRun = [&] (const RunRequest &) { (void) runner.TryContinue (stop); };

            AssertSucceeded (runner.Begin (target, request));

            Assert::AreEqual ((size_t) 1, owner.delivered.size());
            Assert::AreEqual ((uint64_t) 0, owner.delivered[0].cycles);
        }
    };
}

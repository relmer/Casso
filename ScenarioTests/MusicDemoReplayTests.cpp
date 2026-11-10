#include "Pch.h"

#include "GuestSession.h"
#include "HResultAssert.h"
#include "TestMachine.h"
#include "Core/TextEncoding.h"
#include "Debugger/Reverse/ReverseController.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MusicDemoReplayTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MusicDemoReplayTests
    //
    //  The Mockingboard music demo, booted on an Apple //e Enhanced under
    //  history with the owner's keyframe interval, run, taken back, and run on
    //  from there. Running on from the past replays the recorded future, and
    //  every keyframe on the way must match, so the run gets to the end of
    //  history with no keyframe dropped. A replay that diverges cuts history,
    //  and the failure message has the state section it differed in.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MusicDemoReplayTests)
    {
    public:

        static constexpr uint64_t  s_kFrameCycles     = KeyframeSettings::kFrameCycles;
        static constexpr uint64_t  s_kFramesPerSecond = 60;
        static constexpr uint64_t  s_kIntervalFrames  = 10;
        static constexpr size_t    s_kBudgetBytes     = 256ull * 1024 * 1024;



        static void RunSeconds (TestMachine & machine, uint64_t seconds)
        {
            for (uint64_t frame = 0; frame < seconds * s_kFramesPerSecond; frame++)
            {
                machine.RunCycles (s_kFrameCycles);
            }
        }



        TEST_METHOD (RunningOnFromThePastKeepsHistory)
        {
            TestMachine        machine    ("Apple2eEnhanced");
            ReverseController  controller (machine);
            ReverseSettings    settings;
            ReverseResult      result;
            HRESULT            hr         = S_OK;
            uint64_t           middle     = 0;
            size_t             keyframes  = 0;



            settings.keyframes.intervalCycles = s_kFrameCycles * s_kIntervalFrames;
            settings.keyframes.budgetBytes    = s_kBudgetBytes;

            machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
            GuestSession::Mount (machine, GuestSession::RequireRepoImage ("Apple2/Demos/mockingboard-music.dsk"));

            hr = controller.Start (settings);
            AssertSucceeded (hr, L"Start");

            machine.PowerCycle();
            RunSeconds (machine, 3);
            middle = machine.GetCpu()->GetTotalCycles();
            RunSeconds (machine, 3);

            hr = controller.GetKeyframes().WaitForPending();
            AssertSucceeded (hr, L"WaitForPending");

            keyframes = controller.GetKeyframes().GetCount();

            hr = controller.SeekToCycle (middle, result);
            AssertSucceeded (hr, L"SeekToCycle");
            Assert::IsTrue (controller.IsInHistory(), L"the seek is behind live");

            RunSeconds (machine, 4);

            Assert::AreEqual ((size_t) 0, controller.GetCutCount(),
                              TextEncoding::NarrowToWide ("history was cut: " + controller.GetLastDivergence()).c_str());
            Assert::IsFalse  (controller.IsInHistory(), L"the run got to the end of history");
            Assert::IsTrue   (controller.GetKeyframes().GetCount() >= keyframes, L"no keyframe of the recording was dropped");
        }
    };
}

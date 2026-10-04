#include "Pch.h"

#include "Debugger/CpuManagerRunDriver.h"
#include "Debugger/IRunObserver.h"
#include "Debugger/MachineDebugTarget.h"
#include "EmuTests/TestMachine.h"
#include "Shell/CpuManager.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StepSoundTests
    //
    //  Sound is silent while the machine executes one instruction at a time
    //  (step into, trace) and plays whenever it runs code, step over and step
    //  out included. The frame loop drops the speaker's output whenever the
    //  run driver reports a silent run, so the driver's answer is the rule.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StepSoundTests)
    {
    public:

        class NullObserver : public IRunObserver
        {
        public:
            void OnStopped (const StopEvent &) override {}
        };



        class Rig
        {
        public:
            TestMachine          machine;
            MachineDebugTarget   target;
            CpuManager           cpuManager;
            CpuManagerRunDriver  driver;
            NullObserver         observer;



            Rig() :
                machine (std::string ("Apple2e"), TestMachine::Slots::Empty),
                target  (machine),
                driver  (machine, cpuManager, target.GetRunHook())
            {
                driver.SetRunObserver (&observer);
            }

            void Start (RunKind kind)
            {
                RunRequest  request;
                HRESULT     hr = S_OK;



                request.kind = kind;
                hr           = driver.Start (request);
                Assert::IsTrue (SUCCEEDED (hr), L"the run started");
            }
        };



        TEST_METHOD (SingleInstructionStepIsSilent)
        {
            for (RunKind kind : { RunKind::StepInto, RunKind::Trace })
            {
                Rig  rig;



                rig.Start (kind);
                Assert::IsTrue (rig.driver.IsSilent(), L"a single-instruction step plays no sound");
            }
        }

        TEST_METHOD (ARunIsHeard)
        {
            for (RunKind kind : { RunKind::Go, RunKind::RunTo, RunKind::StepOver, RunKind::StepOut })
            {
                Rig  rig;



                rig.Start (kind);
                Assert::IsFalse (rig.driver.IsSilent(), L"a run plays sound");
            }
        }

        TEST_METHOD (SoundResumesWhenTheStepEnds)
        {
            Rig  rig;



            rig.Start (RunKind::StepInto);
            rig.driver.Pause();
            rig.driver.OnPausePointReached (1);

            Assert::IsFalse (rig.driver.IsRunning(), L"the step ended");
            Assert::IsFalse (rig.driver.IsSilent(),  L"and nothing is silenced any longer");

            rig.Start (RunKind::Go);
            Assert::IsFalse (rig.driver.IsSilent(), L"the next run is heard");
        }
    };
}

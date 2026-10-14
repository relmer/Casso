#include "Pch.h"

#include "GuestSession.h"
#include "HResultAssert.h"
#include "TestMachine.h"
#include "Debugger/Events/DebugEventHistory.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"
#include "Shell/ScratchEventReplayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebugEventReplayRateTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DiskEventProbe
    //
    //  The Disk II feed on the History slot of a machine replaying history,
    //  one entry per event, its kind the only field.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class DiskEventProbe : public IDebugEventProbe<int>, public IDisk2EventSink
    {
    public:
        HRESULT  Attach (MachineHost & machine, std::vector<DebugEventEntry<int>> & entries) override
        {
            m_machine = &machine;
            m_entries = &entries;

            machine.AttachObservers ({ this, nullptr }, ObserverSlot::History);

            return S_OK;
        }

        void  Detach (MachineHost & machine) override
        {
            machine.AttachObservers ({}, ObserverSlot::History);

            m_machine = nullptr;
            m_entries = nullptr;
        }

        void OnMotorCommandOn() override                     { Add (1); }
        void OnMotorEngaged() override                       { Add (2); }
        void OnMotorCommandOff() override                    { Add (3); }
        void OnMotorDisengaged() override                    { Add (4); }
        void OnHeadStep        (int, int) override           { Add (5); }
        void OnHeadBump        (int) override                { Add (6); }
        void OnAddressMark     (int, int, int) override      { Add (7); }
        void OnDataMarkRead    (int, int, int, int) override { Add (8); }
        void OnDataMarkWrite   (int, int, int, int) override { Add (9); }
        void OnDriveSelect     (int) override                { Add (10); }
        void OnDiskInserted    (int) override                { Add (11); }
        void OnDiskEjected     (int) override                { Add (12); }

    private:
        void  Add (int kind)
        {
            m_entries->push_back (DebugEventEntry<int> { DebugEventEntryKind::Event, m_machine->GetPosition(), m_machine->GetCpu()->GetTotalCycles(), kind });
        }

        MachineHost                        * m_machine = nullptr;
        std::vector<DebugEventEntry<int>>  * m_entries = nullptr;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugEventReplayRateTests
    //
    //  How fast history's stretches replay with the Disk II feed attached,
    //  on a DOS 3.3 boot, the busiest the disk gets: each stretch timed on
    //  the scratch machine, and its events counted. The rate and the events
    //  per stretch set DebugEventReplayer::kPrefetch (R-045, SC-054).
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugEventReplayRateTests)
    {
    public:

        TEST_METHOD (ADos33BootReplaysWithTheDiskFeed)
        {
            TestMachine                                machine    ("Apple2eEnhanced");
            ReverseController                          controller (machine);
            DebugEventHistory                          history    (machine);
            ScratchEventReplayer<int>                  replayer   (std::make_unique<DiskEventProbe>());
            std::vector<DebugEventBounds>              stretches;
            std::vector<DebugEventStretchResult<int>>  results;
            std::vector<double>                        ms;
            std::vector<size_t>                        events;
            DebugEventJob                              job;
            std::vector<DebugEventBounds>              one (1);
            HRESULT                                    hr         = S_OK;
            double                                     totalMs    = 0.0;
            size_t                                     busy       = 0;
            size_t                                     total      = 0;
            size_t                                     i          = 0;
            std::chrono::steady_clock::time_point      began;



            machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });

            hr = controller.Start (ReverseSettings());
            AssertSucceeded (hr, L"Start");

            GuestSession::BootToPrompt (machine, GuestSession::RequireDos33Master());

            hr = controller.GetKeyframes().WaitForPending();
            AssertSucceeded (hr, L"WaitForPending");

            history.Attach (&controller.GetKeyframes());
            history.GetStretches (stretches);
            Assert::IsTrue (stretches.size() > 10, L"the boot left history to replay");

            replayer.SetMachine (machine.GetConfig(), machine.GetCurrentMachineName());

            //  One stretch a job, newest first, in the order a view scrolling
            //  back requests them, each timed whole: unpacking, loading and
            //  replaying.
            for (i = stretches.size(); i > 0; i--)
            {
                one[0] = stretches[i - 1];

                hr = history.BuildJob (one, job);
                AssertSucceeded (hr, L"BuildJob");

                began = std::chrono::steady_clock::now();

                hr = replayer.Rebuild (job, results);
                AssertSucceeded (hr, L"Rebuild");

                ms.push_back (std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - began).count());
                events.push_back (results[0].entries.size());

                totalMs += ms.back();
                busy    += (results[0].entries.size() > 0) ? 1 : 0;
                total   += results[0].entries.size();
            }

            std::ranges::sort (ms);

            Logger::WriteMessage (std::format ("{} stretches in {:.0f} ms: {:.1f} a second; per stretch median {:.2f} ms, slowest {:.2f} ms\n",
                                               ms.size(), totalMs, ms.size() * 1000.0 / totalMs, ms[ms.size() / 2], ms.back()).c_str());
            Logger::WriteMessage (std::format ("{} stretches with disk events; at most {} events in one, {} in all\n",
                                               busy, std::ranges::max (events), total).c_str());

            Assert::IsTrue (busy > 0, L"the boot ran the disk");
        }
    };
}

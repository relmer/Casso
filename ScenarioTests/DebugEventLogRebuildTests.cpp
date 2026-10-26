#include "Pch.h"

#include "GuestSession.h"
#include "HResultAssert.h"
#include "MachineIdle.h"
#include "TestMachine.h"
#include "Debugger/Events/DebugEventHistory.h"
#include "Debugger/Events/MachineEventFeed.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Shell/DebugEventLogs.h"
#include "Shell/ScratchEventReplayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebugEventLogRebuildTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugEventLogRebuildTests
    //
    //  A DOS 3.3 boot on the real master, with the Disk II log cached from
    //  the start: the same log rebuilt from history, every stretch of it, has
    //  the same events, in the same order, at the same positions and cycles,
    //  with the same sector bytes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugEventLogRebuildTests)
    {
    public:

        static bool  IsSameEntry (const DebugEventEntry<Disk2Event> & a, const DebugEventEntry<Disk2Event> & b)
        {
            bool  isSameData = (a.data == nullptr) == (b.data == nullptr);



            if (isSameData && a.data != nullptr)
            {
                isSameData = *a.data == *b.data;
            }

            return a.kind     == b.kind     &&
                   a.position == b.position &&
                   a.cycle    == b.cycle    &&
                   memcmp (&a.event, &b.event, sizeof (Disk2Event)) == 0 &&
                   isSameData;
        }


        TEST_METHOD (ADos33BootsLogRebuiltFromHistoryMatchesTheLiveLog)
        {
            TestMachine                                       machine    ("Apple2eEnhanced");
            ReverseController                                 controller (machine);
            std::unique_ptr<DebugEventLogs>                   logsOwner  = std::make_unique<DebugEventLogs> (machine);
            DebugEventLogs                                  & logs       = *logsOwner;
            const DebugEventCache<Disk2Event>               & cache      = logs.GetDiskCache();
            DebugEventHistory                                 history    (machine);
            ScratchEventReplayer<Disk2Event>                  replayer   (std::make_unique<DiskEventProbe>());
            std::vector<DebugEventBounds>                     stretches;
            std::vector<DebugEventStretchResult<Disk2Event>>  results;
            std::vector<DebugEventEntry<Disk2Event>>          live;
            std::vector<std::string>                          rows;
            DebugEventJob                                     job;
            HRESULT                                           hr         = S_OK;
            uint64_t                                          end        = 0;
            size_t                                            compared   = 0;
            size_t                                            sectors    = 0;
            size_t                                            i          = 0;
            bool                                              isAtPrompt = false;



            //  History and the log start with the disk in the drive, so every
            //  stretch has its keyframe taken with the disk the boot reads.
            machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
            GuestSession::Mount (machine, GuestSession::RequireDos33Master());

            hr = controller.Start (ReverseSettings());
            AssertSucceeded (hr, L"Start");

            logs.Start (&controller);

            MachineIdle::RunUntilIdle (machine, GuestSession::kBootCycles);

            isAtPrompt = GuestSession::TryPageToPrompt (machine, rows);
            Assert::IsTrue (isAtPrompt, L"DOS booted to its prompt");

            hr = controller.GetKeyframes().WaitForPending();
            AssertSucceeded (hr, L"WaitForPending");

            history.Attach (&controller.GetKeyframes());
            history.GetStretches (stretches);

            Assert::IsTrue   (stretches.size() > 10, L"the boot left history to rebuild");
            Assert::IsFalse  (cache.HasDropped(), L"the live log has the whole boot");
            Assert::AreEqual (stretches.front().startPosition, cache.GetCoveredFrom(), L"the live log and history start together");

            end = stretches.back().endPosition;

            std::ranges::reverse (stretches);

            hr = history.BuildJob (stretches, job);
            AssertSucceeded (hr, L"BuildJob");

            replayer.SetMachine (machine.GetConfig(), machine.GetCurrentMachineName());

            hr = replayer.Rebuild (job, results);
            AssertSucceeded (hr, L"Rebuild");
            Assert::AreEqual (stretches.size(), results.size(), L"every stretch rebuilt");

            for (const DebugEventStretchResult<Disk2Event> & result : results)
            {
                Assert::IsFalse (result.isUnavailable, std::format (L"the stretch from {} replays", result.startPosition).c_str());

                live.clear();
                cache.CopySpan (result.startPosition, result.endPosition, live);

                Assert::AreEqual (live.size(), result.entries.size(), std::format (L"the events of the stretch from {}", result.startPosition).c_str());

                for (i = 0; i < live.size(); i++)
                {
                    Assert::IsTrue (IsSameEntry (live[i], result.entries[i]), std::format (L"event {} of the stretch from {}", i, result.startPosition).c_str());

                    sectors += (live[i].event.type == Disk2EventType::DataRead) ? 1 : 0;
                }

                compared += live.size();
            }

            logs.Stop();

            Logger::WriteMessage (std::format ("{} stretches, {} events compared, {} of them sector reads; {} events live\n",
                                               results.size(), compared, sectors, cache.GetCount()).c_str());

            live.clear();
            cache.CopySpan (cache.GetCoveredFrom(), end, live);

            Assert::AreEqual (live.size(), compared, L"every live event up to the last keyframe is in a rebuilt stretch");
            Assert::AreEqual ((size_t) 98, sectors, L"each sector the boot requested, and none passed over");
        }
    };
}

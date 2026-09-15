#include "Pch.h"

#include "MockDebugTarget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MockDebugTargetTests
//
//  The session tests rely on this mock's records and memory map, so its
//  behavior is pinned here rather than assumed.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (MockDebugTargetTests)
    {
    public:

        class RecordingObserver : public IRunObserver
        {
        public:
            std::vector<StopEvent> stops;

            void OnStopped (const StopEvent & stop) override { stops.push_back (stop); }
        };



        TEST_METHOD (MemoryMap_IoUnreadable_RomReadOnly)
        {
            MockDebugTarget target;
            Byte            value = 0;



            Assert::IsTrue  (target.TryPoke (0x0300, 0xA9));
            Assert::IsTrue  (target.TryPeek (0x0300, value));
            Assert::AreEqual ((Byte) 0xA9, value);
            Assert::IsFalse (target.TryPeek (0xC000, value));
            Assert::IsFalse (target.TryPoke (0xD000, 0x00));
            Assert::AreEqual ((int) MemoryRegion::Io,      (int) target.GetRegion (0xC030));
            Assert::AreEqual ((int) MemoryRegion::Rom,     (int) target.GetRegion (0xFFFF));
            Assert::AreEqual ((int) MemoryRegion::MainRam, (int) target.GetRegion (0x0300));
        }



        TEST_METHOD (RecordsHookMaskRunsAndStops)
        {
            MockDebugTarget   target;
            RecordingObserver observer;
            WatchedPages      pages = {};
            RunRequest        run;
            StopEvent         stop;



            pages[0xC0] = true;
            run.kind    = RunKind::StepOver;
            stop.reason = StopReason::Breakpoint;
            stop.pc     = 0x0303;

            target.SetHookInstalled (true);
            target.SetWatchedPages  (pages);
            target.StartRun         (run);
            target.RequestPause();
            target.SetRunObserver   (&observer);
            target.Stop             (stop);

            Assert::IsTrue   (target.hookInstalled);
            Assert::AreEqual (1, target.hookChanges);
            Assert::IsTrue   (target.watchedPages[0xC0]);
            Assert::AreEqual (1, target.maskChanges);
            Assert::AreEqual ((size_t) 1, target.runs.size());
            Assert::AreEqual ((int) RunKind::StepOver, (int) target.runs[0].kind);
            Assert::AreEqual (1, target.pauseRequests);
            Assert::AreEqual ((size_t) 1, observer.stops.size());
            Assert::AreEqual ((Word) 0x0303, observer.stops[0].pc);
        }
    };
}

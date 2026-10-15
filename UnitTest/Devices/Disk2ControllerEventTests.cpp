#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"


// Disk2Controller carries two DiskImage instances; per-test heap
// allocation would otherwise blow the C6262 stack-frame budget.
#pragma warning (disable: 6262)


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingEventSink
//
//  Captures an ordered log of every controller event for assertion.
//  Each entry records the event type plus the single most-relevant
//  integer argument. The address-mark / data-mark events are
//  reachable through this sink too (the controller's embedded
//  watcher shares the sink pointer), so this fixture also exercises
//  the FR-008 wiring path end-to-end.
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2ControllerEventTests
//
//  Verifies the spec-006 fire sites at every controller event
//  surface (FR-006, FR-007, FR-020, SC-007). The fixture mirrors
//  Disk2ControllerAudioTests's recording-mock pattern.
//
////////////////////////////////////////////////////////////////////////////////

namespace Disk2ControllerEventTests
{
    TEST_CLASS (Disk2ControllerEventTests)
    {
    public:

        class RecordingEventSink : public IDisk2EventSink
        {
        public:
            enum class Event
            {
                MotorCommandOn,
                MotorEngaged,
                MotorCommandOff,
                MotorDisengaged,
                HeadStep,
                HeadBump,
                AddressMark,
                DataMarkRead,
                DataMarkWrite,
                DriveSelect,
                DiskInserted,
                DiskEjected,
                WriteModeOn,
                WriteModeOff,
            };

            struct LogEntry
            {
                Event  event;
                int    arg;
            };

            std::vector<LogEntry>  log;

            void OnMotorCommandOn() override                    { log.push_back ({ Event::MotorCommandOn,  0 }); }
            void OnMotorEngaged() override                      { log.push_back ({ Event::MotorEngaged,    0 }); }
            void OnMotorCommandOff() override                   { log.push_back ({ Event::MotorCommandOff, 0 }); }
            void OnMotorDisengaged() override                   { log.push_back ({ Event::MotorDisengaged, 0 }); }
            void OnHeadStep (int, int newQt) override           { log.push_back ({ Event::HeadStep,        newQt }); }
            void OnHeadBump (int atQt) override                 { log.push_back ({ Event::HeadBump,        atQt }); }
            void OnAddressMark (int, int sector, int) override  { log.push_back ({ Event::AddressMark,  sector }); }
            void OnDataMarkRead (int, int sector, int, std::span<const Byte>) override   { log.push_back ({ Event::DataMarkRead,    sector }); }
            void OnDataMarkWrite (int, int sector, int, std::span<const Byte>) override  { log.push_back ({ Event::DataMarkWrite,   sector }); }
            void OnDriveSelect (int drive) override             { log.push_back ({ Event::DriveSelect,     drive }); }
            void OnDiskInserted (int drive) override            { log.push_back ({ Event::DiskInserted,    drive }); }
            void OnDiskEjected (int drive) override             { log.push_back ({ Event::DiskEjected,     drive }); }
            void OnWriteMode (int drive, bool isOn) override    { log.push_back ({ isOn ? Event::WriteModeOn : Event::WriteModeOff, drive }); }

            int CountOf (Event ev) const
            {
                int     n = 0;
                for (const auto & logEntry : log)
                {
                    if (logEntry.event == ev)
                    {
                        n++;
                    }
                }

                return n;
            }
        };

        TEST_METHOD (MotorOnFirstStrobe_firesBothCommandOnAndEngaged)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);
            ctrl.Write (0xC0E9, 0x00);

            Assert::AreEqual (1, sink.CountOf (RecordingEventSink::Event::MotorCommandOn));
            Assert::AreEqual (1, sink.CountOf (RecordingEventSink::Event::MotorEngaged));
        }


        TEST_METHOD (MotorReStrobe_firesCommandOnButNotEngagedAgain)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);
            ctrl.Write (0xC0E9, 0x00);
            ctrl.Write (0xC0E9, 0x00);
            ctrl.Write (0xC0E9, 0x00);

            Assert::AreEqual (3, sink.CountOf (RecordingEventSink::Event::MotorCommandOn));
            Assert::AreEqual (1, sink.CountOf (RecordingEventSink::Event::MotorEngaged));
        }


        TEST_METHOD (MotorOffStrobe_firesCommandOff_butDisengagedOnlyAfterSpindown)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);
            ctrl.Write (0xC0E9, 0x00);    // motor on
            ctrl.Write (0xC0E8, 0x00);    // motor off (arms spindown)

            // Mid-spindown: command-off has fired, disengaged has not.
            ctrl.Tick (500000);
            Assert::AreEqual (1, sink.CountOf (RecordingEventSink::Event::MotorCommandOff));
            Assert::AreEqual (0, sink.CountOf (RecordingEventSink::Event::MotorDisengaged));

            // Past the 1,000,000-cycle timer.
            ctrl.Tick (600000);
            Assert::AreEqual (1, sink.CountOf (RecordingEventSink::Event::MotorCommandOff));
            Assert::AreEqual (1, sink.CountOf (RecordingEventSink::Event::MotorDisengaged));
        }


        TEST_METHOD (MotorOffFlushCallback_firesOnceOnSpindownCompletion)
        {
            int               flushes = 0;



            Disk2Controller   ctrl (6);

            ctrl.SetMotorOffFlushCallback ([&] () { flushes++; });

            ctrl.Write (0xC0E9, 0x00);    // motor on
            ctrl.Write (0xC0E8, 0x00);    // motor off (arms spindown)

            // Mid-spindown: the disk op isn't done yet, so no flush.
            ctrl.Tick (500000);
            Assert::AreEqual (0, flushes, L"flush must wait for the spindown to complete");

            // Past the 1,000,000-cycle spindown timer -> motor disengages.
            ctrl.Tick (600000);
            Assert::AreEqual (1, flushes, L"motor-off flush fires on spindown completion");

            // Motor already off: further ticks must not re-fire.
            ctrl.Tick (2000000);
            Assert::AreEqual (1, flushes, L"no re-fire while the motor stays off");
        }


        TEST_METHOD (MotorOffFlushCallback_notFiredWhileMotorRuns)
        {
            int               flushes = 0;



            Disk2Controller   ctrl (6);

            ctrl.SetMotorOffFlushCallback ([&] () { flushes++; });

            ctrl.Write (0xC0E9, 0x00);    // motor on, never commanded off
            ctrl.Tick  (5000000);

            Assert::AreEqual (0, flushes, L"a running motor must not trigger a flush");
        }


        TEST_METHOD (MotorOffFlushCallback_firesAgainOnEachOperationCycle)
        {
            int               flushes = 0;



            Disk2Controller   ctrl (6);

            ctrl.SetMotorOffFlushCallback ([&] () { flushes++; });

            // First operation: on -> off -> spin down.
            ctrl.Write (0xC0E9, 0x00);
            ctrl.Write (0xC0E8, 0x00);
            ctrl.Tick  (1100000);
            Assert::AreEqual (1, flushes);

            // Second operation must flush again on its own spindown.
            ctrl.Write (0xC0E9, 0x00);
            ctrl.Write (0xC0E8, 0x00);
            ctrl.Tick  (1100000);
            Assert::AreEqual (2, flushes, L"each disk operation's spindown flushes");
        }


        TEST_METHOD (MotorOffRestrobe_alwaysFiresCommandOff_evenWhenMotorAlreadyOff)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);
            ctrl.Write (0xC0E8, 0x00);
            ctrl.Write (0xC0E8, 0x00);

            // FR-006: every $C0E8 strobe is a logical "motor command
            // off" event, even when no motor is currently engaged.
            Assert::AreEqual (2, sink.CountOf (RecordingEventSink::Event::MotorCommandOff));
            Assert::AreEqual (0, sink.CountOf (RecordingEventSink::Event::MotorDisengaged));
        }


        TEST_METHOD (PhaseChange_noMovement_firesNoHeadEvents)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);

            // Energize phase 0 with no other phases set: qtDelta == 0.
            ctrl.Write (0xC0E1, 0x00);

            Assert::AreEqual (0, sink.CountOf (RecordingEventSink::Event::HeadStep));
            Assert::AreEqual (0, sink.CountOf (RecordingEventSink::Event::HeadBump));
        }


        TEST_METHOD (PhaseChange_pastTrack0_firesHeadBumpNotHeadStep)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);

            // Head at qt 0, energize phase 3 -> decrement direction,
            // pre-clamp delta is negative, clamped to 0 == bump.
            ctrl.Write (0xC0E7, 0x00);

            Assert::AreEqual (0, sink.CountOf (RecordingEventSink::Event::HeadStep));
            Assert::AreEqual (1, sink.CountOf (RecordingEventSink::Event::HeadBump));
        }


        TEST_METHOD (PhaseChange_oneQuarterStep_firesHeadStepWithPrevAndNewQt)
        {
            RecordingEventSink   sink;
            size_t               i = 0;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);

            // qt 0, rot 0. Adjacent phase 1 -> direction=+1,
            // single magnet => qtDelta=+2.
            ctrl.Write (0xC0E3, 0x00);

            Assert::AreEqual (1, sink.CountOf (RecordingEventSink::Event::HeadStep));
            Assert::AreEqual (0, sink.CountOf (RecordingEventSink::Event::HeadBump));
            Assert::AreEqual (2, ctrl.GetQuarterTrack());

            for (const auto & logEntry : sink.log)
            {
                if (logEntry.event == RecordingEventSink::Event::HeadStep)
                {
                    // RecordingEventSink stashes newQt; verify it
                    // matches the post-step position.
                    Assert::AreEqual (2, logEntry.arg);
                }
            }
        }


        //  A DOS seek turns the next phase on before it turns the last one
        //  off: the first pulls the head to the quarter track between, and
        //  the second finishes the half-track step. Each quarter-track move
        //  is a step event.
        TEST_METHOD (ADosSeekPassesTheQuarterTrackBetween)
        {
            RecordingEventSink   sink;
            Disk2Controller      ctrl (6);
            std::vector<int>     positions;



            ctrl.SetEventSink (&sink);

            ctrl.Write (0xC0E1, 0x00);      // phase 0 on: the head is there
            ctrl.Write (0xC0E3, 0x00);      // phase 1 on
            ctrl.Write (0xC0E0, 0x00);      // phase 0 off
            ctrl.Write (0xC0E5, 0x00);      // phase 2 on
            ctrl.Write (0xC0E2, 0x00);      // phase 1 off

            for (const auto & logEntry : sink.log)
            {
                if (logEntry.event == RecordingEventSink::Event::HeadStep)
                {
                    positions.push_back (logEntry.arg);
                }
            }

            Assert::IsTrue   ((positions == std::vector<int> { 1, 2, 3, 4 }), L"quarter tracks 1, 2, 3 and 4");
            Assert::AreEqual (4, ctrl.GetQuarterTrack());
        }


        //  With two adjacent phases on, the head rests on the quarter track
        //  between them, and a phase turned on again does not move it.
        TEST_METHOD (TwoAdjacentPhasesHoldTheQuarterTrackBetween)
        {
            Disk2Controller  ctrl (6);



            ctrl.Write (0xC0E1, 0x00);      // phase 0 on
            ctrl.Write (0xC0E3, 0x00);      // phase 1 on
            Assert::AreEqual (1, ctrl.GetQuarterTrack());

            ctrl.Write (0xC0E3, 0x00);
            ctrl.Write (0xC0E1, 0x00);
            Assert::AreEqual (1, ctrl.GetQuarterTrack(), L"the same phases hold it there");
        }


        //  A phase directly opposite the head pulls neither way, so with
        //  opposite phases on, or the opposite phase alone, the head does not
        //  move.
        TEST_METHOD (OppositePhasesDoNotMoveTheHead)
        {
            Disk2Controller  ctrl (6);



            ctrl.Write (0xC0E1, 0x00);      // phase 0 on
            ctrl.Write (0xC0E5, 0x00);      // phase 2 on, opposite
            Assert::AreEqual (0, ctrl.GetQuarterTrack());

            ctrl.Write (0xC0E0, 0x00);      // phase 0 off: phase 2 alone, opposite
            Assert::AreEqual (0, ctrl.GetQuarterTrack());
        }


        TEST_METHOD (ThePullIsTheAverageOfTheEnergizedPhases)
        {
            Assert::AreEqual ( 0, Disk2Controller::GetPull (0b0000, 5),  L"no phase on");
            Assert::AreEqual ( 2, Disk2Controller::GetPull (0b0010, 0),  L"phase 1 from track 0");
            Assert::AreEqual ( 1, Disk2Controller::GetPull (0b0011, 0),  L"phases 0 and 1: the quarter track between");
            Assert::AreEqual ( 1, Disk2Controller::GetPull (0b0010, 1),  L"phase 1 from the quarter track before it");
            Assert::AreEqual (-2, Disk2Controller::GetPull (0b1000, 0),  L"phase 3 pulls back from track 0");
            Assert::AreEqual ( 0, Disk2Controller::GetPull (0b0100, 0),  L"phase 2 is opposite track 0");
            Assert::AreEqual ( 0, Disk2Controller::GetPull (0b1011, 0),  L"phases 3, 0 and 1 balance");
            Assert::AreEqual ( 2, Disk2Controller::GetPull (0b0010, 64), L"the cycle repeats every eight quarter tracks");
        }

        TEST_METHOD (DriveSelect_firesOnceWithNewDriveIndex)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);
            ctrl.Write (0xC0EB, 0x00);    // drive 2 (index 1)
            ctrl.Write (0xC0EA, 0x00);    // drive 1 (index 0)

            Assert::AreEqual (2, sink.CountOf (RecordingEventSink::Event::DriveSelect));
            Assert::AreEqual (1, sink.log[0].arg);
            Assert::AreEqual (0, sink.log[1].arg);
        }


        TEST_METHOD (EjectDisk_firesOnceWithDriveIndex)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);
            ctrl.EjectDisk (0);

            Assert::AreEqual (1, sink.CountOf (RecordingEventSink::Event::DiskEjected));
            Assert::AreEqual (0, sink.log[0].arg);
        }


        ////////////////////////////////////////////////////////////////
        //
        //  Spec-006 bug 14b. EmulatorShell drives mount/eject through
        //  DiskImageStore + SetExternalDisk, bypassing MountDisk and
        //  EjectDisk. NotifyDiskInserted / NotifyDiskEjected let the
        //  shell still fire the user-facing controller events.
        //
        ////////////////////////////////////////////////////////////////

        TEST_METHOD (NotifyDiskInserted_firesOnceWithDriveIndex)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);
            ctrl.NotifyDiskInserted (1);

            Assert::AreEqual (1, sink.CountOf (RecordingEventSink::Event::DiskInserted));
            Assert::AreEqual (1, sink.log[0].arg);
        }


        TEST_METHOD (NotifyDiskEjected_firesOnceWithDriveIndex)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);
            ctrl.NotifyDiskEjected (0);

            Assert::AreEqual (1, sink.CountOf (RecordingEventSink::Event::DiskEjected));
            Assert::AreEqual (0, sink.log[0].arg);
        }


        TEST_METHOD (NotifyDisk_invalidDrive_isNoop)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            ctrl.SetEventSink (&sink);
            ctrl.NotifyDiskInserted (-1);
            ctrl.NotifyDiskInserted (99);
            ctrl.NotifyDiskEjected  (-1);
            ctrl.NotifyDiskEjected  (99);

            Assert::AreEqual ((size_t) 0, sink.log.size());
        }


        TEST_METHOD (NotifyDisk_noSinkAttached_doesNotCrash)
        {
            Disk2Controller  ctrl (6);

            // No SetEventSink call -- m_eventSink == nullptr.
            ctrl.NotifyDiskInserted (0);
            ctrl.NotifyDiskEjected  (0);
        }


        TEST_METHOD (DetachedSink_firesNothing)
        {
            RecordingEventSink   sink;



            Disk2Controller      ctrl (6);

            // Attach then immediately revoke -- subsequent activity
            // MUST NOT touch the sink (the controller's per-fire-site
            // nullptr guard is the load-bearing FR-007 / SC-007
            // invariant).
            ctrl.SetEventSink (&sink);
            ctrl.SetEventSink (nullptr);

            ctrl.Write (0xC0E9, 0x00);
            ctrl.Write (0xC0E7, 0x00);
            ctrl.Write (0xC0E8, 0x00);
            ctrl.Tick  (1100000);
            ctrl.EjectDisk (0);

            Assert::AreEqual ((size_t) 0, sink.log.size());
        }


        TEST_METHOD (NoSink_quarterTrackPositionMatchesAttachedSinkBaseline)
        {
            RecordingEventSink   sink;
            size_t               i        = 0;



            // T034 regression assertion: with a sink attached the
            // controller's observable head state must move
            // identically to the no-sink baseline. We run the same
            // phase sequence on two controllers (one with a
            // recording sink, one without) and assert their
            // m_quarterTrack values agree at every step. The
            // recording sink records but never consumes, so any
            // mutation of the controller's observable state from
            // event firing would diverge them immediately.
            Disk2Controller      baseline (6);
            Disk2Controller      observed (6);
            const Word           phases[] = { 0xC0E3, 0xC0E5, 0xC0E7, 0xC0E1, 0xC0E3, 0xC0E5 };

            observed.SetEventSink (&sink);

            for (i = 0; i < sizeof (phases) / sizeof (phases[0]); i++)
            {
                baseline.Write (phases[i], 0x00);
                observed.Write (phases[i], 0x00);
                Assert::AreEqual (baseline.GetQuarterTrack(), observed.GetQuarterTrack());
            }

            // And after a motor cycle.
            baseline.Write (0xC0E9, 0x00);
            observed.Write (0xC0E9, 0x00);
            baseline.Write (0xC0E8, 0x00);
            observed.Write (0xC0E8, 0x00);
            baseline.Tick  (1100000);
            observed.Tick  (1100000);

            Assert::AreEqual (baseline.IsMotorOn(),       observed.IsMotorOn());
            Assert::AreEqual (baseline.GetQuarterTrack(), observed.GetQuarterTrack());
            Assert::AreEqual (baseline.GetActiveDrive(),  observed.GetActiveDrive());
        }


        TEST_METHOD (NibbleReadByte_returnedValueUnchangedByWatcherObservation)
        {
            RecordingEventSink   sink;
            int                  i = 0;



            // T034 byte-identity (reduced scope, see commit note):
            // the simplest verifiable invariant is that the read
            // path's return value with vs without a sink is
            // identical -- the watcher must inspect but not mutate
            // the latch byte.
            Disk2Controller      baseline (6);
            Disk2Controller      observed (6);

            observed.SetEventSink (&sink);

            // Bring the engine into latch-read mode (Q6=0, Q7=0)
            // and pump it; the unloaded engine returns deterministic
            // zero / random-stream nibbles depending on Phase 9
            // engine state, but both controllers see the SAME stream
            // because they share no other state.
            for (i = 0; i < 256; i++)
            {
                Byte  a = baseline.Read (0xC08C);
                Byte  b = observed.Read (0xC08C);
                Assert::AreEqual (a, b);
            }
        }


        //  A sequence touching every switch kind: motor, a step, both drive
        //  selects, write mode on and off, and the motor off and spun down.
        static void RunScript (Disk2Controller & ctrl)
        {
            ctrl.Write (0xC0E9, 0x00);      // motor on
            ctrl.Write (0xC0E3, 0x00);      // phase 1 on: a step
            ctrl.Write (0xC0E2, 0x00);      // phase 1 off
            ctrl.Write (0xC0EB, 0x00);      // drive 2
            ctrl.Write (0xC0EA, 0x00);      // drive 1
            ctrl.Write (0xC0EF, 0x00);      // write mode on
            ctrl.Write (0xC0EF, 0x00);      // on again: no event
            ctrl.Write (0xC0EE, 0x00);      // write mode off
            ctrl.Write (0xC0E8, 0x00);      // motor off
            ctrl.Tick  (Disk2Controller::kMotorSpindownCycles + 1);
        }


        static bool IsSameLog (const RecordingEventSink & a, const RecordingEventSink & b)
        {
            return std::equal (a.log.begin(), a.log.end(), b.log.begin(), b.log.end(),
                               [] (const auto & x, const auto & y) { return x.event == y.event && x.arg == y.arg; });
        }


        //  Every event is delivered to the Breakpoints slot alone, and to the
        //  Panel and Breakpoints slots together, in the same order as to the
        //  Panel slot alone.
        TEST_METHOD (EachEventReachesTheBreakpointsSlotAloneAndBesideThePanel)
        {
            RecordingEventSink  panelAlone;
            RecordingEventSink  breakAlone;
            RecordingEventSink  panelBoth;
            RecordingEventSink  breakBoth;
            auto                first  = std::make_unique<Disk2Controller> (6);
            auto                second = std::make_unique<Disk2Controller> (6);
            auto                third  = std::make_unique<Disk2Controller> (6);



            first->SetEventSink (&panelAlone);
            RunScript (*first);

            second->SetEventSink (ObserverSlot::Breakpoints, &breakAlone);
            RunScript (*second);

            third->SetEventSink (&panelBoth);
            third->SetEventSink (ObserverSlot::Breakpoints, &breakBoth);
            RunScript (*third);

            Assert::IsTrue (panelAlone.CountOf (RecordingEventSink::Event::WriteModeOn)  == 1, L"write mode on, once");
            Assert::IsTrue (panelAlone.CountOf (RecordingEventSink::Event::WriteModeOff) == 1, L"write mode off, once");
            Assert::IsTrue (IsSameLog (panelAlone, breakAlone), L"the Breakpoints slot alone");
            Assert::IsTrue (IsSameLog (panelAlone, panelBoth),  L"the Panel slot beside it");
            Assert::IsTrue (IsSameLog (panelAlone, breakBoth),  L"the Breakpoints slot beside the Panel");
        }


        //  With the window's sink set after the debugger's, the debugger's is in
        //  its slot, and the Disk II log's events are the same whether or not
        //  a Breakpoints listener is there.
        TEST_METHOD (TheBreakpointsSlotIsIndependentOfTheOthers)
        {
            RecordingEventSink  breakpoints;
            RecordingEventSink  window;
            RecordingEventSink  historyAlone;
            RecordingEventSink  historyBeside;
            auto                ctrl     = std::make_unique<Disk2Controller> (6);
            auto                alone    = std::make_unique<Disk2Controller> (6);
            auto                beside   = std::make_unique<Disk2Controller> (6);



            ctrl->SetEventSink (ObserverSlot::Breakpoints, &breakpoints);
            ctrl->SetEventSink (&window);
            RunScript (*ctrl);

            Assert::IsFalse (breakpoints.log.empty(), L"the Breakpoints sink is in its slot after the Panel sink is set");
            Assert::IsTrue  (IsSameLog (window, breakpoints));

            alone->SetEventSink (ObserverSlot::History, &historyAlone);
            RunScript (*alone);

            beside->SetEventSink (ObserverSlot::History,     &historyBeside);
            beside->SetEventSink (ObserverSlot::Breakpoints, &breakpoints);
            RunScript (*beside);

            Assert::IsTrue (IsSameLog (historyAlone, historyBeside), L"the History slot's events are the same with a Breakpoints listener");
        }


        TEST_METHOD (WithNoListenerNoSlotIsAny)
        {
            ObserverSlots<IDisk2EventSink>  slots;
            RecordingEventSink              sink;



            Assert::IsFalse (slots.IsAny());

            slots.Set (ObserverSlot::Breakpoints, &sink);
            Assert::IsTrue  (slots.IsAny(), L"the Breakpoints slot alone");

            slots.Set (ObserverSlot::Breakpoints, nullptr);
            Assert::IsFalse (slots.IsAny());
        }
    };
}


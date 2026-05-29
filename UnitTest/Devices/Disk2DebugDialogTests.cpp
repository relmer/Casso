#include "Pch.h"
#include "Disk2DebugDialogState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugDialogTests
//
//  Spec-006 T054 / T063. Headless coverage of the non-Win32 pieces of
//  the Disk II Debug dialog: FilterState defaults, SeedDefaultColumns
//  contract, and the ComputeWasAtTail truth table.
//
////////////////////////////////////////////////////////////////////////////////

namespace Disk2DebugDialogTests
{
    TEST_CLASS (Disk2DebugDialogTests)
    {
    public:

        TEST_METHOD (FilterState_defaults_matchSpec)
        {
            FilterState  f;

            Assert::AreEqual (FilterState::kEventCatAllMask, f.eventTypeMask);
            Assert::AreEqual (0, f.driveFilter);
            Assert::IsTrue   (f.audioMaster);
            Assert::IsTrue   (f.audioStarted);
            Assert::IsTrue   (f.audioRestarted);
            Assert::IsTrue   (f.audioContinued);
            Assert::IsTrue   (f.audioSilent);
            Assert::IsFalse  (f.trackFilterRawQt);
            Assert::IsTrue   (f.trackFilter.IsMatchAll());
            Assert::IsTrue   (f.sectorFilter.IsMatchAll());
        }



        TEST_METHOD (SeedDefaultColumns_populatesAllColumns_inIdOrder)
        {
            std::array<LogicalColumn, kColumnCount>  columns = {};
            const int                                expectedWidths[kColumnCount] =
            {
                kColWallWidth, kColUptimeWidth, kColCycleWidth,
                kColDriveWidth, kColEventWidth, kColDetailWidth
            };

            SeedDefaultColumns (columns);

            for (int i = 0; i < kColumnCount; i++)
            {
                Assert::AreEqual (i,                       columns[i].id);
                Assert::IsNotNull (columns[i].headerText);
                Assert::AreEqual (expectedWidths[i],       columns[i].defaultWidth);
                Assert::AreEqual (expectedWidths[i],       columns[i].savedWidth);
                Assert::IsTrue   (columns[i].visible);
                Assert::IsFalse  (columns[i].autoSizedYet);
            }
        }



        TEST_METHOD (SeedDefaultColumns_headerLabelsMatchSpec)
        {
            std::array<LogicalColumn, kColumnCount>  columns = {};
            const wchar_t * const                    expected[kColumnCount] =
            {
                L"Time", L"Uptime", L"Cycle count", L"Drive", L"Event", L"Detail"
            };

            SeedDefaultColumns (columns);

            for (int i = 0; i < kColumnCount; i++)
            {
                Assert::AreEqual (0, wcscmp (expected[i], columns[i].headerText));
            }
        }



        TEST_METHOD (ComputeWasAtTail_emptyList_returnsTrue)
        {
            Assert::IsTrue (ComputeWasAtTail (0, 10, 0));
        }



        TEST_METHOD (ComputeWasAtTail_partialLastRow_atTail_returnsTrue)
        {
            // Five rows total, viewport shows 10; clearly at the tail.
            Assert::IsTrue (ComputeWasAtTail (0, 10, 5));
        }



        TEST_METHOD (ComputeWasAtTail_exactFit_returnsTrue)
        {
            // topIndex 5 + 10 visible rows = covers rows [5..14]; total 15.
            Assert::IsTrue (ComputeWasAtTail (5, 10, 15));
        }



        TEST_METHOD (ComputeWasAtTail_scrolledUpByOne_returnsFalse)
        {
            // topIndex 4 + 10 covers rows [4..13]; total 15 -> last row hidden.
            Assert::IsFalse (ComputeWasAtTail (4, 10, 15));
        }



        TEST_METHOD (ComputeWasAtTail_scrolledUpByN_returnsFalse)
        {
            Assert::IsFalse (ComputeWasAtTail (0, 10, 100));
        }



        TEST_METHOD (MatchesFilter_eventsLost_alwaysShown_evenWithEverythingOff)
        {
            Disk2EventDisplay  e;
            FilterState         f;

            e.type           = Disk2EventType::EventsLost;
            e.category       = EventCategory::Controller;
            f.eventTypeMask  = 0;
            f.audioMaster    = false;

            Assert::IsTrue (MatchesFilter (e, f));
        }



        TEST_METHOD (MatchesFilter_motorEvent_categoryBitGates)
        {
            Disk2EventDisplay  e;
            FilterState         f;

            e.type     = Disk2EventType::MotorEngaged;
            e.category = EventCategory::Controller;

            Assert::IsTrue (MatchesFilter (e, f));

            f.eventTypeMask &= ~FilterState::kEventCatMotor;
            Assert::IsFalse (MatchesFilter (e, f));
        }



        TEST_METHOD (MatchesFilter_audioMasterOff_hidesAllAudio)
        {
            Disk2EventDisplay  loop;
            Disk2EventDisplay  shot;
            FilterState         f;

            loop.type     = Disk2EventType::AudioLoopStarted;
            loop.category = EventCategory::Audio;
            shot.type     = Disk2EventType::AudioStarted;
            shot.category = EventCategory::Audio;

            f.audioMaster = false;

            Assert::IsFalse (MatchesFilter (loop, f));
            Assert::IsFalse (MatchesFilter (shot, f));
        }



        TEST_METHOD (MatchesFilter_audioSubToggleOnlyAffectsOneShots)
        {
            Disk2EventDisplay  loopStart;
            Disk2EventDisplay  started;
            FilterState         f;

            loopStart.type      = Disk2EventType::AudioLoopStarted;
            loopStart.category  = EventCategory::Audio;
            started.type        = Disk2EventType::AudioStarted;
            started.category    = EventCategory::Audio;

            f.audioStarted    = false;
            f.audioRestarted  = false;
            f.audioContinued  = false;
            f.audioSilent     = false;

            // Loops survive sub-toggles.
            Assert::IsTrue  (MatchesFilter (loopStart, f));
            Assert::IsFalse (MatchesFilter (started,   f));
        }



        TEST_METHOD (MatchesFilter_driveRadio_strictMatchOnEventDrive)
        {
            // Spec-006 bug fix: every drive-specific event now carries
            // its 0-based drive index, so the "Drive 1" radio (1-based
            // UI) must match only drive 0 events and "Drive 2" must
            // match only drive 1 events. The previous "events without
            // drive bypass the predicate" rule has been retired; only
            // synthetic EventsLost still always shows.
            Disk2EventDisplay  d0;
            Disk2EventDisplay  d1;
            FilterState         f;

            d0.type     = Disk2EventType::HeadStep;
            d0.category = EventCategory::Controller;
            d0.drive    = 0;

            d1.type     = Disk2EventType::HeadStep;
            d1.category = EventCategory::Controller;
            d1.drive    = 1;

            // All drives: both pass.
            f.driveFilter = 0;
            Assert::IsTrue (MatchesFilter (d0, f));
            Assert::IsTrue (MatchesFilter (d1, f));

            // Drive 1 radio (UI 1-based -> event 0): only drive 0.
            f.driveFilter = 1;
            Assert::IsTrue  (MatchesFilter (d0, f));
            Assert::IsFalse (MatchesFilter (d1, f));

            // Drive 2 radio (UI 2 -> event 1): only drive 1.
            f.driveFilter = 2;
            Assert::IsFalse (MatchesFilter (d0, f));
            Assert::IsTrue  (MatchesFilter (d1, f));
        }



        TEST_METHOD (MatchesFilter_driveRadio_eventWithoutDrive_rejected_unlessAll)
        {
            Disk2EventDisplay  noDrive;
            FilterState         f;

            noDrive.type     = Disk2EventType::HeadStep;
            noDrive.category = EventCategory::Controller;
            noDrive.drive    = Disk2EventDisplay::kFieldNotApplicable;

            f.driveFilter = 0;
            Assert::IsTrue (MatchesFilter (noDrive, f));

            f.driveFilter = 1;
            Assert::IsFalse (MatchesFilter (noDrive, f));

            f.driveFilter = 2;
            Assert::IsFalse (MatchesFilter (noDrive, f));
        }
    };
}

#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "DiskIIDebugDialogState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIDebugDialogTests
//
//  Spec-006 T054 / T063. Headless coverage of the non-Win32 pieces of
//  the Disk II Debug dialog: FilterState defaults, SeedDefaultColumns
//  contract, and the ComputeWasAtTail truth table.
//
////////////////////////////////////////////////////////////////////////////////

namespace DiskIIDebugDialogTests
{
    TEST_CLASS (DiskIIDebugDialogTests)
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
            Assert::IsTrue   (f.trackFilter.IsMatchAll  ());
            Assert::IsTrue   (f.sectorFilter.IsMatchAll ());
        }



        TEST_METHOD (SeedDefaultColumns_populatesFiveColumns_inIdOrder)
        {
            std::array<LogicalColumn, 5>  columns = {};
            const int                     expectedWidths[5] =
            {
                kColWallWidth, kColUptimeWidth, kColCycleWidth,
                kColEventWidth, kColDetailWidth
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
            std::array<LogicalColumn, 5>  columns = {};
            const wchar_t * const         expected[5] =
            {
                L"Wall", L"Uptime", L"Cycle", L"Event", L"Detail"
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
            DiskIIEventDisplay  e;
            FilterState         f;

            e.type           = DiskIIEventType::EventsLost;
            e.category       = EventCategory::Controller;
            f.eventTypeMask  = 0;
            f.audioMaster    = false;

            Assert::IsTrue (MatchesFilter (e, f));
        }



        TEST_METHOD (MatchesFilter_motorEvent_categoryBitGates)
        {
            DiskIIEventDisplay  e;
            FilterState         f;

            e.type     = DiskIIEventType::MotorEngaged;
            e.category = EventCategory::Controller;

            Assert::IsTrue (MatchesFilter (e, f));

            f.eventTypeMask &= ~FilterState::kEventCatMotor;
            Assert::IsFalse (MatchesFilter (e, f));
        }



        TEST_METHOD (MatchesFilter_audioMasterOff_hidesAllAudio)
        {
            DiskIIEventDisplay  loop;
            DiskIIEventDisplay  shot;
            FilterState         f;

            loop.type     = DiskIIEventType::AudioLoopStarted;
            loop.category = EventCategory::Audio;
            shot.type     = DiskIIEventType::AudioStarted;
            shot.category = EventCategory::Audio;

            f.audioMaster = false;

            Assert::IsFalse (MatchesFilter (loop, f));
            Assert::IsFalse (MatchesFilter (shot, f));
        }



        TEST_METHOD (MatchesFilter_audioSubToggleOnlyAffectsOneShots)
        {
            DiskIIEventDisplay  loopStart;
            DiskIIEventDisplay  started;
            FilterState         f;

            loopStart.type      = DiskIIEventType::AudioLoopStarted;
            loopStart.category  = EventCategory::Audio;
            started.type        = DiskIIEventType::AudioStarted;
            started.category    = EventCategory::Audio;

            f.audioStarted    = false;
            f.audioRestarted  = false;
            f.audioContinued  = false;
            f.audioSilent     = false;

            // Loops survive sub-toggles.
            Assert::IsTrue  (MatchesFilter (loopStart, f));
            Assert::IsFalse (MatchesFilter (started,   f));
        }
    };
}

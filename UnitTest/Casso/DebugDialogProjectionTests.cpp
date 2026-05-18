#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "DebugDialogProjection.h"
#include "DiskIIDebugDialogState.h"

// Code-analysis: CppUnitTest's TEST_METHOD bodies inline an entire
// std::deque<DiskIIEventDisplay> (each entry ~ a few hundred bytes,
// kDisplayDequeCap == 100,000) on the function stack. C6262 fires
// at ~131 KB per frame; the deque header alone plus a handful of
// local DiskIIEventDisplay scratch records is well over the
// /analyze stack-frame budget. Disabling 6262 in this single file
// matches the established repo pattern for headless-test stacks
// that intentionally over-allocate for fixture clarity.
#pragma warning(disable: 6262)

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugDialogProjectionTests
//
//  Spec-006 T042. Headless unit tests for the UI-thread projection
//  helper: FormatEvent column shapes, DrainAndProject FIFO and
//  EventsLost ordering, kDisplayDequeCap rolling-cap enforcement.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static bool LooksLikeWallClock (const wchar_t * s)
    {
        // Shape: HH:MM:SS.mmm  (12 chars + null)
        if (s == nullptr)
        {
            return false;
        }

        if (wcslen (s) != 12)
        {
            return false;
        }

        for (size_t i = 0; i < 12; i++)
        {
            wchar_t  c = s[i];

            if (i == 2 || i == 5)
            {
                if (c != L':') { return false; }
            }
            else if (i == 8)
            {
                if (c != L'.') { return false; }
            }
            else
            {
                if (c < L'0' || c > L'9') { return false; }
            }
        }

        return true;
    }


    static DiskIIEvent MakeStep (int prevQt, int newQt, uint64_t cycle)
    {
        DiskIIEvent  e = {};

        e.category             = EventCategory::Controller;
        e.type                 = DiskIIEventType::HeadStep;
        e.cycle                = cycle;
        e.payload.step.prevQt  = prevQt;
        e.payload.step.newQt   = newQt;

        return e;
    }
}





TEST_CLASS (DebugDialogProjectionTests)
{
public:

    TEST_METHOD (FormatEvent_HeadStep_detailMatchesQtArrow)
    {
        DiskIIEvent          src = MakeStep (12, 16, 1234567);
        DiskIIEventDisplay   out;
        auto                 anchor = std::chrono::steady_clock::now();

        DebugDialogProjection::FormatEvent (src, anchor, out);

        Assert::IsTrue   (LooksLikeWallClock (out.wallStr.data()));
        Assert::AreEqual (std::wstring (L"quarter-track 12 -> 16"), out.detail);
        Assert::AreEqual (std::wstring (L"1,234,567"),   std::wstring (out.cycleStr.data()));
        Assert::IsTrue   (out.type == DiskIIEventType::HeadStep);
    }

    TEST_METHOD (FormatEvent_AddrMark_detailHasTrackSectorVolume)
    {
        DiskIIEvent          src = {};
        DiskIIEventDisplay   out;
        auto                 anchor = std::chrono::steady_clock::now();

        src.category              = EventCategory::Controller;
        src.type                  = DiskIIEventType::AddrMark;
        src.cycle                 = 0;
        src.payload.addrMark      = { 17, 5, 254 };

        DebugDialogProjection::FormatEvent (src, anchor, out);

        Assert::AreEqual (std::wstring (L"T17 S5 V254"),  out.detail);
        Assert::AreEqual (17,                              out.track);
        Assert::AreEqual (5,                               out.sector);
    }

    TEST_METHOD (FormatEvent_DataRead_detailHasSectorAndByteCount)
    {
        DiskIIEvent          src = {};
        DiskIIEventDisplay   out;
        auto                 anchor = std::chrono::steady_clock::now();

        src.type                   = DiskIIEventType::DataRead;
        src.payload.dataMark       = { 5, 256 };

        DebugDialogProjection::FormatEvent (src, anchor, out);

        Assert::AreEqual (std::wstring (L"S5 (256 bytes)"), out.detail);
        Assert::AreEqual (5,                                 out.sector);
    }

    TEST_METHOD (FormatEvent_DriveSelect_detailHasDriveAndDriveField)
    {
        DiskIIEvent          src = {};
        DiskIIEventDisplay   out;
        auto                 anchor = std::chrono::steady_clock::now();

        src.type                = DiskIIEventType::DriveSelect;
        src.payload.drive.drive = 1;

        DebugDialogProjection::FormatEvent (src, anchor, out);

        Assert::AreEqual (std::wstring (L""), out.detail);
        Assert::AreEqual (1,                          out.drive);
    }

    TEST_METHOD (FormatEvent_MotorEngaged_detailIsEmpty)
    {
        DiskIIEvent          src = {};
        DiskIIEventDisplay   out;
        auto                 anchor = std::chrono::steady_clock::now();

        src.type = DiskIIEventType::MotorEngaged;

        DebugDialogProjection::FormatEvent (src, anchor, out);

        Assert::IsTrue (out.detail.empty());
    }

    TEST_METHOD (FormatEvent_AudioStarted_detailHasKindAndDrive)
    {
        DiskIIEvent          src = {};
        DiskIIEventDisplay   out;
        auto                 anchor = std::chrono::steady_clock::now();

        src.category             = EventCategory::Audio;
        src.type                 = DiskIIEventType::AudioStarted;
        src.payload.audio.kind   = SoundKind::HeadStep;
        src.payload.audio.drive  = 0;

        DebugDialogProjection::FormatEvent (src, anchor, out);

        Assert::AreEqual (std::wstring (L"kind=HeadStep"), out.detail);
    }

    TEST_METHOD (FormatEvent_AudioSilent_detailHasKindDriveReason)
    {
        DiskIIEvent          src = {};
        DiskIIEventDisplay   out;
        auto                 anchor = std::chrono::steady_clock::now();

        src.category              = EventCategory::Audio;
        src.type                  = DiskIIEventType::AudioSilent;
        src.payload.audio.kind    = SoundKind::DoorClose;
        src.payload.audio.drive   = 1;
        src.payload.audio.reason  = SilentReason::ColdBootSuppression;

        DebugDialogProjection::FormatEvent (src, anchor, out);

        Assert::AreEqual (std::wstring (L"kind=DoorClose reason=ColdBootSuppression"),
                          out.detail);
    }

    TEST_METHOD (FormatEvent_EventsLost_detailHasCount)
    {
        DiskIIEvent          src = {};
        DiskIIEventDisplay   out;
        auto                 anchor = std::chrono::steady_clock::now();

        src.type                  = DiskIIEventType::EventsLost;
        src.payload.lost.count    = 42;

        DebugDialogProjection::FormatEvent (src, anchor, out);

        Assert::AreEqual (std::wstring (L"[42 events lost]"), out.detail);
    }

    TEST_METHOD (FormatEvent_Uptime_freshAnchor_startsAtZeroZero)
    {
        DiskIIEvent          src = MakeStep (0, 1, 0);
        DiskIIEventDisplay   out;
        auto                 anchor = std::chrono::steady_clock::now();

        DebugDialogProjection::FormatEvent (src, anchor, out);

        // MM:SS.mmm -- first two chars must be "00" since less than
        // a minute has elapsed since the anchor.
        Assert::AreEqual (L'0', out.uptimeStr[0]);
        Assert::AreEqual (L'0', out.uptimeStr[1]);
        Assert::AreEqual (L':', out.uptimeStr[2]);
    }

    TEST_METHOD (DrainAndProject_emptyRingZeroDropped_dequeUnchanged)
    {
        DiskIIEventRing                  ring;
        std::deque<DiskIIEventDisplay>   deque;
        auto                             anchor = std::chrono::steady_clock::now();

        DebugDialogProjection::DrainAndProject (ring, deque, 0, anchor);

        Assert::AreEqual (size_t (0), deque.size());
    }

    TEST_METHOD (DrainAndProject_pushesNEventsInFifoOrder)
    {
        DiskIIEventRing                  ring;
        std::deque<DiskIIEventDisplay>   deque;
        auto                             anchor = std::chrono::steady_clock::now();

        for (int i = 0; i < 5; i++)
        {
            DiskIIEvent  e = MakeStep (i, i + 1, 1000 + static_cast<uint64_t> (i));

            Assert::IsTrue (ring.TryPush (e));
        }

        DebugDialogProjection::DrainAndProject (ring, deque, 0, anchor);

        Assert::AreEqual (size_t (5), deque.size());

        for (int i = 0; i < 5; i++)
        {
            std::wstring  expected = std::format (L"quarter-track {} -> {}", i, i + 1);

            Assert::AreEqual (expected, deque[i].detail);
            Assert::IsTrue   (deque[i].type == DiskIIEventType::HeadStep);
        }
    }

    TEST_METHOD (DrainAndProject_droppedCountEmitsLeadingEventsLost)
    {
        DiskIIEventRing                  ring;
        std::deque<DiskIIEventDisplay>   deque;
        auto                             anchor = std::chrono::steady_clock::now();

        Assert::IsTrue (ring.TryPush (MakeStep (1, 2, 100)));

        DebugDialogProjection::DrainAndProject (ring, deque, 17, anchor);

        Assert::AreEqual (size_t (2),                            deque.size());
        Assert::IsTrue   (deque[0].type == DiskIIEventType::EventsLost);
        Assert::AreEqual (std::wstring (L"[17 events lost]"),    deque[0].detail);
        Assert::IsTrue   (deque[1].type == DiskIIEventType::HeadStep);
    }

    TEST_METHOD (DrainAndProject_rollingCapEnforced_oldestDroppedFromFront)
    {
        DiskIIEventRing                  ring;
        std::deque<DiskIIEventDisplay>   deque;
        auto                             anchor = std::chrono::steady_clock::now();

        // Pre-fill the deque to the cap so a single drained entry
        // is enough to force a pop_front.
        for (size_t i = 0; i < DebugDialogProjection::kDisplayDequeCap; i++)
        {
            DiskIIEventDisplay  sentinel;

            sentinel.type   = DiskIIEventType::HeadStep;
            sentinel.detail = std::format (L"old-{}", i);
            deque.push_back (std::move (sentinel));
        }

        std::wstring  firstBeforeDrain = deque.front().detail;

        Assert::IsTrue (ring.TryPush (MakeStep (7, 8, 9999)));

        DebugDialogProjection::DrainAndProject (ring, deque, 0, anchor);

        Assert::AreEqual (DebugDialogProjection::kDisplayDequeCap,           deque.size());
        Assert::AreNotEqual (firstBeforeDrain,                                deque.front().detail);
        Assert::AreEqual (std::wstring (L"quarter-track 7 -> 8"),             deque.back().detail);
    }

    TEST_METHOD (EventLabel_returnsStableStringsPerType)
    {
        Assert::AreEqual (std::wstring (L"Motor engaged"),
            std::wstring (DebugDialogProjection::EventLabel (EventCategory::Controller,
                                                              DiskIIEventType::MotorEngaged)));
        Assert::AreEqual (std::wstring (L"Audio silent"),
            std::wstring (DebugDialogProjection::EventLabel (EventCategory::Audio,
                                                              DiskIIEventType::AudioSilent)));
        Assert::AreEqual (std::wstring (L"Events lost"),
            std::wstring (DebugDialogProjection::EventLabel (EventCategory::Controller,
                                                              DiskIIEventType::EventsLost)));
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  Spec-006 T072 -- filter composition tests.
    //
    //  Build a small deque of pre-formatted display rows and exercise
    //  the FR-014 / FR-014a / FR-014b / FR-014c composition surface.
    //
    ////////////////////////////////////////////////////////////////////////////

    static DiskIIEventDisplay MakeDisplay (
        DiskIIEventType  type,
        EventCategory    cat,
        int              drive  = DiskIIEventDisplay::kFieldNotApplicable,
        int              track  = DiskIIEventDisplay::kFieldNotApplicable,
        int              sector = DiskIIEventDisplay::kFieldNotApplicable)
    {
        DiskIIEventDisplay  d;

        d.category = cat;
        d.type     = type;
        d.drive    = drive;
        d.track    = track;
        d.sector   = sector;

        return d;
    }



    TEST_METHOD (Filter_allChecked_showsEverything)
    {
        FilterState           f;
        DiskIIEventDisplay    motor    = MakeDisplay (DiskIIEventType::MotorEngaged,  EventCategory::Controller);
        DiskIIEventDisplay    step     = MakeDisplay (DiskIIEventType::HeadStep,      EventCategory::Controller);
        DiskIIEventDisplay    addr     = MakeDisplay (DiskIIEventType::AddrMark,      EventCategory::Controller, 1, 17, 5);
        DiskIIEventDisplay    audio    = MakeDisplay (DiskIIEventType::AudioStarted,  EventCategory::Audio,      1);
        DiskIIEventDisplay    loop     = MakeDisplay (DiskIIEventType::AudioLoopStarted, EventCategory::Audio,   1);

        Assert::IsTrue (MatchesFilter (motor, f));
        Assert::IsTrue (MatchesFilter (step,  f));
        Assert::IsTrue (MatchesFilter (addr,  f));
        Assert::IsTrue (MatchesFilter (audio, f));
        Assert::IsTrue (MatchesFilter (loop,  f));
    }



    TEST_METHOD (Filter_allUncheckedAndAudioOff_hidesAllNonLost)
    {
        FilterState           f;
        DiskIIEventDisplay    motor = MakeDisplay (DiskIIEventType::MotorEngaged, EventCategory::Controller);
        DiskIIEventDisplay    audio = MakeDisplay (DiskIIEventType::AudioStarted, EventCategory::Audio, 1);
        DiskIIEventDisplay    lost  = MakeDisplay (DiskIIEventType::EventsLost,   EventCategory::Controller);

        f.eventTypeMask = 0;
        f.audioMaster   = false;

        Assert::IsFalse (MatchesFilter (motor, f));
        Assert::IsFalse (MatchesFilter (audio, f));
        Assert::IsTrue  (MatchesFilter (lost,  f));    // EventsLost is never filterable
    }



    TEST_METHOD (Filter_driveOne_showsOnlyDriveOneTaggedEvents)
    {
        FilterState           f;
        // Drive radio "Drive 1" = UI value 1 = internal drive index 0.
        // Drive radio "Drive 2" = UI value 2 = internal drive index 1.
        DiskIIEventDisplay    drv1  = MakeDisplay (DiskIIEventType::DriveSelect, EventCategory::Controller, 0);
        DiskIIEventDisplay    drv2  = MakeDisplay (DiskIIEventType::DriveSelect, EventCategory::Controller, 1);
        DiskIIEventDisplay    motor = MakeDisplay (DiskIIEventType::MotorEngaged, EventCategory::Controller);

        f.driveFilter = 1;    // "Drive 1" -> internal 0

        Assert::IsTrue  (MatchesFilter (drv1,  f));
        Assert::IsFalse (MatchesFilter (drv2,  f));
        // Motor events have no drive field -> drive predicate bypasses
        // (events on the shared spindle aren't drive-tagged; hiding
        // them when filtering by drive would hide most of the stream).
        Assert::IsTrue  (MatchesFilter (motor, f));

        f.driveFilter = 2;    // "Drive 2" -> internal 1
        Assert::IsFalse (MatchesFilter (drv1, f));
        Assert::IsTrue  (MatchesFilter (drv2, f));

        f.driveFilter = 0;    // "All"
        Assert::IsTrue (MatchesFilter (drv1,  f));
        Assert::IsTrue (MatchesFilter (drv2,  f));
        Assert::IsTrue (MatchesFilter (motor, f));
    }



    TEST_METHOD (Filter_trackList_showsOnlyMatchingAddrMarks)
    {
        FilterState           f;
        DiskIIEventDisplay    t0  = MakeDisplay (DiskIIEventType::AddrMark, EventCategory::Controller, 1, 0,  0);
        DiskIIEventDisplay    t1  = MakeDisplay (DiskIIEventType::AddrMark, EventCategory::Controller, 1, 1,  0);
        DiskIIEventDisplay    t17 = MakeDisplay (DiskIIEventType::AddrMark, EventCategory::Controller, 1, 17, 0);
        DiskIIEventDisplay    t5  = MakeDisplay (DiskIIEventType::AddrMark, EventCategory::Controller, 1, 5,  0);

        f.trackFilter = TrackSectorPredicate::Parse (L"0-2,17", false);

        Assert::IsTrue  (MatchesFilter (t0,  f));
        Assert::IsTrue  (MatchesFilter (t1,  f));
        Assert::IsTrue  (MatchesFilter (t17, f));
        Assert::IsFalse (MatchesFilter (t5,  f));
    }



    TEST_METHOD (Filter_sectorOutOfRangeValue_matchesNothing_doesNotThrow)
    {
        FilterState           f;
        DiskIIEventDisplay    s0  = MakeDisplay (DiskIIEventType::DataRead, EventCategory::Controller, 1, DiskIIEventDisplay::kFieldNotApplicable, 0);

        f.sectorFilter = TrackSectorPredicate::Parse (L"999", false);

        Assert::IsFalse (MatchesFilter (s0, f));
    }



    TEST_METHOD (Filter_combined_driveAndTrackAndReadOnlyAndAudioOff)
    {
        FilterState           f;
        // driveFilter == 1 means "Drive 1" in the UI; the underlying
        // event index for that selection is 0. Read events from drive 0
        // must pass the predicate.
        DiskIIEventDisplay    matching   = MakeDisplay (DiskIIEventType::DataRead, EventCategory::Controller, 0, DiskIIEventDisplay::kFieldNotApplicable, 0);
        DiskIIEventDisplay    wrongDrive = MakeDisplay (DiskIIEventType::DataRead, EventCategory::Controller, 1, DiskIIEventDisplay::kFieldNotApplicable, 0);
        DiskIIEventDisplay    motor      = MakeDisplay (DiskIIEventType::MotorEngaged, EventCategory::Controller);
        DiskIIEventDisplay    audio      = MakeDisplay (DiskIIEventType::AudioStarted, EventCategory::Audio, 0);

        f.eventTypeMask = FilterState::kEventCatRead;
        f.driveFilter   = 1;
        f.audioMaster   = false;

        Assert::IsTrue  (MatchesFilter (matching,   f));
        Assert::IsFalse (MatchesFilter (wrongDrive, f));
        // motor has no drive field; eventTypeMask gates it out (Read only).
        Assert::IsFalse (MatchesFilter (motor,      f));
        Assert::IsFalse (MatchesFilter (audio,      f));
    }



    TEST_METHOD (Filter_audioMasterOff_hidesAllSixAudioOutcomes_butLoopsToo)
    {
        FilterState           f;

        f.audioMaster = false;

        Assert::IsFalse (MatchesFilter (MakeDisplay (DiskIIEventType::AudioStarted,     EventCategory::Audio, 1), f));
        Assert::IsFalse (MatchesFilter (MakeDisplay (DiskIIEventType::AudioRestarted,   EventCategory::Audio, 1), f));
        Assert::IsFalse (MatchesFilter (MakeDisplay (DiskIIEventType::AudioContinued,   EventCategory::Audio, 1), f));
        Assert::IsFalse (MatchesFilter (MakeDisplay (DiskIIEventType::AudioSilent,      EventCategory::Audio, 1), f));
        Assert::IsFalse (MatchesFilter (MakeDisplay (DiskIIEventType::AudioLoopStarted, EventCategory::Audio, 1), f));
        Assert::IsFalse (MatchesFilter (MakeDisplay (DiskIIEventType::AudioLoopStopped, EventCategory::Audio, 1), f));
    }



    TEST_METHOD (Filter_audioMasterOnSilentOnly_showsSilentAndLoops_hidesOthers)
    {
        FilterState           f;

        f.audioStarted    = false;
        f.audioRestarted  = false;
        f.audioContinued  = false;
        f.audioSilent     = true;

        Assert::IsFalse (MatchesFilter (MakeDisplay (DiskIIEventType::AudioStarted,     EventCategory::Audio, 1), f));
        Assert::IsFalse (MatchesFilter (MakeDisplay (DiskIIEventType::AudioRestarted,   EventCategory::Audio, 1), f));
        Assert::IsFalse (MatchesFilter (MakeDisplay (DiskIIEventType::AudioContinued,   EventCategory::Audio, 1), f));
        Assert::IsTrue  (MatchesFilter (MakeDisplay (DiskIIEventType::AudioSilent,      EventCategory::Audio, 1), f));
        // Loops are gated only by audioMaster, never by sub-toggles.
        Assert::IsTrue  (MatchesFilter (MakeDisplay (DiskIIEventType::AudioLoopStarted, EventCategory::Audio, 1), f));
        Assert::IsTrue  (MatchesFilter (MakeDisplay (DiskIIEventType::AudioLoopStopped, EventCategory::Audio, 1), f));
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  Spec-006 T082 -- Pause / Clear behavior at the projection layer.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (PauseInducedOverflow_singleLostMarkerWithCoalescedCount)
    {
        DiskIIEventRing                  ring;
        std::deque<DiskIIEventDisplay>   deque;
        uint32_t                         dropped       = 0;
        uint64_t                         totalPushed   = 8192;
        uint64_t                         pushAttempted = 0;
        auto                             anchor        = std::chrono::steady_clock::now();

        // Producer never gets drained -> ring fills, every push past
        // capacity returns false.
        for (pushAttempted = 0; pushAttempted < totalPushed; pushAttempted++)
        {
            DiskIIEvent  e = MakeStep (1, 2, pushAttempted);

            if (!ring.TryPush (e))
            {
                dropped++;
            }
        }

        Assert::AreEqual (totalPushed - DiskIIEventRing::kEventRingCapacity,
                          static_cast<uint64_t> (dropped));

        DebugDialogProjection::DrainAndProject (ring, deque, dropped, anchor);

        // Exactly one EventsLost marker; the rest are the surviving
        // DrainBatch entries.
        size_t  lostCount = 0;

        for (size_t i = 0; i < deque.size(); i++)
        {
            if (deque[i].type == DiskIIEventType::EventsLost)
            {
                lostCount++;
            }
        }

        Assert::AreEqual (size_t (1), lostCount);
        Assert::AreEqual (DiskIIEventRing::kEventRingCapacity + 1, static_cast<uint32_t> (deque.size()));
    }



    TEST_METHOD (ClearWithInFlight_inFlightDrainsIntoEmptyDequeWithNoMarker)
    {
        DiskIIEventRing                  ring;
        std::deque<DiskIIEventDisplay>   deque;
        uint32_t                         dropped     = 0;
        int                              i           = 0;
        auto                             anchor      = std::chrono::steady_clock::now();

        // Phase 1: 50 events drained into the deque.
        for (i = 0; i < 50; i++)
        {
            Assert::IsTrue (ring.TryPush (MakeStep (i, i + 1, static_cast<uint64_t> (i))));
        }

        DebugDialogProjection::DrainAndProject (ring, deque, 0, anchor);
        Assert::AreEqual (size_t (50), deque.size());

        // Phase 2: 100 more events pushed but NOT drained (simulating
        // events arriving while the UI is otherwise busy).
        for (i = 0; i < 100; i++)
        {
            Assert::IsTrue (ring.TryPush (MakeStep (i, i + 1, 100 + static_cast<uint64_t> (i))));
        }

        // Simulate the Clear button: clear deque, leave the ring alone,
        // leave dropped at 0.
        deque.clear();
        Assert::AreEqual (size_t (0), deque.size());

        DebugDialogProjection::DrainAndProject (ring, deque, dropped, anchor);

        Assert::AreEqual (size_t (100), deque.size());

        for (size_t k = 0; k < deque.size(); k++)
        {
            Assert::IsFalse (deque[k].type == DiskIIEventType::EventsLost);
        }
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  Spec-006 T091 -- clipboard formatter (BuildClipboardText)
    //
    ////////////////////////////////////////////////////////////////////////////

    static DiskIIEventDisplay MakeFormattedDisplay (
        DiskIIEventType  type,
        EventCategory    cat,
        const wchar_t *  wall,
        const wchar_t *  uptime,
        const wchar_t *  cycle,
        const wchar_t *  detail)
    {
        DiskIIEventDisplay  d;

        d.category = cat;
        d.type     = type;

        wcsncpy_s (d.wallStr.data   (), d.wallStr.size   (), wall,   _TRUNCATE);
        wcsncpy_s (d.uptimeStr.data(), d.uptimeStr.size(), uptime, _TRUNCATE);
        wcsncpy_s (d.cycleStr.data  (), d.cycleStr.size  (), cycle,  _TRUNCATE);

        d.detail = detail;
        return d;
    }



    TEST_METHOD (BuildClipboardText_singleRow_allColumns_tabSeparatedCrlfTerminated)
    {
        std::array<LogicalColumn, kColumnCount>  columns = {};
        DiskIIEventDisplay            row     = MakeFormattedDisplay (
                                                    DiskIIEventType::HeadStep,
                                                    EventCategory::Controller,
                                                    L"12:34:56.789",
                                                    L"00:01.234",
                                                    L"1,000",
                                                    L"quarter-track 4 -> 5");
        std::vector<const DiskIIEventDisplay *>  selected;

        SeedDefaultColumns (columns);
        selected.push_back (&row);

        std::wstring  out = BuildClipboardText (selected, columns);

        Assert::AreEqual (
            std::wstring (L"12:34:56.789\t00:01.234\t1,000\t\tHead step\tquarter-track 4 -> 5\r\n"),
            out);
    }



    TEST_METHOD (BuildClipboardText_hiddenColumns_areOmittedIncludingLeadingTab)
    {
        std::array<LogicalColumn, kColumnCount>  columns = {};
        DiskIIEventDisplay            row     = MakeFormattedDisplay (
                                                    DiskIIEventType::AddrMark,
                                                    EventCategory::Controller,
                                                    L"WALL",
                                                    L"UPTIME",
                                                    L"42",
                                                    L"T0 S0 V254");
        std::vector<const DiskIIEventDisplay *>  selected;

        SeedDefaultColumns (columns);

        // Hide Time, Cycle count, and Drive. Expected order: Uptime, Event, Detail.
        columns[0].visible = false;    // Time
        columns[2].visible = false;    // Cycle count
        columns[3].visible = false;    // Drive

        selected.push_back (&row);

        std::wstring  out = BuildClipboardText (selected, columns);

        Assert::AreEqual (std::wstring (L"UPTIME\tAddress mark\tT0 S0 V254\r\n"), out);
    }



    TEST_METHOD (BuildClipboardText_multipleRows_eachOnItsOwnCrlfTerminatedLine)
    {
        std::array<LogicalColumn, kColumnCount>  columns = {};
        DiskIIEventDisplay            r0      = MakeFormattedDisplay (
                                                    DiskIIEventType::MotorEngaged,
                                                    EventCategory::Controller,
                                                    L"00:00:00.000",
                                                    L"00:00.000",
                                                    L"0",
                                                    L"");
        DiskIIEventDisplay            r1      = MakeFormattedDisplay (
                                                    DiskIIEventType::HeadBump,
                                                    EventCategory::Controller,
                                                    L"00:00:00.500",
                                                    L"00:00.500",
                                                    L"1,000",
                                                    L"at quarter-track 0");
        std::vector<const DiskIIEventDisplay *>  selected;

        SeedDefaultColumns (columns);
        selected.push_back (&r0);
        selected.push_back (&r1);

        std::wstring  out = BuildClipboardText (selected, columns);

        Assert::AreEqual (
            std::wstring (L"00:00:00.000\t00:00.000\t0\t\tMotor engaged\t\r\n"
                          L"00:00:00.500\t00:00.500\t1,000\t\tHead bump\tat quarter-track 0\r\n"),
            out);
    }



    TEST_METHOD (FormatEvent_uptimeAnchorJustReset_emitsZeroMinutesZeroSeconds)
    {
        // Spec-006 T103b / FR-004a / SC-015. Simulate the
        // ResetUptimeAnchor path: capture the anchor "now", let a
        // sliver of wall time elapse, format an event, and assert
        // the Uptime column reads 00:00.xxx (not whatever the
        // pre-reset value was).
        std::chrono::steady_clock::time_point  anchor      = std::chrono::steady_clock::now();
        DiskIIEvent                            src         = {};
        DiskIIEventDisplay                     out         = {};
        const wchar_t                       *  uptimeText  = nullptr;

        std::this_thread::sleep_for (std::chrono::milliseconds (5));

        src.type            = DiskIIEventType::MotorEngaged;
        src.category        = EventCategory::Controller;
        src.cycle           = 0;

        DebugDialogProjection::FormatEvent (src, anchor, out);

        uptimeText = out.uptimeStr.data();

        Assert::IsNotNull (uptimeText);

        // FR-005 uptime shape: MM:SS.mmm. A just-reset anchor must
        // start with "00:00." regardless of however many ms the
        // sleep slipped by.
        Assert::AreEqual (L'0', uptimeText[0]);
        Assert::AreEqual (L'0', uptimeText[1]);
        Assert::AreEqual (L':', uptimeText[2]);
        Assert::AreEqual (L'0', uptimeText[3]);
        Assert::AreEqual (L'0', uptimeText[4]);
        Assert::AreEqual (L'.', uptimeText[5]);
    }
};

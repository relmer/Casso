#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "DebugDialogProjection.h"

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
        Assert::AreEqual (std::wstring (L"qt=12 -> 16"), out.detail);
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

        Assert::AreEqual (std::wstring (L"drive=1"), out.detail);
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

        Assert::AreEqual (std::wstring (L"kind=HeadStep drive=0"), out.detail);
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

        Assert::AreEqual (std::wstring (L"kind=DoorClose drive=1 reason=ColdBootSuppression"),
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
            std::wstring  expected = std::format (L"qt={} -> {}", i, i + 1);

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
        Assert::AreEqual (std::wstring (L"qt=7 -> 8"),                        deque.back().detail);
    }

    TEST_METHOD (EventLabel_returnsStableStringsPerType)
    {
        Assert::AreEqual (std::wstring (L"MOTOR ENGAGED"),
            std::wstring (DebugDialogProjection::EventLabel (EventCategory::Controller,
                                                              DiskIIEventType::MotorEngaged)));
        Assert::AreEqual (std::wstring (L"AUDIO SILENT"),
            std::wstring (DebugDialogProjection::EventLabel (EventCategory::Audio,
                                                              DiskIIEventType::AudioSilent)));
        Assert::AreEqual (std::wstring (L"EVENTS LOST"),
            std::wstring (DebugDialogProjection::EventLabel (EventCategory::Controller,
                                                              DiskIIEventType::EventsLost)));
    }
};

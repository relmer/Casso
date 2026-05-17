#include "Pch.h"

#include "DiskIIDebugDialogState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope column defaults
//
////////////////////////////////////////////////////////////////////////////////

static const wchar_t * const  s_kpszColumnHeaders[kColumnCount] =
{
    L"Wall",
    L"Uptime",
    L"Cycle",
    L"Event",
    L"Detail",
};

static const int              s_kColumnDefaultWidths[kColumnCount] =
{
    kColWallWidth,
    kColUptimeWidth,
    kColCycleWidth,
    kColEventWidth,
    kColDetailWidth,
};





////////////////////////////////////////////////////////////////////////////////
//
//  SeedDefaultColumns
//
//  Populate the dialog's logical column model with the five spec-006
//  columns in fixed id order. All columns default to visible with
//  autoSizedYet = false so the first ShowColumn pass runs the FR-027
//  auto-size-to-header step.
//
////////////////////////////////////////////////////////////////////////////////

void SeedDefaultColumns (std::array<LogicalColumn, 5> & columns) noexcept
{
    int  i = 0;

    for (i = 0; i < kColumnCount; i++)
    {
        columns[i].id            = i;
        columns[i].headerText    = s_kpszColumnHeaders[i];
        columns[i].defaultWidth  = s_kColumnDefaultWidths[i];
        columns[i].savedWidth    = s_kColumnDefaultWidths[i];
        columns[i].visible       = true;
        columns[i].autoSizedYet  = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeWasAtTail
//
//  Auto-tail decision per plan.md "Auto-Tail Scroll Algorithm". The
//  dialog calls this BEFORE the projection mutates the deque so the
//  pre-drain visible-window position decides whether to ensure-visible
//  on the new last row.
//
//  Rules:
//      * Empty list           -> at tail (vacuously true).
//      * Visible last row     >= totalCount - 1 -> at tail.
//      * Anything scrolled-up -> not at tail.
//
////////////////////////////////////////////////////////////////////////////////

bool ComputeWasAtTail (int topIndex, int countPerPage, int totalCount) noexcept
{
    if (totalCount <= 0)
    {
        return true;
    }

    return (topIndex + countPerPage) >= totalCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventTypeCategoryBit
//
//  Map a DiskIIEventType to its FR-014 checkbox-category bit. Audio
//  event types return 0 (audio gating is handled separately by the
//  audioMaster + sub-toggle path). The EventsLost synthetic also
//  returns 0; the filter treats it as always-shown.
//
////////////////////////////////////////////////////////////////////////////////

static uint32_t EventTypeCategoryBit (DiskIIEventType type) noexcept
{
    switch (type)
    {
        case DiskIIEventType::MotorCommandOn:
        case DiskIIEventType::MotorEngaged:
        case DiskIIEventType::MotorCommandOff:
        case DiskIIEventType::MotorDisengaged:    return FilterState::kEventCatMotor;

        case DiskIIEventType::HeadStep:           return FilterState::kEventCatHeadStep;
        case DiskIIEventType::HeadBump:           return FilterState::kEventCatHeadBump;
        case DiskIIEventType::AddrMark:           return FilterState::kEventCatAddrMark;
        case DiskIIEventType::DataRead:           return FilterState::kEventCatRead;
        case DiskIIEventType::DataWrite:          return FilterState::kEventCatWrite;

        case DiskIIEventType::DiskInserted:
        case DiskIIEventType::DiskEjected:        return FilterState::kEventCatDoor;

        case DiskIIEventType::DriveSelect:        return FilterState::kEventCatDriveSelect;

        default:                                  return 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MatchesAudioSubToggle
//
//  Audio-side gating per FR-014c. Loop events are gated ONLY by the
//  audio master; the four "one-shot" outcomes (Started / Restarted /
//  Continued / Silent) also honor their per-outcome sub-toggle.
//
////////////////////////////////////////////////////////////////////////////////

static bool MatchesAudioSubToggle (DiskIIEventType type, const FilterState & f) noexcept
{
    switch (type)
    {
        case DiskIIEventType::AudioStarted:        return f.audioStarted;
        case DiskIIEventType::AudioRestarted:      return f.audioRestarted;
        case DiskIIEventType::AudioContinued:      return f.audioContinued;
        case DiskIIEventType::AudioSilent:         return f.audioSilent;
        case DiskIIEventType::AudioLoopStarted:    return true;
        case DiskIIEventType::AudioLoopStopped:    return true;
        default:                                   return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MatchesFilter
//
//  Compose the FR-014 filter (event-type / drive / track / sector /
//  audio) over one DiskIIEventDisplay. Synthetic EventsLost rows are
//  always shown so the overflow marker is never filterable.
//
//  Field-absent rule: when a display row's track or sector field is
//  kFieldNotApplicable, the corresponding text predicate is bypassed
//  (an event with no track cannot be track-rejected). Drive is
//  symmetric.
//
////////////////////////////////////////////////////////////////////////////////

bool MatchesFilter (const DiskIIEventDisplay & e, const FilterState & f) noexcept
{
    uint32_t  catBit = 0;

    if (e.type == DiskIIEventType::EventsLost)
    {
        return true;
    }

    if (e.category == EventCategory::Audio)
    {
        if (!f.audioMaster)
        {
            return false;
        }

        if (!MatchesAudioSubToggle (e.type, f))
        {
            return false;
        }
    }
    else
    {
        catBit = EventTypeCategoryBit (e.type);

        if (catBit != 0 && (f.eventTypeMask & catBit) == 0)
        {
            return false;
        }
    }

    if (f.driveFilter != 0)
    {
        if (e.drive == DiskIIEventDisplay::kFieldNotApplicable)
        {
            return false;
        }

        if (e.drive != f.driveFilter)
        {
            return false;
        }
    }

    if (e.track != DiskIIEventDisplay::kFieldNotApplicable)
    {
        if (!f.trackFilter.Matches (e.track))
        {
            return false;
        }
    }

    if (e.sector != DiskIIEventDisplay::kFieldNotApplicable)
    {
        if (!f.sectorFilter.Matches (e.sector))
        {
            return false;
        }
    }

    return true;
}

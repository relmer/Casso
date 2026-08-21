#include "Pch.h"

#include "Disk2DebugDialogState.h"
#include "DebugDialogProjection.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope column defaults
//
////////////////////////////////////////////////////////////////////////////////

static const wchar_t * const  s_kpszColumnHeaders[kColumnCount] =
{
    L"Time",
    L"Uptime",
    L"Cycle count",
    L"Drive",
    L"Event",
    L"Detail",
};

static const int              s_kColumnDefaultWidths[kColumnCount] =
{
    kColWallWidth,
    kColUptimeWidth,
    kColCycleWidth,
    kColDriveWidth,
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

void SeedDefaultColumns (std::array<LogicalColumn, kColumnCount> & columns) noexcept
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
        columns[i].userResized   = false;
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
    // An empty list is vacuously at tail, which is what keeps the first
    // arriving row from being treated as a scroll-away.
    return totalCount <= 0
        || (topIndex + countPerPage) >= totalCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventTypeCategoryBit
//
//  Map a Disk2EventType to its FR-014 checkbox-category bit. Audio
//  event types return 0 (audio gating is handled separately by the
//  audioMaster + sub-toggle path). The EventsLost synthetic also
//  returns 0; the filter treats it as always-shown.
//
////////////////////////////////////////////////////////////////////////////////

static uint32_t EventTypeCategoryBit (Disk2EventType type) noexcept
{
    // 0 means "no checkbox category", which the filter reads as always-shown.
    // Audio types and the EventsLost synthetic both land there via `default`.
    uint32_t  bit = 0;



    switch (type)
    {
        case Disk2EventType::MotorCommandOn:
        case Disk2EventType::MotorEngaged:
        case Disk2EventType::MotorCommandOff:
        case Disk2EventType::MotorDisengaged:    bit = FilterState::kEventCatMotor;       break;

        case Disk2EventType::HeadStep:           bit = FilterState::kEventCatHeadStep;    break;
        case Disk2EventType::HeadBump:           bit = FilterState::kEventCatHeadBump;    break;
        case Disk2EventType::AddrMark:           bit = FilterState::kEventCatAddrMark;    break;
        case Disk2EventType::DataRead:           bit = FilterState::kEventCatRead;        break;
        case Disk2EventType::DataWrite:          bit = FilterState::kEventCatWrite;       break;

        case Disk2EventType::DiskInserted:
        case Disk2EventType::DiskEjected:        bit = FilterState::kEventCatDoor;        break;

        case Disk2EventType::DriveSelect:        bit = FilterState::kEventCatDriveSelect; break;

        default:                                                                          break;
    }

    return bit;
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

static bool MatchesAudioSubToggle (Disk2EventType type, const FilterState & f) noexcept
{
    // Loop events (and anything non-audio that reaches here) have no
    // sub-toggle of their own, so they pass -- the audio master already
    // gated them.
    bool  shown = true;



    switch (type)
    {
        case Disk2EventType::AudioStarted:     shown = f.audioStarted;   break;
        case Disk2EventType::AudioRestarted:   shown = f.audioRestarted; break;
        case Disk2EventType::AudioContinued:   shown = f.audioContinued; break;
        case Disk2EventType::AudioSilent:      shown = f.audioSilent;    break;

        case Disk2EventType::AudioLoopStarted:
        case Disk2EventType::AudioLoopStopped:
        default:                                                         break;
    }

    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MatchesFilter
//
//  Compose the FR-014 filter (event-type / drive / track / sector /
//  audio) over one Disk2EventDisplay. Synthetic EventsLost rows are
//  always shown so the overflow marker is never filterable.
//
//  Field-absent rule: when a display row's track or sector field is
//  kFieldNotApplicable, the corresponding text predicate is bypassed
//  (an event with no track cannot be track-rejected). Drive is
//  symmetric.
//
////////////////////////////////////////////////////////////////////////////////

bool MatchesFilter (const Disk2EventDisplay & e, const FilterState & f) noexcept
{
    uint32_t  catBit = 0;
    bool      shown  = true;



    // Synthetic overflow markers are never filterable -- losing the "N events
    // lost" row to a filter would hide the fact that anything was lost.
    if (e.type != Disk2EventType::EventsLost)
    {
        if (e.category == EventCategory::Audio)
        {
            // Master first, then the per-outcome sub-toggle.
            shown = f.audioMaster && MatchesAudioSubToggle (e.type, f);
        }
        else
        {
            // A type with no category bit has no checkbox and is always shown.
            catBit = EventTypeCategoryBit (e.type);
            shown  = (catBit == 0) || (f.eventTypeMask & catBit) != 0;
        }

        if (shown && f.driveFilter != 0)
        {
            // driveFilter is the 1-based UI selection (1 = Drive 1, 2 = Drive
            // 2); event.drive is the 0-based internal index (matches
            // Disk2Controller::m_activeDrive).
            //
            // A row with NO drive is rejected outright rather than bypassed.
            // Spec-006 bug fix: the previous "bypass" rule masked events that
            // legitimately had no drive_index stamped because of producer-side
            // oversights. Every real controller / audio event now stamps its
            // drive at fire time, so reaching here with kFieldNotApplicable
            // means "no drive at all" -- which a non-All radio never matches.
            shown = (e.drive != Disk2EventDisplay::kFieldNotApplicable)
                    && (e.drive == (f.driveFilter - 1));
        }

        // Field-absent rule: a row with no track (or sector) cannot be
        // track-rejected, so the predicate is bypassed rather than failed.
        // This is the opposite of the drive rule directly above, because
        // those fields are legitimately absent on many event types.
        if (shown && e.track != Disk2EventDisplay::kFieldNotApplicable)
        {
            shown = f.trackFilter.Matches (e.track);
        }

        if (shown && e.sector != Disk2EventDisplay::kFieldNotApplicable)
        {
            shown = f.sectorFilter.Matches (e.sector);
        }
    }

    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendColumnText
//
//  Append a single row's value for the logical column id to `out`.
//  Wall / Uptime / Cycle / Detail come straight off the display
//  record; Event resolves via DebugDialogProjection::EventLabel.
//
////////////////////////////////////////////////////////////////////////////////

void AppendColumnText (std::wstring & out, const Disk2EventDisplay & e, int logicalId)
{
    std::wstring_view  label;
    wchar_t            driveBuf[4] = {};



    switch (logicalId)
    {
        case 0: out.append (e.wallStr.data()); break;
        case 1: out.append (e.uptimeStr.data()); break;
        case 2: out.append (e.cycleStr.data()); break;
        case 3:
            if (e.drive != Disk2EventDisplay::kFieldNotApplicable)
            {
                swprintf_s (driveBuf, L"%d", e.drive + 1);
                out.append (driveBuf);
            }

            break;
        case 4:
            label = DebugDialogProjection::EventLabel (e.category, e.type);
            out.append (label);
            break;
        case 5: out.append (e.detail); break;
        default: break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildClipboardText
//
//  Tab-separated rows in visible-column order, CRLF terminator.
//  Hidden columns are omitted entirely -- no leading tab placeholder,
//  no spacer string -- per FR-026.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BuildClipboardText (
    const std::vector<const Disk2EventDisplay *> &  selected,
    const std::array<LogicalColumn, kColumnCount> &  columns)
{
    std::wstring  out;
    int           colIdx       = 0;
    bool          firstColumn  = true;



    for (const Disk2EventDisplay * row : selected)
    {
        if (row == nullptr)
        {
            continue;
        }

        firstColumn = true;

        for (colIdx = 0; colIdx < kColumnCount; colIdx++)
        {
            if (!columns[colIdx].visible)
            {
                continue;
            }

            if (!firstColumn)
            {
                out.push_back (L'\t');
            }

            AppendColumnText (out, *row, columns[colIdx].id);
            firstColumn = false;
        }

        out.append (L"\r\n");
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlanVisibleColumns
//
//  Spec-006 T108 / FR-026 / FR-027. Pure planner: walks the logical
//  column model in id order, emits a VisibleColumnSpec for each
//  visible entry carrying the width the ListView should use and a
//  needsAutoSize flag set iff this column has never been auto-sized
//  yet. The caller (RebuildListViewColumns on Win32, the test
//  fixture in Disk2DebugDialogColumnTests headless) consumes the
//  vector to either drive a real LV or assert the plan's contents.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<VisibleColumnSpec> PlanVisibleColumns (
    const std::array<LogicalColumn, kColumnCount> & model) noexcept
{
    std::vector<VisibleColumnSpec>  out;
    int                             i = 0;



    out.reserve (kColumnCount);

    for (i = 0; i < kColumnCount; i++)
    {
        VisibleColumnSpec  spec = {};

        if (!model[i].visible)
        {
            continue;
        }

        spec.id            = model[i].id;
        spec.headerText    = model[i].headerText;
        spec.width         = model[i].savedWidth;
        spec.needsAutoSize = !model[i].autoSizedYet;

        out.push_back (spec);
    }

    return out;
}


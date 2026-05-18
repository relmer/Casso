#include "Pch.h"

#include "DiskIIDebugDialogState.h"
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
            // Events without a drive index (motor, head, addr-mark,
            // data-read on the shared spindle) are tagged by the
            // controller fire-site as "no drive specified". Drive
            // radio filter bypasses them so selecting "Drive 1"
            // doesn't hide the read traffic on drive 0 just because
            // those events aren't carrying a redundant drive tag.
            // Symmetric with the track / sector predicates below.
        }
        else if (e.drive != (f.driveFilter - 1))
        {
            // driveFilter is the 1-based UI selection (1 = Drive 1,
            // 2 = Drive 2); event.drive is the 0-based internal index
            // (matches DiskIIController::m_activeDrive). The off-by-one
            // here is the spec-006 bug-fix: previously the filter
            // compared identical values and "Drive 1" hid every audio
            // event (drive=0) on the only physical drive that ever
            // boots.
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





////////////////////////////////////////////////////////////////////////////////
//
//  AppendColumnText
//
//  Append a single row's value for the logical column id to `out`.
//  Wall / Uptime / Cycle / Detail come straight off the display
//  record; Event resolves via DebugDialogProjection::EventLabel.
//
////////////////////////////////////////////////////////////////////////////////

void AppendColumnText (std::wstring & out, const DiskIIEventDisplay & e, int logicalId)
{
    std::wstring_view  label;
    wchar_t            driveBuf[4] = {};

    switch (logicalId)
    {
        case 0: out.append (e.wallStr.data   ()); break;
        case 1: out.append (e.uptimeStr.data()); break;
        case 2: out.append (e.cycleStr.data  ()); break;
        case 3:
            if (e.drive != DiskIIEventDisplay::kFieldNotApplicable)
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
    const std::vector<const DiskIIEventDisplay *> &  selected,
    const std::array<LogicalColumn, kColumnCount> &  columns)
{
    std::wstring  out;
    size_t        rowIdx       = 0;
    int           colIdx       = 0;
    bool          firstColumn  = true;

    for (rowIdx = 0; rowIdx < selected.size(); rowIdx++)
    {
        const DiskIIEventDisplay *  row = selected[rowIdx];

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
//  fixture in DiskIIDebugDialogColumnTests headless) consumes the
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


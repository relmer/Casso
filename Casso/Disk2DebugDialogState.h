#pragma once

#include "Pch.h"

#include "../CassoEmuCore/Devices/Disk2Event.h"
#include "Disk2EventDisplay.h"
#include "TrackSectorPredicate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugDialogState
//
//  Non-Win32 pieces of the spec-006 Disk II Debug dialog. Factored out
//  of Disk2DebugDialog.cpp so the headless UnitTest project can link
//  them without dragging in CreateWindowEx, ListView, RichEdit, etc.
//
//  Public surface:
//      LogicalColumn         -- per-column model (id / header / widths /
//                               visibility / first-auto-size flag) per
//                               FR-026 / FR-027.
//      FilterState           -- composed filter (event-type mask, drive
//                               radio, track/sector predicates, audio
//                               master + sub-toggles, raw-qt flag) per
//                               FR-014 / FR-014a..e.
//      SeedDefaultColumns    -- populates m_columns with the spec-006
//                               default widths and labels.
//      ComputeWasAtTail      -- pure auto-tail decision per FR-013.
//      MatchesFilter         -- applies the composed filter to one
//                               display row.
//
////////////////////////////////////////////////////////////////////////////////

struct LogicalColumn
{
    int        id;
    LPCWSTR    headerText;
    int        defaultWidth;
    int        savedWidth;
    bool       visible;
    bool       autoSizedYet;

    // Spec-006 bug fix. Set true the moment HDN_ENDTRACK fires on
    // this column header so the periodic auto-grow check no longer
    // tries to widen it past the user's chosen width. Cleared on
    // ClearEvents() / SoftReset so a fresh boot re-runs the fit.
    bool       userResized;
};



struct FilterState
{
    static constexpr uint32_t  kEventCatMotor       = 1u << 0;
    static constexpr uint32_t  kEventCatHeadStep    = 1u << 1;
    static constexpr uint32_t  kEventCatHeadBump    = 1u << 2;
    static constexpr uint32_t  kEventCatAddrMark    = 1u << 3;
    static constexpr uint32_t  kEventCatRead        = 1u << 4;
    static constexpr uint32_t  kEventCatWrite       = 1u << 5;
    static constexpr uint32_t  kEventCatDoor        = 1u << 6;
    static constexpr uint32_t  kEventCatDriveSelect = 1u << 7;
    static constexpr uint32_t  kEventCatAllMask     = 0xFFu;

    uint32_t              eventTypeMask     = kEventCatAllMask;
    int                   driveFilter       = 0;     // 0 = All, 1 = Drive 1, 2 = Drive 2
    TrackSectorPredicate  trackFilter;
    TrackSectorPredicate  sectorFilter;
    bool                  audioMaster       = true;
    bool                  audioStarted      = true;
    bool                  audioRestarted    = true;
    bool                  audioContinued    = true;
    bool                  audioSilent       = true;
    bool                  trackFilterRawQt  = false;
};



// FR-004 column widths (sum ~= 850 px).
constexpr int  kColWallWidth   = 110;
constexpr int  kColUptimeWidth = 90;
constexpr int  kColCycleWidth  = 110;
constexpr int  kColDriveWidth  = 56;
constexpr int  kColEventWidth  = 130;
constexpr int  kColDetailWidth = 360;
constexpr int  kColumnCount    = 6;

// Logical id of the Detail column. It is the rightmost column and
// gets special-cased everywhere: it always flex-fills the LV client
// remainder rather than being content-sized, so the auto-grow path
// must skip it (otherwise the remainder math fights the per-cell
// measurement).
constexpr int  kDetailColumnId = kColumnCount - 1;



void   SeedDefaultColumns (std::array<LogicalColumn, kColumnCount> & columns) noexcept;
bool   ComputeWasAtTail   (int topIndex, int countPerPage, int totalCount) noexcept;
bool   MatchesFilter      (const Disk2EventDisplay & e, const FilterState & f) noexcept;





////////////////////////////////////////////////////////////////////////////////
//
//  VisibleColumnSpec / PlanVisibleColumns
//
//  Pure helper extracted for spec-006 T108. Given the in-memory
//  LogicalColumn model, produce the ordered list of columns the
//  ListView should hold (one entry per visible column, in id order).
//  Each entry carries the LV-side width to use (savedWidth, with
//  autoSizedYet flagging "first show -- caller should run the
//  LVSCW_AUTOSIZE_USEHEADER pass"). Tests assert the planner's
//  semantics without instantiating a real ListView.
//
////////////////////////////////////////////////////////////////////////////////

struct VisibleColumnSpec
{
    int        id;
    LPCWSTR    headerText;
    int        width;
    bool       needsAutoSize;
};

std::vector<VisibleColumnSpec> PlanVisibleColumns (
    const std::array<LogicalColumn, kColumnCount> & model) noexcept;

// Phase 9: format a clipboard payload from a vector of selected
// display rows. Tab-separated UTF-16 in visible-column order (hidden
// columns -- and their leading tabs -- are omitted per FR-026), CRLF
// row terminator. Pure helper; testable without a real clipboard or
// HWND.
// Spec-006. Append the text representation of one display cell to
// `out`. Exposed for the dialog's content-aware column auto-sizer
// (MeasureColumnContentWidth iterates m_deque calling this to find
// the widest cell), and reused internally by BuildClipboardText.
void          AppendColumnText (
    std::wstring &                                   out,
    const Disk2EventDisplay &                        e,
    int                                              logicalId);

std::wstring  BuildClipboardText (
    const std::vector<const Disk2EventDisplay *> &  selected,
    const std::array<LogicalColumn, kColumnCount> &  columns);


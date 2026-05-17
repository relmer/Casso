#pragma once

#include "Pch.h"

#include "../CassoEmuCore/Devices/DiskIIEvent.h"
#include "DiskIIEventDisplay.h"
#include "TrackSectorPredicate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIDebugDialogState
//
//  Non-Win32 pieces of the spec-006 Disk II Debug dialog. Factored out
//  of DiskIIDebugDialog.cpp so the headless UnitTest project can link
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



// FR-004 column widths (sum ~= 790 px).
constexpr int  kColWallWidth   = 110;
constexpr int  kColUptimeWidth = 90;
constexpr int  kColCycleWidth  = 110;
constexpr int  kColEventWidth  = 110;
constexpr int  kColDetailWidth = 360;
constexpr int  kColumnCount    = 5;



void   SeedDefaultColumns (std::array<LogicalColumn, 5> & columns) noexcept;
bool   ComputeWasAtTail   (int topIndex, int countPerPage, int totalCount) noexcept;
bool   MatchesFilter      (const DiskIIEventDisplay & e, const FilterState & f) noexcept;

// Phase 9: format a clipboard payload from a vector of selected
// display rows. Tab-separated UTF-16 in visible-column order (hidden
// columns -- and their leading tabs -- are omitted per FR-026), CRLF
// row terminator. Pure helper; testable without a real clipboard or
// HWND.
std::wstring  BuildClipboardText (
    const std::vector<const DiskIIEventDisplay *> &  selected,
    const std::array<LogicalColumn, 5> &             columns);


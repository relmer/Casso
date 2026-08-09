#pragma once

#include "Pch.h"

#include "../CassoEmuCore/Devices/Disk2Event.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2EventDisplay
//
//  UI-thread projection of a Disk2Event: every column already formatted
//  to its final wide-character string so the virtual-mode ListView's
//  LVN_GETDISPINFO handler is an O(1) array lookup with no allocation
//  on the dispatch path.
//
//  Wall  : HH:MM:SS.mmm  (host-local wall clock at format time)        FR-004
//  Uptime: MM:SS.mmm     (since most recent //e reset / power cycle)   FR-004a
//  Cycle : decimal cumulative CPU cycle counter with thousands
//          separators ("1,234,567")                                    FR-005
//
//  Detail is a std::wstring (not a fixed buffer) because the formatter
//  produces variable-length text per event type (e.g., "T17 S5 V254"
//  vs "kind=MotorLoop drive=1 reason=ColdBootSuppression").
//
//  Drive / track / sector copies of the underlying integer fields are
//  kept so the FR-014 filter projection can evaluate matches without
//  re-parsing the detail string. INT_MIN sentinel for "not applicable
//  to this event type".
//
////////////////////////////////////////////////////////////////////////////////

struct Disk2EventDisplay
{
    static constexpr int   kFieldNotApplicable = INT_MIN;

    // Monotonic identity assigned when the event is projected into the
    // display deque (see DebugDialogProjection::DrainAndProject). Stable
    // across sort reorders and deque front-eviction, so a UI can track a
    // selected / focused event by seq rather than by a shifting row or
    // deque index. 0 means "unassigned".
    uint64_t                 seq         = 0;

    EventCategory            category    = EventCategory::Controller;
    Disk2EventType           type        = Disk2EventType::EventsLost;

    int                      drive       = kFieldNotApplicable;
    int                      track       = kFieldNotApplicable;
    int                      sector      = kFieldNotApplicable;

    std::array<wchar_t, 16>  wallStr     {};   // HH:MM:SS.mmm + null
    std::array<wchar_t, 12>  uptimeStr   {};   // MM:SS.mmm    + null
    std::array<wchar_t, 24>  cycleStr    {};   // decimal-grouped uint64

    std::wstring             detail;
};

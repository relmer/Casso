#pragma once

#include "Pch.h"

#include "Devices/InputEvent.h"
#include "Machines/Apple2/Common/Disk2Event.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLogKind
//
////////////////////////////////////////////////////////////////////////////////

enum class DebugLogKind
{
    Disk,
    Input,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLogRowKind
//
//  What one row of an event log is: an event; a gap where history
//  recording paused; the start of a stretch whose events cannot be rebuilt;
//  or a reset or power cycle, from the input journal.
//
////////////////////////////////////////////////////////////////////////////////

enum class DebugLogRowKind : uint8_t
{
    Event,
    Gap,
    Unavailable,
    Reset,
    PowerCycle,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLogRow
//
//  One row of an event log pane, stamped with the machine's position and
//  cycle, and the host's clock at that cycle (a UTC FILETIME, 0 when
//  unknown), with the event's own bytes, as a sector's, where it has them.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct DebugLogRow
{
    DebugLogRowKind                           kind     = DebugLogRowKind::Event;
    uint64_t                                  position = 0;
    uint64_t                                  cycle    = 0;
    uint64_t                                  wallTime = 0;
    T                                         event    = {};
    std::shared_ptr<const std::vector<Byte>>  data;
};

using DiskLogRow  = DebugLogRow<Disk2Event>;
using InputLogRow = DebugLogRow<InputEvent>;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLogDelta
//
//  What changed in an event log since the pane's last delta, applied in
//  this order: with isReset, every row the pane has is dropped; every row
//  at or after cutFrom is dropped; the older rows go before the pane's
//  oldest row and the newer rows after its newest. The pane's rows then
//  start at heldFrom, which is where history begins when isAtStart is set;
//  while isRebuilding is set, rows from before heldFrom are being rebuilt.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct DebugLogDelta
{
    static constexpr uint64_t  kNoCut = UINT64_MAX;

    bool                         isReset      = false;
    uint64_t                     cutFrom      = kNoCut;
    std::vector<DebugLogRow<T>>  older;
    std::vector<DebugLogRow<T>>  newer;
    uint64_t                     heldFrom     = 0;
    bool                         isAtStart    = false;
    bool                         isRebuilding = false;

    bool  IsEmpty() const
    {
        return !isReset && cutFrom == kNoCut && older.empty() && newer.empty();
    }
};

using DiskLogDelta  = DebugLogDelta<Disk2Event>;
using InputLogDelta = DebugLogDelta<InputEvent>;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLogDeltas
//
//  Every delta made for the event log panes since the window last took
//  them, oldest first, for it to apply in turn.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugLogDeltas
{
    std::vector<DiskLogDelta>   disk;
    std::vector<InputLogDelta>  input;
};

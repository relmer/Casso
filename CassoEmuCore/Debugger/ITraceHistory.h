#pragma once

#include "Pch.h"

#include "Cpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TraceSpan
//
//  Positions [from, to) history can rebuild the instructions of, with no
//  gap in recording between them.
//
////////////////////////////////////////////////////////////////////////////////

struct TraceSpan
{
    uint64_t  from = 0;
    uint64_t  to   = 0;

    bool  operator== (const TraceSpan &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHistoryEntry
//
//  An instruction rebuilt from history; isAvailable is false for one whose
//  stretch could not be replayed (its keyframe was taken with other disks).
//
////////////////////////////////////////////////////////////////////////////////

struct TraceHistoryEntry
{
    Cpu::TraceEntry  entry       = {};
    bool             isAvailable = true;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ITraceHistory
//
//  The instruction trace's depth (FR-198, FR-210 to FR-212): the
//  instructions the machine ran, rebuilt from history on demand.
//
////////////////////////////////////////////////////////////////////////////////

class ITraceHistory
{
public:
    virtual ~ITraceHistory() = default;

    //  The spans history covers, oldest first, ending at the machine's
    //  position, or while it runs live, at its newest keyframe.
    virtual void      GetSpans      (std::vector<TraceSpan> & outSpans) = 0;

    //  The instructions of [from, to), inside one span, oldest first. False,
    //  with nothing in outEntries, while a rebuild of their stretch is
    //  running; the next call after it finishes returns them.
    virtual bool      TryGetEntries (uint64_t from, uint64_t to, std::vector<TraceHistoryEntry> & outEntries) = 0;

    //  The position of the first instruction at or after cycle; UINT64_MAX
    //  when every one history covers is before it.
    virtual uint64_t  FindCycle     (uint64_t cycle) = 0;
};

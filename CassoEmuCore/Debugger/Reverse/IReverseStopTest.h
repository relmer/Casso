#pragma once

class IDisk2EventSink;
class IWatchSink;
class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  StretchSummary
//
//  What one stretch of history did, for IReverseStopTest::CanStopIn: a bit
//  for each 256-byte page an instruction started on, and whether the Disk II
//  controller went untouched with its motor stopped throughout.
//
////////////////////////////////////////////////////////////////////////////////

struct StretchSummary
{
    std::array<uint64_t, 4>  pagesRun    = {};
    bool                     isDiskQuiet = false;

    bool  IsAnyPageRun (Word first, Word last) const
    {
        for (uint32_t page = first >> 8; page <= (uint32_t) (last >> 8); page++)
        {
            if ((pagesRun[page >> 6] & (1ull << (page & 63))) != 0)
            {
                return true;
            }
        }

        return false;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  IReverseStopTest
//
//  The debugger's stop conditions, asked about every instruction a replay
//  runs, so running backward stops where running forward would have. The
//  machine is in the state that instruction begins with, so a breakpoint
//  condition or a memory comparison reads the restored machine.
//
//  ShouldStopBefore is a breakpoint: true stops before the instruction at
//  pc. TakePendingStop is a watchpoint: true if the instruction just run
//  raised a stop for the next boundary, and the pending stop is cleared.
//  GetWatchSink is where the bus reports watched accesses during the
//  replay, in place of the debugger's own sink, or null for none, and
//  GetDiskSink the Disk II controller's events, in place of the debugger's
//  disk breakpoint listener. None of them may log, count a hit or stop
//  anything itself; the reverse command selects the hit to land on.
//
//  CanStopIn is called before a stretch is replayed: false when nothing in
//  the stretch could stop, so the search goes past it without replaying it.
//  The default replays every stretch.
//
////////////////////////////////////////////////////////////////////////////////

class IReverseStopTest
{
public:
    virtual               ~IReverseStopTest() = default;

    virtual bool               ShouldStopBefore (MachineHost & machine, Word pc) = 0;
    virtual bool               TakePendingStop  ()                               = 0;
    virtual IWatchSink       * GetWatchSink     ()                               { return nullptr; }
    virtual IDisk2EventSink  * GetDiskSink      ()                               { return nullptr; }
    virtual bool               CanStopIn        (const StretchSummary & summary) { (void) summary; return true; }
};

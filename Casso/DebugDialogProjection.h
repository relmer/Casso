#pragma once

#include "Pch.h"

#include "../CassoEmuCore/Devices/DiskIIEvent.h"
#include "../CassoEmuCore/Devices/DiskIIEventRing.h"
#include "DiskIIEventDisplay.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugDialogProjection
//
//  UI-thread helper that drains the producer's SPSC ring of
//  DiskIIEvent records, formats each into a DiskIIEventDisplay
//  (Wall / Uptime / Cycle / Detail strings already laid out), and
//  appends them to the dialog's rolling deque. The dialog's
//  LVN_GETDISPINFO handler then reads from that deque without
//  allocating anything per repaint.
//
//  This class is stateless -- all entry points are static. The deque
//  and uptime anchor live on the dialog itself.
//
//  Per-event-type Detail strings (FR-005):
//      HeadStep       :  qt=<prev> -> <new>
//      HeadBump       :  qt=<atQt>
//      AddrMark       :  T<track> S<sector> V<volume>
//      DataRead       :  S<sector> (<n> bytes)
//      DriveSelect    :  drive=<n>
//      DiskInserted   :  drive=<n>
//      DiskEjected    :  drive=<n>
//      EventsLost     :  [<count> events lost]
//      MotorCommandOn / MotorCommandOff /
//      MotorEngaged   / MotorDisengaged   :  (empty)
//      AudioStarted   / AudioRestarted    /
//      AudioContinued / AudioLoopStarted  /
//      AudioLoopStopped                   :  kind=<SoundKind> drive=<n>
//      AudioSilent                        :  kind=<SoundKind> drive=<n> reason=<SilentReason>
//
////////////////////////////////////////////////////////////////////////////////

class DebugDialogProjection
{
public:
    // FR-007 "rolling cap". 100,000 entries * ~ a few hundred bytes
    // per DiskIIEventDisplay is well under the dialog's memory budget;
    // older entries are dropped from the front in FIFO order.
    static constexpr size_t   kDisplayDequeCap   = 100000;

    // Drain at most this many events per DrainAndProject call so the
    // intermediate buffer fits comfortably on the UI-thread stack.
    static constexpr uint32_t kDrainBatchSize    = 256;

    // Returns the fixed-width label for the Event column based on
    // (category, type). Stable wide string view; no allocation.
    static std::wstring_view  EventLabel (EventCategory cat, DiskIIEventType type);

    // Format one source event into a display record.
    static void               FormatEvent      (
        const DiskIIEvent &                          src,
        std::chrono::steady_clock::time_point        uptimeAnchor,
        DiskIIEventDisplay &                         out);

    // Drain the ring; if droppedCount > 0, push a synthetic
    // EventsLost record FIRST before the drained batch (FR-010).
    // After the batch, enforce the rolling cap by pop_front'ing
    // until size <= kDisplayDequeCap.
    static void               DrainAndProject  (
        DiskIIEventRing &                            ring,
        std::deque<DiskIIEventDisplay> &             deque,
        uint32_t                                     droppedCount,
        std::chrono::steady_clock::time_point        uptimeAnchor);
};

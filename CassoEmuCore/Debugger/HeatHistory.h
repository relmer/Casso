#pragma once

#include "Pch.h"

#include "Debugger/AccessHeatMap.h"
#include "Debugger/HeatRebuildJob.h"
#include "Debugger/Reverse/IHistoryObserver.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/SnapshotCompressor.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory
//
//  Keeps the heat map where the machine stands in reverse execution's
//  history, not where the host's clock has reached.
//
//  The totals. With every keyframe taken live, the counts made since the
//  keyframe before go beside it in the history, packed (HeatCountDelta), so
//  the totals at any keyframe follow from the totals at any other: add the
//  counts of the stretches between going forward, take them away going
//  back. The anchor is the last keyframe the machine stood at, taken or
//  loaded, with its totals. When a replay loads a keyframe, the totals are
//  set to that keyframe's, and the replay from it to where it stops counts
//  the rest, so after any seek, step or run the totals are those at the
//  position landed on, and stepping or running forward counts on from
//  there. A reset subtracts the totals at its position from the anchor's,
//  so the totals after it are counted from the reset; a position before it
//  shows none. When history drops a keyframe the anchor stands at, the
//  anchor moves to the neighbor kept, by the counts of the stretch between.
//
//  The map counts only from the position it was turned on at, and so does
//  a replay of the past; the keyframe at or before that position stands at
//  totals of zero, and one before it at or below zero, which reads as none.
//  Stretches recorded while the map was off have nothing kept and add
//  nothing. A run on from the past takes its totals from history at every
//  keyframe it reaches.
//
//  The last accesses. The map keeps, for every address, the latest access
//  up to the furthest the machine has run, which is the last access as of
//  any earlier position whenever it is from before that position. Where it
//  is not, the last access before the machine is found in history at the
//  moment it is asked for (LookUpLastAccess), and nothing more is stored for
//  it: the counts kept beside each keyframe say which stretch it was in, the
//  newest stretch whose counts include the address, and that one stretch is
//  replayed on a second machine (IHeatAccessFinder) to find the instruction.
//  The partial stretch the machine stands in is replayed first, up to the
//  machine, unless its counts show no access there. A record from a future
//  history drops is forgotten, and found the same way.
//
//  The heat. A move through history clears it, and the heat at the landing
//  is rebuilt by replaying three fade times before it, on a second machine
//  (IHeatRebuilder), so neither the machine nor the debugger waits; heat
//  gathered meanwhile is kept and the rebuilt heat merged into it. The
//  window is replayed in parts, the newest seconds first, each merged as it
//  comes in, so a long fade time shows the recent heat at once and the
//  older fills in after. During a drag of the timeline, the rebuild waits
//  for the drag to end; in cumulative mode, for fading mode to be chosen.
//
//  Everything here runs on the thread that runs the machine.
//
////////////////////////////////////////////////////////////////////////////////

class HeatHistory : public IHistoryObserver
{
public:
    HeatHistory (MachineHost & machine, AccessHeatMap & map);
    ~HeatHistory() override;

    HeatHistory             (const HeatHistory &) = delete;
    HeatHistory & operator= (const HeatHistory &) = delete;

    void      Attach            (KeyframeStore * keyframes);
    void      SetRebuilder      (IHeatRebuilder * rebuilder) { m_rebuilder = rebuilder; }
    void      SetAccessFinder   (IHeatAccessFinder * finder) { m_finder = finder; }
    void      SetCumulative     (bool isCumulative)          { m_isCumulative = isCumulative; }
    bool      IsAttached        () const                     { return m_keyframes != nullptr; }

    //  The map was turned on or off; the user reset its counts.
    void      OnMapStarted      ();
    void      OnMapStopped      ();
    void      Reset             ();

    //  The machine moved through history and stands where it landed;
    //  isInterim while a drag of the timeline goes on.
    void      OnMoved           (bool isInterim);

    //  Merges a rebuilt heat that has come in, and requests one now due;
    //  whether, fading, the heat still waits on one.
    void      Service           ();
    bool      IsRebuilding      () const { return !m_isCumulative && (m_isRebuildDue || m_awaited != 0); }

    //  The rebuild of the heat at where the machine stands; false when
    //  history holds nothing to replay up to it.
    HRESULT   MakeRebuildJob    (HeatRebuildJob & outJob, bool & outHasWindow);

    //  The totals at a keyframe, from the counts history keeps.
    bool      TryGetTotalsAt    (uint64_t keyframePosition, std::vector<int64_t> & outTotals);

    //  The instruction that last wrote or read an address in a space as of
    //  where the machine stands, looked up in history where the map does not
    //  hold it; unknown when history does not reach back to it either.
    HeatAccessState  LookUpLastAccess (HeatSpace space, bool isWrite, Word address, HeatLastAccess & outAccess);

    void      OnKeyframeAdding  (uint64_t position, std::vector<Byte> & outSide) override;
    void      OnKeyframeLoaded  (uint64_t position) override;

private:
    //  Where, in seconds of machine time before the machine, a rebuild's
    //  window is cut into parts, so its newest part is short and comes in
    //  at once.
    static constexpr std::array<int, 4>  kPartSeconds = { 2, 10, 30, 90 };

    //  How many fade times a rebuild replays. A single access is cold after
    //  one, but an address touched a thousand times stays warm for about
    //  two and a half, so one fade time would lose what a busy stretch just
    //  before it left on the map.
    static constexpr double  kWindowFades = 3.0;

    double    GetWindowSeconds  () const { return m_map.GetFadeSeconds() * kWindowFades; }

    void      OnKeyframeDrop    (KeyframeDrop drop);
    bool      TryFindKeyframe   (uint64_t position, size_t & outIndex) const;
    bool      TryWalk           (size_t from, size_t to, std::vector<int64_t> & ioTotals);
    bool      TryApplyStretch   (size_t index, bool isSubtracting, std::vector<int64_t> & ioTotals);
    void      SetAnchor         (uint64_t position, const std::vector<int64_t> & totals);
    void      DropAnchor        ();
    void      RequestRebuild    ();
    size_t    FindWindowStart   (size_t target, uint64_t cycle) const;
    void      FindPartStarts    (size_t start, size_t target, uint64_t position, uint64_t cycle, std::vector<size_t> & outStarts) const;
    HRESULT   AddPart           (size_t index, uint64_t endPosition, uint64_t endCycle, HeatRebuildJob & ioJob);
    HRESULT   CopyInputs        (size_t journalIndex, uint64_t position, std::vector<InputRecord> & outInputs) const;
    HRESULT   CopyDisks         (std::vector<HeatRebuildDisk> & outDisks);
    HRESULT   FindInStretch     (size_t index, uint64_t endPosition, uint64_t endCycle, const HeatRebuildJob::Query & query, bool & outIsFound, HeatLastAccess & outAccess);
    bool      HasCountIn        (size_t index, size_t entry, bool & outHasSide);

    //  The last access a query asks for, in the stretches before the machine.
    HeatAccessState  SearchHistory (const HeatRebuildJob::Query & query, HeatLastAccess & outAccess);

    MachineHost                    & m_machine;
    AccessHeatMap                  & m_map;
    KeyframeStore                  * m_keyframes      = nullptr;
    IHeatRebuilder                 * m_rebuilder      = nullptr;
    IHeatAccessFinder              * m_finder         = nullptr;
    KeyframeUnpacker                 m_noUnpacker;                   // holds nothing, so every copy includes its whole snapshot

    bool                             m_hasAnchor      = false;
    uint64_t                         m_anchorPosition = 0;
    std::vector<int64_t>             m_anchorTotals;
    std::vector<int64_t>             m_now;
    std::vector<int64_t>             m_walk;
    std::vector<Byte>                m_delta;
    SnapshotCompressor               m_compressor;

    uint64_t                         m_heatFrom       = 0;           // where the map was turned on or last reset
    uint64_t                         m_generation     = 0;
    uint64_t                         m_awaited        = 0;           // the rebuild in flight, 0 for none
    bool                             m_isRebuildDue   = false;
    bool                             m_isDragging     = false;
    bool                             m_isCumulative   = false;

    //  Last accesses looked up in history, by entry, for the position they
    //  were looked up at.
    std::unordered_map<size_t, std::pair<HeatAccessState, HeatLastAccess>>  m_lookups;
    uint64_t                                                                m_lookupsAt = UINT64_MAX;

    std::unordered_map<uint64_t, std::shared_ptr<const std::vector<Byte>>>  m_diskImages;   // by medium
    std::shared_ptr<const std::vector<Byte>>                                m_blankSectorImage;
};

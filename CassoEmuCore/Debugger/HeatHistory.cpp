#include "Pch.h"

#include "Debugger/HeatHistory.h"

#include "Debugger/HeatCountDelta.h"
#include "Debugger/HeatKeyframeSide.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::HeatHistory
//
////////////////////////////////////////////////////////////////////////////////

HeatHistory::HeatHistory (
    MachineHost    & machine,
    AccessHeatMap  & map) :
    m_machine (machine),
    m_map     (map)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::~HeatHistory
//
////////////////////////////////////////////////////////////////////////////////

HeatHistory::~HeatHistory()
{
    Attach (nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::Attach
//
//  The history to keep counts in, or none. A store attached afresh holds no
//  counts whose totals are known here, so the anchor waits for the next
//  keyframe the machine stands at.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::Attach (KeyframeStore * keyframes)
{
    if (keyframes == m_keyframes)
    {
        return;
    }

    if (m_keyframes != nullptr)
    {
        m_keyframes->SetDropListener (nullptr);
    }

    m_keyframes = keyframes;

    DropAnchor();

    if (m_keyframes != nullptr)
    {
        m_keyframes->SetDropListener ([this] (KeyframeDrop drop) { OnKeyframeDrop (drop); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::OnMapStarted
//
//  The map counts from here on, so every keyframe at or before here stands
//  at no count at all: the newest of them is the anchor, with totals of
//  zero. The heat starts from here too.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::OnMapStarted()
{
    uint64_t  position = m_machine.GetPosition();
    size_t    index    = 0;
    bool      isFound  = false;



    m_heatFrom     = position;
    m_isRebuildDue = false;
    m_isDragging   = false;
    m_awaited      = 0;

    m_lookups.clear();
    DropAnchor();

    isFound = m_keyframes != nullptr && m_keyframes->TryFindByPosition (position, index);

    if (isFound)
    {
        m_now.assign (AccessHeatMap::kEntryCount, 0);
        SetAnchor (m_keyframes->GetInfo (index).position, m_now);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::OnMapStopped
//
//  The totals and heat are gone with the map, and so is the memory held
//  for them here.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::OnMapStopped()
{
    if (m_rebuilder != nullptr && m_awaited != 0)
    {
        m_rebuilder->Cancel();
    }

    m_awaited      = 0;
    m_isRebuildDue = false;
    m_isDragging   = false;

    DropAnchor();

    m_anchorTotals = std::vector<int64_t>();
    m_now          = std::vector<int64_t>();
    m_walk         = std::vector<int64_t>();
    m_bits         = std::vector<uint64_t>();
    m_oldestBits   = std::vector<uint64_t>();

    m_diskImages.clear();
    m_lookups.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::Reset
//
//  The totals start over where the machine stands: every keyframe's totals,
//  the anchor's included, lose the totals here, so a later position counts
//  from here and an earlier one falls below zero, which reads as none. The
//  heat starts over from here as well, and a rebuild in flight is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::Reset()
{
    if (m_map.IsOn() && m_hasAnchor)
    {
        m_map.GetTotalsNow (m_now);

        for (size_t i = 0; i < m_anchorTotals.size(); i++)
        {
            m_anchorTotals[i] -= m_now[i];
        }
    }

    m_map.Reset();

    if (m_rebuilder != nullptr && m_awaited != 0)
    {
        m_rebuilder->Cancel();
    }

    m_heatFrom     = m_machine.GetPosition();
    m_awaited      = 0;
    m_isRebuildDue = false;

    m_lookups.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::OnKeyframeAdding
//
//  The counts made since the keyframe before, which is the newest held:
//  the totals now less the totals there, followed from the anchor. The new
//  keyframe becomes the anchor. The written bits go beside them, or a note
//  that they have not changed since the keyframe before.
//
//  This runs on the thread that runs the machine, at every keyframe, over
//  all 1.3 million totals, so nothing is copied that need not be: an anchor
//  that is already the newest keyframe needs no walk, and the totals now
//  become the anchor's by a swap.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::OnKeyframeAdding (
    uint64_t             position,
    std::vector<Byte>  & outSide)
{
    HRESULT                         hr        = S_OK;
    size_t                          anchor    = 0;
    size_t                          newest    = 0;
    bool                            hasWalk   = false;
    bool                            isChanged = false;
    const std::vector<int64_t>    * earlier   = nullptr;



    outSide.clear();
    m_counts.clear();

    if (!m_map.IsOn() || m_keyframes == nullptr)
    {
        DropAnchor();
        return;
    }

    m_map.GetTotalsNow (m_now);

    if (m_hasAnchor && m_keyframes->GetCount() > 0 && TryFindKeyframe (m_anchorPosition, anchor))
    {
        newest  = m_keyframes->GetCount() - 1;
        earlier = &m_anchorTotals;
        hasWalk = true;

        if (anchor != newest)
        {
            m_walk  = m_anchorTotals;
            earlier = &m_walk;
            hasWalk = TryWalk (anchor, newest, m_walk);
        }
    }

    if (hasWalk && earlier->size() == m_now.size())
    {
        HeatCountDelta::Encode (m_now.data(), earlier->data(), m_now.size(), m_delta);

        hr = HeatCountDelta::Pack (m_delta, m_compressor, m_counts);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    isChanged = m_map.TakeKeptChange() || m_keyframes->GetCount() == 0;

    if (isChanged)
    {
        m_map.GetKeptBits (m_bits);
    }

    hr = HeatKeyframeSide::Make (m_counts, isChanged ? &m_bits : nullptr, m_compressor, outSide);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_anchorTotals.swap (m_now);
    m_anchorPosition = position;
    m_hasAnchor      = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::OnKeyframeLoaded
//
//  A replay put the machine back at a keyframe: the totals become that
//  keyframe's, followed from the anchor, and the keyframe the anchor.
//  Without an anchor to follow from, the totals stand as they are. The
//  written bits become the keyframe's.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::OnKeyframeLoaded (uint64_t position)
{
    size_t  index   = 0;
    size_t  anchor  = 0;
    bool    hasWalk = false;



    if (!m_map.IsOn() || m_keyframes == nullptr || !TryFindKeyframe (position, index))
    {
        return;
    }

    GetKeptAt (index, m_bits);
    m_map.SetKeptBits (m_bits);

    if (m_hasAnchor && TryFindKeyframe (m_anchorPosition, anchor))
    {
        m_walk  = m_anchorTotals;
        hasWalk = TryWalk (anchor, index, m_walk);
    }

    if (!hasWalk)
    {
        m_map.GetTotalsNow (m_walk);
    }

    m_map.SetTotals (m_walk);

    SetAnchor (position, m_walk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::TryGetTotalsAt
//
////////////////////////////////////////////////////////////////////////////////

bool HeatHistory::TryGetTotalsAt (
    uint64_t                keyframePosition,
    std::vector<int64_t>  & outTotals)
{
    size_t  index  = 0;
    size_t  anchor = 0;



    if (!m_hasAnchor || m_keyframes == nullptr || !TryFindKeyframe (keyframePosition, index) || !TryFindKeyframe (m_anchorPosition, anchor))
    {
        return false;
    }

    outTotals = m_anchorTotals;

    return TryWalk (anchor, index, outTotals);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::LookUpLastAccess
//
//  The map's record when it is one as of here; otherwise what history holds,
//  looked up once for each position the machine stands at.
//
////////////////////////////////////////////////////////////////////////////////

HeatAccessState HeatHistory::LookUpLastAccess (
    HeatSpace          space,
    bool               isWrite,
    Word               address,
    HeatLastAccess   & outAccess)
{
    HeatAccessState        state    = m_map.GetLastAccess (space, isWrite, address, outAccess);
    uint64_t               position = m_machine.GetPosition();
    size_t                 entry    = AccessHeatMap::GetLastIndex (space, isWrite, address);
    HeatRebuildJob::Query  query;



    if (state != HeatAccessState::Unknown || m_finder == nullptr || m_keyframes == nullptr)
    {
        return state;
    }

    if (m_lookupsAt != position)
    {
        m_lookups.clear();
        m_lookupsAt = position;
    }

    if (m_lookups.contains (entry))
    {
        outAccess = m_lookups[entry].second;
        return m_lookups[entry].first;
    }

    query.isSet   = true;
    query.space   = space;
    query.isWrite = isWrite;
    query.address = address;

    state = SearchHistory (query, outAccess);

    m_lookups[entry] = { state, outAccess };

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::SearchHistory
//
//  First the stretch the machine stands in, from its keyframe up to the
//  machine, unless the counts kept for it show no access there; then each
//  stretch before it, newest first, by its counts, down to where the map
//  started counting, and the first with an access is replayed. A stretch
//  with no counts kept, or a replay that cannot reach its end, leaves the
//  access unknown, as does history running out before the map started.
//
////////////////////////////////////////////////////////////////////////////////

HeatAccessState HeatHistory::SearchHistory (
    const HeatRebuildJob::Query  & query,
    HeatLastAccess               & outAccess)
{
    HRESULT          hr       = S_OK;
    const EmuCpu   * cpu      = m_machine.GetCpu();
    uint64_t         position = m_machine.GetPosition();
    size_t           entry    = AccessHeatMap::GetIndex (query.space, query.isWrite ? HeatKind::Write : HeatKind::Read, query.address);
    size_t           here     = 0;
    bool             isFound  = false;
    bool             hasCount = false;
    bool             hasSide  = false;
    HeatAccessState  state    = HeatAccessState::Unknown;



    outAccess = HeatLastAccess();

    CBRA (cpu);

    hr = m_keyframes->WaitForPending();
    CHR (hr);

    isFound = m_keyframes->TryFindByPosition (position, here);
    BAIL_OUT_IF (!isFound, S_OK);

    hasCount = here + 1 >= m_keyframes->GetCount() || HasCountIn (here + 1, entry, hasSide) || !hasSide;

    if (hasCount && m_keyframes->GetInfo (here).position < position)
    {
        hr = FindInStretch (here, position, cpu->GetTotalCycles(), query, isFound, outAccess);
        CHR (hr);

        BAIL_OUT_IF (isFound, S_OK);
    }

    for (size_t index = here; index > 0; index--)
    {
        if (m_keyframes->GetInfo (index).position <= m_heatFrom)
        {
            state = HeatAccessState::None;
            break;
        }

        hasCount = HasCountIn (index, entry, hasSide);

        BAIL_OUT_IF (!hasSide, S_OK);

        if (!hasCount)
        {
            continue;
        }

        hr = FindInStretch (index - 1, m_keyframes->GetInfo (index).position, m_keyframes->GetInfo (index).cycle, query, isFound, outAccess);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    if (m_keyframes->GetInfo (0).position <= m_heatFrom)
    {
        state = HeatAccessState::None;
    }

Error:
    if (FAILED (hr))
    {
        state = HeatAccessState::Unknown;
    }

    return isFound ? HeatAccessState::Found : state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::HasCountIn
//
//  Whether the counts kept with keyframe index, those of the stretch from
//  the keyframe before it, include the entry; and whether it kept counts.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatHistory::HasCountIn (
    size_t    index,
    size_t    entry,
    bool    & outHasSide)
{
    HRESULT       hr      = S_OK;
    const Byte  * side    = nullptr;
    size_t        size    = 0;
    uint64_t      count   = 0;
    bool          isRead  = false;



    outHasSide = TryGetCounts (index, side, size);

    BAIL_OUT_IF (!outHasSide, S_OK);

    hr = HeatCountDelta::Unpack (side, size, m_compressor, m_delta);
    CHR (hr);

    isRead = HeatCountDelta::TryGetCount (m_delta.data(), m_delta.size(), entry, count);
    CBREx (isRead, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    if (FAILED (hr))
    {
        outHasSide = false;
    }

    return SUCCEEDED (hr) && count > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::FindInStretch
//
//  Replays from keyframe index to the end given on the finder's machine,
//  with the map there counting from where this one started, and takes the
//  last access the query asks for that the replay made.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeatHistory::FindInStretch (
    size_t                          index,
    uint64_t                        endPosition,
    uint64_t                        endCycle,
    const HeatRebuildJob::Query   & query,
    bool                          & outIsFound,
    HeatLastAccess                & outAccess)
{
    HRESULT            hr = S_OK;
    HeatRebuildJob     job;
    HeatRebuildResult  result;



    outIsFound = false;

    hr = AddPart (index, endPosition, endCycle, job);
    CHR (hr);

    job.inputsFrom  = m_keyframes->GetInfo (index).journalIndex;
    job.countFrom   = m_heatFrom;
    job.fadeSeconds = m_map.GetFadeSeconds();
    job.generation  = ++m_generation;
    job.query       = query;

    hr = CopyInputs (job.inputsFrom, endPosition, job.inputs);
    CHR (hr);

    hr = CopyDisks (job.disks);
    CHR (hr);

    hr = m_finder->FindAccess (job, result);
    CHR (hr);

    outIsFound = result.hasAccess;
    outAccess  = result.access;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::OnKeyframeDrop
//
//  History is about to drop a keyframe: when it is the anchor, the anchor
//  moves to the neighbor that stays, by the counts of the stretch between,
//  which are still there to read. Dropping everything drops the anchor.
//  Dropping the newest drops a future, and the map's records from it, from
//  the keyframe before it or the machine, whichever is earlier, are
//  forgotten. Dropping the oldest keeps the written bits at the keyframe
//  that becomes the oldest, which may note them only as unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::OnKeyframeDrop (KeyframeDrop drop)
{
    size_t    count     = m_keyframes->GetCount();
    size_t    index     = (drop == KeyframeDrop::Oldest) ? 0 : count - 1;
    bool      isApplied = false;
    uint64_t  cut       = m_machine.GetPosition();



    if (drop == KeyframeDrop::Oldest && count >= 2 && m_map.IsOn())
    {
        GetKeptAt (1, m_bits);

        m_oldestBits.swap (m_bits);
        m_oldestBitsAt = m_keyframes->GetInfo (1).position;
    }
    else if (drop == KeyframeDrop::All)
    {
        m_oldestBits.clear();
    }

    if (drop != KeyframeDrop::Oldest)
    {
        if (drop == KeyframeDrop::Newest && count >= 2)
        {
            cut = std::min (cut, m_keyframes->GetInfo (count - 2).position);
        }

        m_map.ForgetAccessesFrom (cut);
        m_lookups.clear();
    }

    if (!m_hasAnchor || drop == KeyframeDrop::All)
    {
        DropAnchor();
        return;
    }

    if (count == 0 || m_keyframes->GetInfo (index).position != m_anchorPosition)
    {
        return;
    }

    if (count < 2)
    {
        DropAnchor();
        return;
    }

    if (drop == KeyframeDrop::Oldest)
    {
        isApplied        = TryApplyStretch (1, false, m_anchorTotals);
        m_anchorPosition = m_keyframes->GetInfo (1).position;
    }
    else
    {
        isApplied        = TryApplyStretch (index, true, m_anchorTotals);
        m_anchorPosition = m_keyframes->GetInfo (index - 1).position;
    }

    if (!isApplied)
    {
        DropAnchor();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::TryFindKeyframe
//
////////////////////////////////////////////////////////////////////////////////

bool HeatHistory::TryFindKeyframe (
    uint64_t   position,
    size_t   & outIndex) const
{
    return m_keyframes->TryFindByPosition (position, outIndex) && m_keyframes->GetInfo (outIndex).position == position;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::TryWalk
//
//  The totals at keyframe from become those at keyframe to.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatHistory::TryWalk (
    size_t                  from,
    size_t                  to,
    std::vector<int64_t>  & ioTotals)
{
    bool    isApplied = true;
    size_t  k         = 0;



    for (k = from + 1; isApplied && k <= to; k++)
    {
        isApplied = TryApplyStretch (k, false, ioTotals);
    }

    for (k = from; isApplied && k > to; k--)
    {
        isApplied = TryApplyStretch (k, true, ioTotals);
    }

    return isApplied;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::TryApplyStretch
//
//  Adds, or takes away, the counts kept with keyframe index: those made
//  from the keyframe before it to it. A stretch recorded while the map was
//  off has none and adds nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatHistory::TryApplyStretch (
    size_t                  index,
    bool                    isSubtracting,
    std::vector<int64_t>  & ioTotals)
{
    HRESULT       hr        = S_OK;
    const Byte  * side      = nullptr;
    size_t        size      = 0;
    bool          isApplied = true;
    bool          hasCounts = false;



    hasCounts = TryGetCounts (index, side, size);

    BAIL_OUT_IF (!hasCounts, S_OK);

    hr = HeatCountDelta::Unpack (side, size, m_compressor, m_delta);
    CHR (hr);

    isApplied = HeatCountDelta::TryApply (m_delta.data(), m_delta.size(), isSubtracting, ioTotals.data(), ioTotals.size());

Error:
    return SUCCEEDED (hr) && isApplied;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::TryGetCounts
//
//  The packed counts kept with keyframe index; false when it kept none.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatHistory::TryGetCounts (
    size_t          index,
    const Byte   *& outBytes,
    size_t        & outSize) const
{
    const Byte              * side    = nullptr;
    size_t                    size    = 0;
    HeatKeyframeSide::Part    counts;
    HeatKeyframeSide::Part    written;



    outBytes = nullptr;
    outSize  = 0;

    m_keyframes->GetSide (index, side, size);

    if (!HeatKeyframeSide::TrySplit (side, size, counts, written) || counts.size == 0)
    {
        return false;
    }

    outBytes = counts.bytes;
    outSize  = counts.size;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::GetKeptAt
//
//  From the keyframe back to the newest that kept the bits themselves; one
//  at or before where the map started counting, or with nothing kept, stands
//  at the bits the map started with, as does history that does not reach
//  back to bits at all. The oldest keyframe's are the ones kept as it
//  became the oldest when it notes them unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::GetKeptAt (
    size_t                   index,
    std::vector<uint64_t>  & outBits)
{
    HRESULT                   hr      = S_OK;
    const Byte              * side    = nullptr;
    size_t                    size    = 0;
    size_t                    k       = index + 1;
    bool                      isFound = false;
    HeatKeyframeSide::Part    counts;
    HeatKeyframeSide::Part    written;



    outBits.clear();

    while (!isFound && k > 0 && m_keyframes != nullptr && k <= m_keyframes->GetCount())
    {
        k--;

        if (m_keyframes->GetInfo (k).position <= m_map.GetCountFrom())
        {
            break;
        }

        m_keyframes->GetSide (k, side, size);

        if (size == 0 || !HeatKeyframeSide::TrySplit (side, size, counts, written))
        {
            break;
        }

        if (written.size != 0)
        {
            hr      = HeatKeyframeSide::UnpackWritten (written, m_compressor, outBits);
            isFound = SUCCEEDED (hr);
            break;
        }

        if (k == 0 && !m_oldestBits.empty() && m_oldestBitsAt == m_keyframes->GetInfo (0).position)
        {
            outBits = m_oldestBits;
            isFound = true;
        }
    }

    if (!isFound)
    {
        AccessHeatMap::MakeKeptBits (!m_map.IsStartedClear(), outBits);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::SetAnchor
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::SetAnchor (
    uint64_t                      position,
    const std::vector<int64_t>  & totals)
{
    m_anchorPosition = position;
    m_anchorTotals   = totals;
    m_hasAnchor      = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::DropAnchor
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::DropAnchor()
{
    m_hasAnchor      = false;
    m_anchorPosition = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::OnMoved
//
//  The heat counted on the way here is the totals' alone, and the heat is
//  cleared, to be rebuilt for here; a rebuild still running is for where
//  the machine was, and is dropped. During a drag the rebuild waits for the
//  drag to end.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::OnMoved (bool isInterim)
{
    const EmuCpu  * cpu = m_machine.GetCpu();



    if (!m_map.IsOn() || cpu == nullptr)
    {
        return;
    }

    m_map.ClearHeat (cpu->GetTotalCycles());

    if (m_rebuilder != nullptr && m_awaited != 0)
    {
        m_rebuilder->Cancel();
    }

    m_awaited      = 0;
    m_isRebuildDue = true;
    m_isDragging   = isInterim;

    Service();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::Service
//
//  Each part of the rebuild for the move last asked about is merged into
//  the heat gathered since as it comes in, the newest first; a part of any
//  other is stale. A rebuild that is due goes out once no drag holds it and
//  the map fades.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::Service()
{
    HeatRebuildResult  result;
    bool               isTaken = false;
    bool               isOurs  = false;



    if (m_rebuilder == nullptr)
    {
        m_isRebuildDue = false;
        return;
    }

    isTaken = m_rebuilder->TryTakeResult (result);

    while (isTaken)
    {
        isOurs = m_awaited != 0 && result.generation == m_awaited;

        if (isOurs && SUCCEEDED (result.hr))
        {
            m_map.MergeHeat (result.heat, result.cycle);
        }

        if (isOurs && result.isLast)
        {
            m_awaited = 0;
        }

        isTaken = m_rebuilder->TryTakeResult (result);
    }

    if (m_isRebuildDue && !m_isDragging && !m_isCumulative && m_awaited == 0)
    {
        RequestRebuild();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::RequestRebuild
//
//  The heat is cleared as of now, since the rebuild covers everything up to
//  here, and the rebuild goes out; with nothing in history to replay up to
//  here, the heat simply starts from here.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::RequestRebuild()
{
    HRESULT                          hr        = S_OK;
    const EmuCpu                   * cpu       = m_machine.GetCpu();
    std::shared_ptr<HeatRebuildJob>  job;
    bool                             hasWindow = false;



    m_isRebuildDue = false;

    BAIL_OUT_IF (!m_map.IsOn() || cpu == nullptr || m_keyframes == nullptr, S_OK);

    m_map.ClearHeat (cpu->GetTotalCycles());

    job = std::make_shared<HeatRebuildJob>();
    CPRA (job);

    hr = MakeRebuildJob (*job, hasWindow);
    CHR (hr);

    BAIL_OUT_IF (!hasWindow, S_OK);

    hr = m_rebuilder->Submit (job);
    CHR (hr);

    m_awaited = job->generation;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::MakeRebuildJob
//
//  The window ends where the machine stands and reaches back kWindowFades
//  fade times of machine time, to the keyframe at or before its start; it
//  reaches no further back than history, than a keyframe the replay loads
//  rather than reaches, or than where the heat began. It is cut into parts
//  at keyframes, newest first, so the heat nearest the machine comes in
//  first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeatHistory::MakeRebuildJob (
    HeatRebuildJob  & outJob,
    bool            & outHasWindow)
{
    HRESULT              hr       = S_OK;
    const EmuCpu       * cpu      = m_machine.GetCpu();
    uint64_t             position = m_machine.GetPosition();
    uint64_t             cycle    = 0;
    size_t               target   = 0;
    size_t               start    = 0;
    bool                 isFound  = false;
    std::vector<size_t>  starts;



    outHasWindow = false;
    outJob       = HeatRebuildJob();

    CBRA (cpu);
    CBRA (m_keyframes);

    cycle = cpu->GetTotalCycles();

    hr = m_keyframes->WaitForPending();
    CHR (hr);

    isFound = m_keyframes->TryFindByPosition (position, target);
    BAIL_OUT_IF (!isFound, S_OK);

    start = FindWindowStart (target, cycle);

    BAIL_OUT_IF (m_keyframes->GetInfo (start).position >= position, S_OK);

    FindPartStarts (start, target, position, cycle, starts);

    for (size_t part = 0; part < starts.size(); part++)
    {
        hr = AddPart (starts[part], (part == 0) ? position : outJob.parts.back().startPosition, (part == 0) ? cycle : outJob.parts.back().startCycle, outJob);
        CHR (hr);
    }

    outJob.inputsFrom  = m_keyframes->GetInfo (start).journalIndex;
    outJob.countFrom   = m_heatFrom;
    outJob.fadeSeconds = m_map.GetFadeSeconds();
    outJob.generation  = ++m_generation;

    hr = CopyInputs (outJob.inputsFrom, position, outJob.inputs);
    CHR (hr);

    hr = CopyDisks (outJob.disks);
    CHR (hr);

    outHasWindow = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::FindPartStarts
//
//  The keyframes the parts begin at, newest first: the newest at or before
//  each of kPartSeconds before the machine's cycle that lies after the
//  window's start and before the machine, then the window's start.
//
////////////////////////////////////////////////////////////////////////////////

void HeatHistory::FindPartStarts (
    size_t                 start,
    size_t                 target,
    uint64_t               position,
    uint64_t               cycle,
    std::vector<size_t>  & outStarts) const
{
    size_t    cut      = 0;
    uint64_t  cutCycle = 0;



    outStarts.clear();

    for (int age : kPartSeconds)
    {
        cutCycle = (uint64_t) (age * AccessHeatMap::kCyclesPerSecond);

        if ((double) age >= GetWindowSeconds() || cutCycle >= cycle)
        {
            break;
        }

        cutCycle = cycle - cutCycle;
        cut      = target;

        while (cut > start && m_keyframes->GetInfo (cut).cycle > cutCycle)
        {
            cut--;
        }

        if (cut <= start)
        {
            break;
        }

        if (m_keyframes->GetInfo (cut).position >= position || (!outStarts.empty() && cut >= outStarts.back()))
        {
            continue;
        }

        outStarts.push_back (cut);
    }

    outStarts.push_back (start);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::AddPart
//
//  The part from keyframe index to the end given, its keyframe copied out
//  still packed, with the written bits as of it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeatHistory::AddPart (
    size_t             index,
    uint64_t           endPosition,
    uint64_t           endCycle,
    HeatRebuildJob   & ioJob)
{
    HRESULT               hr   = S_OK;
    const KeyframeInfo  & info = m_keyframes->GetInfo (index);
    KeyframeCopy          copy = KeyframeCopy::Gone;
    HeatRebuildPart       part;



    hr = m_keyframes->CopyPacked (info.position, m_noUnpacker, part.start, copy);
    CHR (hr);

    CBRA (copy == KeyframeCopy::Copied);

    part.startPosition = info.position;
    part.startCycle    = info.cycle;
    part.journalIndex  = info.journalIndex;
    part.endPosition   = endPosition;
    part.endCycle      = endCycle;

    GetKeptAt (index, part.kept);

    ioJob.parts.push_back (std::move (part));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::FindWindowStart
//
//  From the keyframe at or before the target back: the first keyframe at or
//  before the window's first cycle, or the oldest, a keyframe a replay loads,
//  or one at or before where the heat began, whichever comes first.
//
////////////////////////////////////////////////////////////////////////////////

size_t HeatHistory::FindWindowStart (
    size_t    target,
    uint64_t  cycle) const
{
    uint64_t              window = (uint64_t) (GetWindowSeconds() * AccessHeatMap::kCyclesPerSecond);
    size_t                start  = target;
    const KeyframeInfo  * info   = nullptr;



    while (start > 0)
    {
        info = &m_keyframes->GetInfo (start);

        if (info->isBoundary || info->cycle > cycle || cycle - info->cycle >= window || info->position <= m_heatFrom)
        {
            break;
        }

        start--;
    }

    return start;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::CopyInputs
//
//  The recorded inputs from the window's keyframe on, through the target's
//  position.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeatHistory::CopyInputs (
    size_t                      journalIndex,
    uint64_t                    position,
    std::vector<InputRecord>  & outInputs) const
{
    HRESULT               hr      = S_OK;
    const InputJournal  & journal = m_machine.GetInputJournal();
    size_t                index   = journalIndex;
    bool                  isHeld  = journalIndex >= journal.GetBeginIndex() && journalIndex <= journal.GetEndIndex();



    outInputs.clear();

    CBRA (isHeld);

    for (index = journalIndex; index < journal.GetEndIndex() && journal.GetRecord (index).position <= position; index++)
    {
        outInputs.push_back (journal.GetRecord (index));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistory::CopyDisks
//
//  Each disk in a bay as a rebuild mounts it: a sector image as a blank one
//  of its format, shared by all, and any other as its format's file would
//  hold it, made once per medium and shared with every rebuild after; the
//  images of media no longer in a bay are let go. A disk its format cannot
//  hold fails the rebuild.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeatHistory::CopyDisks (std::vector<HeatRebuildDisk> & outDisks)
{
    HRESULT                                                                  hr       = S_OK;
    DiskImageStore                                                         & store    = m_machine.GetDiskStore();
    DiskImage                                                              * image    = nullptr;
    std::unordered_map<uint64_t, std::shared_ptr<const std::vector<Byte>>>   kept;
    std::shared_ptr<std::vector<Byte>>                                       bytes;
    HeatRebuildDisk                                                          disk;
    int                                                                      slot     = 0;
    int                                                                      drive    = 0;
    bool                                                                     isSector = false;



    outDisks.clear();

    if (m_blankSectorImage == nullptr)
    {
        m_blankSectorImage = std::make_shared<const std::vector<Byte>> (DiskImage::kDos33ImageSize, (Byte) 0);
    }

    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            image = store.IsMounted (slot, drive) ? store.GetImage (slot, drive) : nullptr;

            if (image == nullptr)
            {
                continue;
            }

            disk            = HeatRebuildDisk();
            disk.slot       = slot;
            disk.drive      = drive;
            disk.mediaId    = store.GetMediaId (slot, drive);
            disk.trackCount = image->GetTrackCount();
            disk.format     = image->GetSourceFormat();
            isSector        = disk.format == DiskFormat::Dsk || disk.format == DiskFormat::Do || disk.format == DiskFormat::Po;

            if (isSector)
            {
                disk.image = m_blankSectorImage;
            }
            else if (m_diskImages.contains (disk.mediaId))
            {
                disk.image = m_diskImages[disk.mediaId];
            }
            else
            {
                bytes = std::make_shared<std::vector<Byte>>();
                CPRA (bytes);

                hr = image->Serialize (*bytes);
                CHR (hr);

                disk.image = bytes;
            }

            if (!isSector)
            {
                kept[disk.mediaId] = disk.image;
            }

            outDisks.push_back (disk);
        }
    }

    m_diskImages.swap (kept);

Error:
    return hr;
}






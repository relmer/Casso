#include "Pch.h"

#include "Debugger/Reverse/HistoryMarks.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/KeyframeStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetKind
//
//  The mark a journal record makes, if any: a reset, a power cycle, a disk
//  mounted or ejected (a swap is the two), a write-protect change in the
//  drive or the image, and a state file loaded.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryMarks::TryGetKind (
    InputKind          kind,
    HistoryMarkKind  & outKind)
{
    bool  isMark = true;



    switch (kind)
    {
        case InputKind::Reset:              outKind = HistoryMarkKind::Reset;        break;
        case InputKind::PowerCycle:         outKind = HistoryMarkKind::PowerCycle;   break;
        case InputKind::DiskMount:          outKind = HistoryMarkKind::DiskChange;   break;
        case InputKind::DiskEject:          outKind = HistoryMarkKind::DiskChange;   break;
        case InputKind::DriveWriteProtect:  outKind = HistoryMarkKind::WriteProtect; break;
        case InputKind::ImageWriteProtect:  outKind = HistoryMarkKind::WriteProtect; break;
        case InputKind::StateLoad:          outKind = HistoryMarkKind::StateLoad;    break;

        default:
            isMark = false;
            break;
    }

    return isMark;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EstimateCycle
//
//  The history cycle at a position: from the keyframe at or before it, on
//  at the rate of cycles to positions between that keyframe and the next,
//  or the end of history after the newest. Before the oldest keyframe, the
//  oldest keyframe's cycle.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryMarks::EstimateCycle (
    const KeyframeStore  & keyframes,
    uint64_t               position,
    uint64_t               endPosition,
    uint64_t               endCycle)
{
    size_t    index         = 0;
    uint64_t  fromPosition  = 0;
    uint64_t  fromCycle     = 0;
    uint64_t  toPosition    = endPosition;
    uint64_t  toCycle       = endCycle;
    bool      isFound       = false;



    if (keyframes.GetCount() == 0)
    {
        return endCycle;
    }

    isFound = keyframes.TryFindByPosition (position, index);

    if (!isFound)
    {
        return keyframes.GetInfo (0).cycle;
    }

    fromPosition = keyframes.GetInfo (index).position;
    fromCycle    = keyframes.GetInfo (index).cycle;

    if (index + 1 < keyframes.GetCount())
    {
        toPosition = keyframes.GetInfo (index + 1).position;
        toCycle    = keyframes.GetInfo (index + 1).cycle;
    }

    if (toPosition <= fromPosition || toCycle <= fromCycle || position <= fromPosition)
    {
        return fromCycle;
    }

    position = std::min (position, toPosition);

    return fromCycle + (position - fromPosition) * (toCycle - fromCycle) / (toPosition - fromPosition);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Update
//
//  The record marks persist from one call to the next; the gaps and every
//  mark's cycle come from the keyframes each time a list is made, so a mark
//  past the newest keyframe moves to its exact cycle once the next keyframe
//  is taken.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryMarks::Update (
    const KeyframeStore        & keyframes,
    const InputJournal         & journal,
    uint64_t                     endPosition,
    uint64_t                     endCycle,
    std::vector<HistoryMark>   & outMarks)
{
    HistoryMark  mark;
    bool         isChanged = false;
    size_t       i         = 0;



    isChanged |= ScanJournal      (journal);
    isChanged |= DropOutside      (keyframes, journal);
    isChanged |= HasKeyframeMoved (keyframes);

    if (!isChanged)
    {
        return false;
    }

    outMarks.clear();

    for (const RecordMark & record : m_records)
    {
        mark.kind     = record.kind;
        mark.position = record.position;
        mark.cycle    = EstimateCycle (keyframes, record.position, endPosition, endCycle);
        mark.count    = 1;

        outMarks.push_back (mark);
    }

    for (i = 0; i < keyframes.GetCount(); i++)
    {
        const KeyframeInfo  & info = keyframes.GetInfo (i);



        if (!info.hasGapBefore)
        {
            continue;
        }

        mark.kind     = HistoryMarkKind::Gap;
        mark.position = info.position;
        mark.cycle    = info.cycle;
        mark.count    = 1;

        outMarks.push_back (mark);
    }

    std::stable_sort (outMarks.begin(), outMarks.end(), [] (const HistoryMark & a, const HistoryMark & b) { return a.position < b.position; });

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void HistoryMarks::Clear()
{
    m_records.clear();

    m_scanned       = 0;
    m_cutCount      = 0;
    m_hasScanned    = false;
    m_keyframeCount = 0;
    m_oldest        = 0;
    m_newest        = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScanJournal
//
//  Adds a mark for each record made since the last scan. After a cut, every
//  record is read again. A record of the same kind at the same position as
//  the one before it, as a swap's eject and mount, makes no second mark.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryMarks::ScanJournal (const InputJournal & journal)
{
    HistoryMarkKind  kind      = HistoryMarkKind::Reset;
    RecordMark       mark;
    bool             isChanged = false;
    bool             isSame    = false;
    size_t           i         = 0;



    if (!m_hasScanned || journal.GetCutCount() != m_cutCount)
    {
        isChanged    = !m_records.empty() || !m_hasScanned;
        m_hasScanned = true;
        m_cutCount   = journal.GetCutCount();
        m_scanned    = journal.GetBeginIndex();

        m_records.clear();
    }

    m_scanned = std::max (m_scanned, journal.GetBeginIndex());

    for (i = m_scanned; i < journal.GetEndIndex(); i++)
    {
        const InputRecord  & record = journal.GetRecord (i);



        if (!TryGetKind (record.kind, kind))
        {
            continue;
        }

        isSame = !m_records.empty() && m_records.back().kind == kind && m_records.back().position == record.position;

        if (isSame)
        {
            continue;
        }

        mark.kind     = kind;
        mark.position = record.position;
        mark.index    = i;

        m_records.push_back (mark);
        isChanged = true;
    }

    m_scanned = journal.GetEndIndex();

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropOutside
//
//  Drops the marks whose records the journal has dropped, or that lie
//  before the oldest keyframe, outside history.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryMarks::DropOutside (
    const KeyframeStore  & keyframes,
    const InputJournal   & journal)
{
    uint64_t  oldest    = (keyframes.GetCount() > 0) ? keyframes.GetInfo (0).position : UINT64_MAX;
    bool      isChanged = false;



    while (!m_records.empty() && (m_records.front().index < journal.GetBeginIndex() || m_records.front().position < oldest))
    {
        m_records.pop_front();
        isChanged = true;
    }

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasKeyframeMoved
//
//  Whether a keyframe has been taken or dropped since the last call, which
//  moves a gap or the cycles marks are estimated at.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryMarks::HasKeyframeMoved (const KeyframeStore & keyframes)
{
    size_t    count   = keyframes.GetCount();
    uint64_t  oldest  = (count > 0) ? keyframes.GetInfo (0).position : 0;
    uint64_t  newest  = (count > 0) ? keyframes.GetInfo (count - 1).position : 0;
    bool      isMoved =count != m_keyframeCount || oldest != m_oldest || newest != m_newest;



    m_keyframeCount = count;
    m_oldest        = oldest;
    m_newest        = newest;

    return isMoved;
}

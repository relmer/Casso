#include "Pch.h"

#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/KeyframeLayout.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Fork
//
//  The keyframes at and after position, and the journal's records from
//  journalIndex on, become a branch left behind; cycle is the history cycle
//  at position. With no keyframe there, no branch is made, and the
//  records go as a cut drops them. The newest group left is reloaded, as
//  after a cut. More than kMaxBranches branches then drops the oldest.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Fork (
    uint64_t         position,
    uint64_t         cycle,
    InputJournal   & journal,
    size_t           journalIndex)
{
    HRESULT  hr      = S_OK;
    bool     isMoved = false;



    hr = WaitForPending();
    CHR (hr);

    isMoved = m_arena.MoveTailToBranch (position, cycle, journal, journalIndex, m_wallClock());

    if (isMoved)
    {
        hr = FinishPathChange();
        CHR (hr);
    }

    m_arena.CapBranches();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SwitchTo
//
//  The branch, and each branch it forks from, moved onto the active path,
//  the tail moved off the path left as a branch. PathChanged then goes to
//  the listeners, with the first fork position.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::SwitchTo (
    uint32_t         branchId,
    InputJournal   & journal)
{
    HRESULT  hr = S_OK;



    hr = WaitForPending();
    CHR (hr);

    hr = m_arena.SwitchTo (branchId, journal, KeyframeLayout::GetGroupSlots (m_settings), m_wallClock());
    CHR (hr);

    hr = FinishPathChange();
    CHR (hr);

    NotifyDrop (KeyframeDrop::PathChanged);

    m_arena.CapBranches();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishPathChange
//
//  After a fork or a switch: the newest group left is reloaded and the next
//  keyframe scheduled after it, or with no keyframe left on the active path,
//  the next one opens a group at once.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::FinishPathChange()
{
    HRESULT  hr = S_OK;



    if (m_arena.GetCount() == 0)
    {
        m_packQueue.GetCodec().ClearLatest();
        m_arena.ResetGroup();

        m_nextDueCycle = 0;
        BAIL_OUT_IF (true, S_OK);
    }

    hr = ReloadLatestWhole();
    CHR (hr);

    ScheduleAfter (m_arena.GetNewestInfo().cycle);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyBranchEnd
//
//  As CopyPacked, for the live end of a branch left behind: the newest of
//  its keyframes, against the whole snapshot of its group, in the branch or
//  on the active path before the fork. Gone for a branch not in the store, or
//  one whose group's whole snapshot is in another branch.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::CopyBranchEnd (
    uint32_t                  branchId,
    const KeyframeUnpacker  & unpacker,
    PackedKeyframe          & outPacked,
    KeyframeCopy            & outCopy)
{
    HRESULT                hr      = S_OK;
    bool                   isFound = false;
    KeyframeInfo           whole;
    KeyframeInfo           end;
    KeyframeCodec::Packed  wholePacked;
    KeyframeCodec::Packed  endPacked;



    outCopy = KeyframeCopy::Gone;

    isFound = m_arena.TryGetBranchEnd (branchId, whole, wholePacked, end, endPacked);
    BAIL_OUT_IF (!isFound, S_OK);

    outCopy = KeyframeCopy::Copied;

    outPacked.wholePosition = whole.position;
    outPacked.wholeChecksum = whole.checksum;
    outPacked.stateBytes    = end.stateBytes;
    outPacked.isWhole       = end.isWhole;

    outPacked.whole.clear();
    outPacked.difference.clear();

    if (!unpacker.HoldsWhole (whole.position, whole.checksum))
    {
        outPacked.whole.assign (wholePacked.data, wholePacked.data + wholePacked.packedBytes);
    }

    if (!end.isWhole)
    {
        outPacked.difference.assign (endPacked.data, endPacked.data + endPacked.packedBytes);
    }

Error:
    return hr;
}

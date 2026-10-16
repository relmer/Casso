#include "Pch.h"

#include "Debugger/Events/DebugEventHistory.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventHistory::DebugEventHistory
//
////////////////////////////////////////////////////////////////////////////////

DebugEventHistory::DebugEventHistory (MachineHost & machine)
    : m_machine (machine)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventHistory::GetStretches
//
//  From each keyframe to the next, or to where recording paused when a gap
//  lies before the next.
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventHistory::GetStretches (std::vector<DebugEventBounds> & outStretches) const
{
    size_t  count = (m_keyframes != nullptr) ? m_keyframes->GetCount() : 0;
    size_t  i     = 0;



    outStretches.clear();

    for (i = 0; i + 1 < count; i++)
    {
        const KeyframeInfo  & start = m_keyframes->GetInfo (i);
        const KeyframeInfo  & next  = m_keyframes->GetInfo (i + 1);

        outStretches.push_back ({ start.position, next.hasGapBefore ? next.gapStart : next.position, next.hasGapBefore });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventHistory::BuildJob
//
//  The stretches, newest first as given, each with its packed keyframe
//  copied out; the journal's inputs from the oldest stretch's keyframe
//  through the newest's end; and the disks in the bays.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugEventHistory::BuildJob (
    const std::vector<DebugEventBounds>  & stretches,
    DebugEventJob                        & outJob)
{
    HRESULT  hr       = S_OK;
    bool     isCopied = false;
    bool     isValid  = m_keyframes != nullptr && !stretches.empty();



    outJob = DebugEventJob();

    CBRAEx (isValid, E_INVALIDARG);

    for (const DebugEventBounds & bounds : stretches)
    {
        hr = AddStretch (bounds, outJob);
        CHR (hr);
    }

    outJob.inputsFrom = outJob.stretches.back().journalIndex;
    outJob.generation = ++m_generation;

    isCopied = m_machine.GetInputJournal().TryCopyRecords (outJob.inputsFrom, outJob.stretches.front().endPosition, outJob.inputs);
    CBRA (isCopied);

    hr = m_diskCopier.Copy (m_machine.GetDiskStore(), outJob.disks);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventHistory::AddStretch
//
//  The stretch whose keyframe is at the bounds' start, a keyframe history
//  must have; the end's cycle is the next keyframe's when the bounds end
//  there, and zero otherwise: before a gap, since KeyframeInfo has no cycle
//  for a gap's start, and for part of a stretch or the live one after the
//  newest keyframe, which end at the machine's position.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugEventHistory::AddStretch (
    const DebugEventBounds  & bounds,
    DebugEventJob           & ioJob)
{
    HRESULT             hr      = S_OK;
    size_t              index   = 0;
    bool                isFound = false;
    bool                hasNext = false;
    KeyframeCopy        copy    = KeyframeCopy::Gone;
    DebugEventStretch   stretch;



    isFound = m_keyframes->TryFindByPosition (bounds.startPosition, index);
    CBREx (isFound, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hasNext = index + 1 < m_keyframes->GetCount();

    hr = m_keyframes->CopyPacked (bounds.startPosition, m_noUnpacker, stretch.start, copy);
    CHR (hr);

    CBREx (copy == KeyframeCopy::Copied, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    stretch.startPosition = bounds.startPosition;
    stretch.startCycle    = m_keyframes->GetInfo (index).cycle;
    stretch.journalIndex  = m_keyframes->GetInfo (index).journalIndex;
    stretch.endPosition   = bounds.endPosition;
    stretch.endCycle      = (hasNext && !bounds.hasGapAfter && bounds.endPosition == m_keyframes->GetInfo (index + 1).position)
                          ? m_keyframes->GetInfo (index + 1).cycle : 0;

    ioJob.stretches.push_back (std::move (stretch));

Error:
    return hr;
}

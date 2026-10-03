#include "Pch.h"

#include "Debugger/Reverse/Replayer.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DebugHook.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/UndoRing.h"
#include "Devices/Disk/DiskImage.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Replayer::Replayer
//
////////////////////////////////////////////////////////////////////////////////

Replayer::Replayer (
    MachineHost    & machine,
    KeyframeStore  & keyframes,
    UndoRing       & ring) :
    m_machine   (machine),
    m_keyframes (keyframes),
    m_ring      (ring)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreKeyframe
//
//  Loads keyframe index. The ring keeps what it holds before the keyframe's
//  position when the keyframe falls inside it, and starts over otherwise.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::RestoreKeyframe (size_t index)
{
    HRESULT       hr       = S_OK;
    bool          isValid  = index < m_keyframes.GetCount();
    KeyframeInfo  info;
    bool          isInRing = false;



    CBRAEx (isValid, E_INVALIDARG);

    info = m_keyframes.GetInfo (index);

    hr = m_keyframes.Restore (index, m_scratch);
    CHR (hr);

    hr = LoadState (m_scratch, info.position, info.journalIndex);
    CHR (hr);

    isInRing = info.position >= m_ring.GetFirstPosition() && info.position <= m_ring.GetEndPosition();

    if (isInRing)
    {
        m_ring.TruncateAt (info.position);
    }
    else
    {
        m_ring.Clear();
    }

    FindNextKeyframe (info.position);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreCheckpoint
//
//  Loads the ring's checkpoint index and drops what the ring holds after it.
//  A checkpoint holding shared segments is flattened into the scratch buffer
//  first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::RestoreCheckpoint (size_t index)
{
    HRESULT                  hr         = S_OK;
    bool                     isValid    = index < m_ring.GetCheckpointCount();
    const UndoCheckpoint   * checkpoint = nullptr;
    bool                     isShared   = false;
    uint64_t                 position   = 0;



    CBRAEx (isValid, E_INVALIDARG);

    checkpoint = &m_ring.GetCheckpoint (index);
    isShared   = !checkpoint->segments.empty();
    position   = checkpoint->position;

    if (isShared)
    {
        StateWriter::Flatten (checkpoint->state, checkpoint->segments, m_scratch);
    }

    hr = LoadState (isShared ? m_scratch : checkpoint->state, position, checkpoint->journalIndex);
    CHR (hr);

    m_ring.TruncateAt (position);

    FindNextKeyframe (position);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunTo
//
//  Runs from where the machine stands to target, never past endPosition, the
//  end of recorded history. With a stop test, every instruction is asked
//  about and the latest position before endPosition that it fired at is
//  reported; the replay itself does not stop there. A divergence ends the
//  replay at once, with the machine at the keyframe that failed.
//
//  The disk store is told a replay is running, so no flush reaches a file
//  and no changed file is taken up; a boundary keyframe the replay reaches
//  is loaded rather than checked.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::RunTo (
    const ReplayTarget  & target,
    uint64_t              endPosition,
    IReverseStopTest    * stopTest,
    ReplayReport        & report)
{
    HRESULT      hr       = S_OK;
    DebugHook  * hook     = m_machine.GetDebugHook();
    EmuCpu     * cpu      = m_machine.GetCpu();
    uint64_t     position = 0;
    uint64_t     cycle    = 0;
    bool         isDone   = false;



    CBRA (cpu);

    report = ReplayReport();

    // A watchpoint the restore or an earlier run left pending belongs to no
    // instruction of this replay.
    if (stopTest != nullptr)
    {
        stopTest->TakePendingStop();
    }

    m_machine.SetDebugHook (nullptr);
    m_machine.SetInputJournalOn (false);
    m_machine.GetDiskStore().SetReplaying (true);
    m_isReplaying = true;

    if (m_outputGate)
    {
        m_outputGate (true);
    }

    while (!isDone)
    {
        position = m_machine.GetPosition();
        cycle    = cpu->GetTotalCycles();

        hr = ApplyInputs (position, false);
        CHR (hr);

        hr = LoadBoundaryIfDue();
        CHR (hr);

        hr = TakeCheckpointIfDue();
        CHR (hr);

        hr = CheckKeyframe (report);
        CHR (hr);

        isDone = report.isDiverged || position >= target.position || cycle >= target.cycle || position >= endPosition;

        if (!isDone)
        {
            hr = Step (stopTest, endPosition, report);
            CHR (hr);
        }
    }

Error:
    m_isReplaying = false;
    m_machine.GetDiskStore().SetReplaying (false);

    if (m_outputGate)
    {
        m_outputGate (false);
    }

    m_machine.SetDebugHook (hook);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Step
//
//  One instruction of a replay: the stop test's breakpoint before it, the
//  observed inputs it reads, the instruction, and the stop test's watchpoint
//  after it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::Step (
    IReverseStopTest  * stopTest,
    uint64_t            endPosition,
    ReplayReport      & report)
{
    HRESULT   hr       = S_OK;
    uint64_t  position = m_machine.GetPosition();
    uint64_t  after    = 0;



    if (stopTest != nullptr && stopTest->ShouldStopBefore (m_machine, m_machine.GetCpu()->GetPC()))
    {
        report.hasHit  = true;
        report.lastHit = position;
    }

    hr = ApplyInputs (position, true);
    CHR (hr);

    m_machine.StepOne();

    after = m_machine.GetPosition();

    if (stopTest != nullptr && stopTest->TakePendingStop() && after < endPosition)
    {
        report.hasHit  = true;
        report.lastHit = after;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  The machine's state, position and journal cursor, then the host's
//  re-derivation (the video mode) through the loaded callback.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::LoadState (
    const std::vector<Byte>  & state,
    uint64_t                   position,
    size_t                     journalIndex)
{
    HRESULT      hr     = S_OK;
    StateReader  reader (state);



    hr = m_machine.LoadState (reader);
    CHR (hr);

    m_machine.SetPosition (position);
    m_journalCursor = journalIndex;

    if (m_onStateLoaded)
    {
        m_onStateLoaded();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyInputs
//
//  Applies, in journal order, every record before position, and the records
//  at position: the boundary ones always, and the observed ones too when
//  includeObserved is set, because the instruction at position is next.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::ApplyInputs (
    uint64_t  position,
    bool      includeObserved)
{
    HRESULT               hr      = S_OK;
    const InputJournal  & journal = m_machine.GetInputJournal();
    const InputRecord   * record  = nullptr;
    bool                  isDue   = false;



    while (m_journalCursor < journal.GetEndIndex())
    {
        record = &journal.GetRecord (m_journalCursor);
        isDue  = record->position < position || (record->position == position && (includeObserved || !record->isObserved));

        if (!isDue)
        {
            break;
        }

        hr = ApplyInput (*record);
        CHR (hr);

        m_journalCursor++;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyInput
//
//  A record a device holds goes back to that device. A reset or power cycle
//  is carried out on the machine; a power cycle draws new memory from the
//  Prng, which no snapshot holds, so the next keyframe's checksum will catch
//  the difference. A drive's write-protect switch is set on its disk. A
//  mount, an eject or a change to an image's write protection is not redone
//  from the file: the boundary keyframe taken just after it holds the disks
//  as they were, and LoadBoundaryIfDue loads it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::ApplyInput (const InputRecord & record)
{
    HRESULT      hr           = S_OK;
    bool         isApplied    = m_machine.ApplyDeviceInput (record);
    bool         isReset      = record.kind == InputKind::Reset;
    bool         isPowerCycle = record.kind == InputKind::PowerCycle;
    bool         isHostState  = record.kind == InputKind::HostState;
    bool         isDriveWp    = record.kind == InputKind::DriveWriteProtect;
    bool         isMedia      = record.kind == InputKind::DiskMount || record.kind == InputKind::DiskEject || record.kind == InputKind::ImageWriteProtect;
    DiskImage  * image        = nullptr;



    BAIL_OUT_IF (isApplied || isMedia, S_OK);

    CBREx (isReset || isPowerCycle || isHostState || isDriveWp, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    if (isDriveWp)
    {
        image = m_machine.GetDiskStore().GetImage (kDiskControllerSlot, record.value);

        if (image != nullptr)
        {
            image->SetUserWriteProtected (record.detail != 0);
        }
    }
    else if (isHostState)
    {
        hr = m_machine.LoadHostInputState (record.payload);
        CHR (hr);
    }
    else if (isReset)
    {
        m_machine.SoftReset();
    }
    else
    {
        m_machine.PowerCycle();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadBoundaryIfDue
//
//  At the position of a boundary keyframe the machine was changed from
//  outside its recorded inputs (a disk went in or out, or memory was edited),
//  so the replay takes the keyframe's state instead of computing it. The ring
//  keeps what the replay has added before it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::LoadBoundaryIfDue()
{
    HRESULT       hr       = S_OK;
    uint64_t      position = m_machine.GetPosition();
    bool          isDue    = m_nextKeyframe < m_keyframes.GetCount() && m_keyframes.GetInfo (m_nextKeyframe).position == position;
    KeyframeInfo  info;



    BAIL_OUT_IF (!isDue, S_OK);

    info = m_keyframes.GetInfo (m_nextKeyframe);

    BAIL_OUT_IF (!info.isBoundary, S_OK);

    hr = m_keyframes.Restore (m_nextKeyframe, m_scratch);
    CHR (hr);

    hr = LoadState (m_scratch, info.position, info.journalIndex);
    CHR (hr);

    m_nextKeyframe++;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeCheckpointIfDue
//
//  The ring's checkpoints are taken where the replay applies boundary
//  inputs, which is where a live run takes them, so a checkpoint from a
//  replay and one from the live run hold the same state.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::TakeCheckpointIfDue()
{
    HRESULT        hr     = S_OK;
    uint64_t       cycle  = m_machine.GetCpu()->GetTotalCycles();
    bool           isDue  = m_ring.IsCheckpointDue (cycle);
    StateWriter  & writer = m_writer;



    BAIL_OUT_IF (!isDue, S_OK);

    writer.Reuse      (m_ring.TakeSpareBuffer(), m_ring.TakeSpareSegments());
    writer.SetSharing (true);

    hr = m_machine.SaveState (writer);
    CHR (hr);

    hr = m_ring.AddCheckpoint (m_machine.GetPosition(), cycle, m_journalCursor, writer.TakeBytes(), writer.TakeSegments());
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CheckKeyframe
//
//  At the position of the next keyframe, the replayed machine must hash to
//  that keyframe's checksum.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::CheckKeyframe (ReplayReport & report)
{
    HRESULT      hr       = S_OK;
    uint64_t     position = m_machine.GetPosition();
    bool         isDue    = m_nextKeyframe < m_keyframes.GetCount() && m_keyframes.GetInfo (m_nextKeyframe).position == position;
    StateWriter  writer;
    bool         isMatch  = false;



    BAIL_OUT_IF (!isDue, S_OK);

    hr = m_machine.SaveState (writer);
    CHR (hr);

    isMatch = m_keyframes.DoesStateMatch (m_nextKeyframe, writer.GetBytes());

    if (!isMatch)
    {
        report.isDiverged       = true;
        report.divergedKeyframe = m_nextKeyframe;
    }

    m_nextKeyframe++;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindNextKeyframe
//
//  The first keyframe after afterPosition, the next one a replay checks.
//
////////////////////////////////////////////////////////////////////////////////

void Replayer::FindNextKeyframe (uint64_t afterPosition)
{
    size_t  count = m_keyframes.GetCount();



    m_nextKeyframe = 0;

    while (m_nextKeyframe < count && m_keyframes.GetInfo (m_nextKeyframe).position <= afterPosition)
    {
        m_nextKeyframe++;
    }
}

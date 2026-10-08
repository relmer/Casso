#include "Pch.h"

#include "Debugger/Reverse/Replayer.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DebugHook.h"
#include "Debugger/DebugMemoryView.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Devices/Disk/DiskImage.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Replayer::Replayer
//
////////////////////////////////////////////////////////////////////////////////

Replayer::Replayer (
    MachineHost    & machine,
    KeyframeStore  & keyframes) :
    m_machine   (machine),
    m_keyframes (keyframes)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreKeyframe
//
//  Loads keyframe index. The store's keyframes in flight must have been
//  waited for before index was taken.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::RestoreKeyframe (size_t index)
{
    HRESULT       hr      = S_OK;
    bool          isValid = index < m_keyframes.GetCount();
    KeyframeInfo  info;



    CBRAEx (isValid, E_INVALIDARG);

    info = m_keyframes.GetInfo (index);

    m_restoreCount++;

    hr = m_keyframes.Restore (index, m_scratch);
    CHR (hr);

    hr = LoadState (m_scratch, info.position, info.journalIndex);
    CHR (hr);

    FindNextKeyframe (info.position);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadFrom
//
//  Loads a state the store does not hold, as RestoreKeyframe loads one it
//  does; the next keyframe to check is the store's first after position.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::LoadFrom (
    const std::vector<Byte>  & state,
    uint64_t                   position,
    size_t                     journalIndex)
{
    HRESULT  hr = S_OK;



    hr = LoadState (state, position, journalIndex);
    CHR (hr);

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
//  With steps, the registers and bytes of every instruction the replay runs
//  are appended to it, one per position; with stackPointers, only the stack
//  pointer each began with, which is all a search for a call needs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::RunTo (
    const ReplayTarget       & target,
    uint64_t                   endPosition,
    IReverseStopTest         * stopTest,
    ReplayReport             & report,
    std::vector<ReplayStep>  * steps,
    std::vector<Byte>        * stackPointers)
{
    HRESULT       hr        = S_OK;
    DebugHook   * hook      = m_machine.GetDebugHook();
    IWatchSink  * watchSink = m_machine.GetMemoryBus().GetWatchSink();
    EmuCpu      * cpu       = m_machine.GetCpu();
    uint64_t      position  = 0;
    uint64_t      cycle     = 0;
    bool          isDone    = false;



    CBRA (cpu);

    report = ReplayReport();

    // A watchpoint the restore or an earlier run left pending belongs to no
    // instruction of this replay.
    if (stopTest != nullptr)
    {
        stopTest->TakePendingStop();
    }

    // The debugger's watchpoints would log and count what the replay
    // touches; only the stop test's checker sees it.
    m_machine.GetMemoryBus().SetWatchSink ((stopTest != nullptr) ? stopTest->GetWatchSink() : nullptr);

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

        hr = CheckKeyframe (report);
        CHR (hr);

        isDone = report.isDiverged || position >= target.position || cycle >= target.cycle || position >= endPosition;

        if (!isDone)
        {
            hr = Step (stopTest, endPosition, report, steps, stackPointers);
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
    m_machine.GetMemoryBus().SetWatchSink (watchSink);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrepareStepHere
//
//  Everything a replay does before an instruction, for one the machine is
//  about to run itself: the inputs recorded up to and at its position, a
//  boundary keyframe due there, and the check of a keyframe due there; the
//  records the instruction itself observed only when it is about to run,
//  not when the machine has just arrived there. The machine then runs the instruction as it ordinarily would, with the debug
//  hook and the debugger's watchpoints attached, so running forward from the
//  past replays the recorded future while breakpoints still stop it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::PrepareStepHere (
    ReplayReport  & report,
    bool            includeObserved)
{
    HRESULT   hr       = S_OK;
    uint64_t  position = m_machine.GetPosition();



    report = ReplayReport();

    hr = ApplyInputs (position, false);
    CHR (hr);

    hr = LoadBoundaryIfDue();
    CHR (hr);

    hr = CheckKeyframe (report);
    CHR (hr);

    BAIL_OUT_IF (report.isDiverged, S_OK);

    hr = ApplyInputs (m_machine.GetPosition(), includeObserved);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Step
//
//  One instruction of a replay: its registers and bytes when they are
//  collected, the stop test's breakpoint before it, the observed inputs it
//  reads, the instruction, and the stop test's watchpoint after it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::Step (
    IReverseStopTest         * stopTest,
    uint64_t                   endPosition,
    ReplayReport             & report,
    std::vector<ReplayStep>  * steps,
    std::vector<Byte>        * stackPointers)
{
    HRESULT   hr       = S_OK;
    EmuCpu  * cpu      = m_machine.GetCpu();
    uint64_t  position = m_machine.GetPosition();
    uint64_t  after    = 0;



    if (steps != nullptr)
    {
        steps->push_back (MakeStep (*cpu));
    }

    if (stackPointers != nullptr)
    {
        stackPointers->push_back (cpu->GetSP());
    }

    if (stopTest != nullptr && stopTest->ShouldStopBefore (m_machine, cpu->GetPC()))
    {
        report.hasHit  = true;
        report.lastHit = position;
    }

    hr = ApplyInputs (position, true);
    CHR (hr);

    m_machine.StepOne();
    m_replayedCount++;

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
//  MakeStep
//
//  The instruction the CPU is about to run: its registers and cycle count,
//  and its three bytes as the CPU sees them under the current banking, read
//  through the debugger's side-effect-free view. A byte the view cannot
//  read without touching a device is left zero.
//
////////////////////////////////////////////////////////////////////////////////

ReplayStep Replayer::MakeStep (const EmuCpu & cpu) const
{
    Cpu6502Registers  registers = cpu.GetCpu6502()->GetRegisters();
    DebugMemoryView   memory    (m_machine);
    ReplayStep        step;
    bool              isRead    = false;



    step.cycles = cpu.GetTotalCycles();
    step.pc     = registers.pc;
    step.sp     = registers.sp;
    step.a      = registers.a;
    step.x      = registers.x;
    step.y      = registers.y;
    step.p      = registers.p;

    isRead = memory.TryPeek (registers.pc, step.opcode);
    IGNORE_RETURN_VALUE (isRead, false);

    isRead = memory.TryPeek ((Word) (registers.pc + 1), step.op1);
    IGNORE_RETURN_VALUE (isRead, false);

    isRead = memory.TryPeek ((Word) (registers.pc + 2), step.op2);
    IGNORE_RETURN_VALUE (isRead, false);

    return step;
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



    hr = m_isOverMounted ? m_machine.LoadStateOverMountedMedia (reader) : m_machine.LoadState (reader);
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
//  is made again on the machine, and the reset callback told of it; a power
//  cycle draws the same memory as it did live, since every keyframe holds
//  the Prng, and leaves the drives on the disks in their bays. A drive's
//  write-protect switch is set on its disk. A mount, an eject or a change to
//  an image's write protection is not redone from the file: the boundary
//  keyframe taken just after it holds the disks as they were, and
//  LoadBoundaryIfDue loads it.
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

    if ((isReset || isPowerCycle) && m_onReset)
    {
        m_onReset (isPowerCycle);
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
//  so the replay takes the keyframe's state instead of computing it.
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
//  CheckKeyframe
//
//  At the position of the next keyframe, the replayed machine must hash to
//  that keyframe's checksum.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Replayer::CheckKeyframe (ReplayReport & report)
{
    HRESULT        hr       = S_OK;
    uint64_t       position = m_machine.GetPosition();
    bool           isDue    = m_nextKeyframe < m_keyframes.GetCount() && m_keyframes.GetInfo (m_nextKeyframe).position == position;
    StateWriter  & writer   = m_checkWriter;
    bool           isMatch  = false;



    BAIL_OUT_IF (!isDue, S_OK);

    writer.Reuse (writer.TakeBytes());

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

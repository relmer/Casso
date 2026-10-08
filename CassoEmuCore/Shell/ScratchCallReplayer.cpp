#include "Pch.h"

#include "Shell/ScratchCallReplayer.h"

#include "Debugger/Reverse/Replayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::ScratchCallReplayer
//
////////////////////////////////////////////////////////////////////////////////

ScratchCallReplayer::ScratchCallReplayer()
{
    CallStackRecorder::MarkOpcodes (m_opcodes.data());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::~ScratchCallReplayer
//
//  The job running is abandoned and waited for before the machine goes.
//
////////////////////////////////////////////////////////////////////////////////

ScratchCallReplayer::~ScratchCallReplayer()
{
    Cancel();
    WaitForWork();
    DetachRecorder();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::SetMachine
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::SetMachine (
    const MachineConfig  & config,
    const std::wstring   & name)
{
    m_scratch.SetMachine (config, name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::WaitForWork
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::WaitForWork()
{
    if (m_queue != nullptr)
    {
        m_queue->WaitAll();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::Submit
//
//  The job waits in the one slot, replacing any there, and the one running
//  is abandoned unless it is of the same generation; a worker is started
//  when none is running. The progress starts again from nothing, so the
//  last job's is not shown for this one while it waits.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchCallReplayer::Submit (std::shared_ptr<const CallStackRebuildJob> job)
{
    HRESULT  hr      = S_OK;
    bool     isStart = false;



    CBRAEx (job, E_INVALIDARG);

    hr = UseQueue();
    CHR (hr);

    m_progress.store (0.0f, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex>  held (m_lock);

        m_next = job;
        m_latest.store (job->generation, std::memory_order_release);

        isStart     = !m_isRunning;
        m_isRunning = true;
    }

    if (isStart)
    {
        hr = m_queue->Submit (RunJob, this);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::TryTakeResult
//
////////////////////////////////////////////////////////////////////////////////

bool ScratchCallReplayer::TryTakeResult (CallStackRebuildResult & outResult)
{
    std::lock_guard<std::mutex>  held   (m_lock);
    bool                         hasOne = !m_results.empty();



    if (hasOne)
    {
        outResult = std::move (m_results.front());
        m_results.pop_front();
    }

    return hasOne;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::Cancel
//
//  Nothing waiting is run, the job running stops at its next check, and the
//  results not yet taken are dropped.
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::Cancel()
{
    std::lock_guard<std::mutex>  held (m_lock);



    m_next.reset();
    m_results.clear();
    m_latest.store (0, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::Rebuild
//
//  On the calling thread, once the worker has finished: the job becomes the
//  one wanted, so nothing abandons it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchCallReplayer::Rebuild (
    const CallStackRebuildJob  & job,
    CallStackRebuildResult     & outResult)
{
    HRESULT  hr = S_OK;



    WaitForWork();

    m_latest.store (job.generation, std::memory_order_release);

    hr = Run (job, outResult);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::OnWatchedFetch
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::OnWatchedFetch (Word pc, Byte sp, Byte opcode)
{
    m_recorder.OnInstruction (pc, sp, opcode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::ShouldStopBefore
//
////////////////////////////////////////////////////////////////////////////////

bool ScratchCallReplayer::ShouldStopBefore (MachineHost & machine, Word pc)
{
    UNREFERENCED_PARAMETER (machine);
    UNREFERENCED_PARAMETER (pc);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::TakePendingStop
//
////////////////////////////////////////////////////////////////////////////////

bool ScratchCallReplayer::TakePendingStop()
{
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::OnWatchedAccess
//
//  Only the stack page is watched, and only its writes reach the record.
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::OnWatchedAccess (Word address, Byte value, BusAccess access, std::optional<Byte> previous)
{
    if (access == BusAccess::Write && (address >> kPageShift) == kStackPage)
    {
        m_recorder.OnStackWrite (address, value, previous);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::RunJob
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::RunJob (void * context)
{
    static_cast<ScratchCallReplayer *> (context)->RunPending();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::RunPending
//
//  Replays the job waiting, then any that arrived meanwhile, and stops when
//  none waits. A result is kept for the job still wanted only.
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::RunPending()
{
    std::shared_ptr<const CallStackRebuildJob>  job;
    CallStackRebuildResult                      result;
    HRESULT                                     hr = S_OK;



    for (;;)
    {
        {
            std::lock_guard<std::mutex>  held (m_lock);

            job = std::move (m_next);
            m_next.reset();

            if (job == nullptr)
            {
                m_isRunning = false;
                break;
            }
        }

        hr = Run (*job, result);
        IGNORE_RETURN_VALUE (hr, S_OK);

        {
            std::lock_guard<std::mutex>  held (m_lock);

            if (!IsAbandoned (*job))
            {
                m_results.push_back (std::move (result));
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::Run
//
//  The parts in order, from the newest that starts the record again for a
//  fresh job, then the record as of where the last one ended, its last
//  instruction settled. A job that cannot reach its end -- its last part
//  was saved with other disks, say -- fails. The result holds how long the
//  job took and how many instructions it replayed whether or not it got
//  through; its hr says which. A job that fails leaves nothing for the next
//  to continue.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchCallReplayer::Run (
    const CallStackRebuildJob  & job,
    CallStackRebuildResult     & outResult)
{
    HRESULT                                hr          = S_OK;
    Replayer                             * replayer    = nullptr;
    uint64_t                               before      = 0;
    size_t                                 first       = 0;
    size_t                                 index       = 0;
    bool                                   isThrough   = false;
    bool                                   isAbandoned = false;
    std::chrono::steady_clock::time_point  began       = std::chrono::steady_clock::now();



    outResult            = CallStackRebuildResult();
    outResult.generation = job.generation;

    hr = Start (job);
    CHR (hr);

    if (!job.isContinued && job.parts.size() > 1)
    {
        hr = FindStartPart (job, first);
        CHR (hr);
    }

    if (first > 0)
    {
        m_isRestartDue = true;
    }

    m_jobStart = job.parts[first].startPosition;

    replayer = m_scratch.GetReplayer();
    CBRA (replayer);

    before = replayer->GetReplayedCount();

    for (index = first; index < job.parts.size(); index++)
    {
        isAbandoned = IsAbandoned (job);
        BAIL_OUT_IF (isAbandoned, E_ABORT);

        hr = RunPart (job, job.parts[index]);
        CHR (hr);
    }

    isThrough = m_isBegun && m_scratch.GetMachine()->GetPosition() == m_jobEnd;
    CBREx (isThrough, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    Finish (outResult);

    m_ongoing = job.generation;

Error:
    if (FAILED (hr))
    {
        m_ongoing = 0;
    }

    if (replayer != nullptr)
    {
        outResult.instructions = replayer->GetReplayedCount() - before;
    }

    DetachRecorder();

    outResult.hr = hr;
    outResult.ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - began).count();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::Start
//
//  A fresh job brings the machine up to date, with a view of it for the
//  record to read, and begins the record anew; one that continues needs the
//  machine and record the last job of its generation left. Either way the
//  job's inputs are the journal from here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchCallReplayer::Start (const CallStackRebuildJob & job)
{
    HRESULT        hr         = S_OK;
    MachineHost  * machine    = nullptr;
    bool           hasParts   = !job.parts.empty();
    bool           isResuming = false;



    CBRAEx (hasParts, E_INVALIDARG);

    m_jobEnd = job.parts.back().endPosition;
    m_progress.store (0.0f, std::memory_order_relaxed);

    if (job.isContinued)
    {
        isResuming = m_ongoing == job.generation && m_ongoing != 0;
        CBREx (isResuming, E_UNEXPECTED);
    }
    else
    {
        m_ongoing      = 0;
        m_isBegun      = false;
        m_isRestartDue = false;
        m_recordFrom   = 0;

        hr = m_scratch.Build();
        CHR (hr);
    }

    machine = m_scratch.GetMachine();
    CBRA (machine);

    if (!job.isContinued)
    {
        m_view = std::make_unique<DebugMemoryView> (*machine);
    }

    machine->GetInputJournal().LoadRecords (job.inputsFrom, job.inputs);

    AttachRecorder();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::FindStartPart
//
//  The newest part of a fresh job that starts the record again: one after a
//  gap saved with the job's disks, or the first saved with them after one
//  saved with other disks, as when a disk put in a drive is taken out again.
//  The record keeps nothing from the parts before it, so they are not
//  replayed. Only each part's saved media is read, newest first. 0 when no
//  part starts the record again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchCallReplayer::FindStartPart (
    const CallStackRebuildJob  & job,
    size_t                     & outStart)
{
    HRESULT                 hr          = S_OK;
    size_t                  index       = job.parts.size();
    bool                    isSame      = false;
    bool                    isNewerSame = false;
    MachineHost::MediaIds   saved       = {};



    outStart = 0;

    while (index > 0)
    {
        index--;

        hr = ReadPartMedia (job.parts[index], saved);
        CHR (hr);

        isSame = IsSavedWith (saved, job.disks);

        if (!isSame && isNewerSame)
        {
            outStart = index + 1;
            break;
        }

        if (isSame && job.parts[index].isAfterGap)
        {
            outStart = index;
            break;
        }

        isNewerSame = isSame;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::ReadPartMedia
//
//  The part's keyframe, unpacked into the state a load takes, and the media
//  its bays held when it was saved.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchCallReplayer::ReadPartMedia (
    const CallStackRebuildPart  & part,
    MachineHost::MediaIds       & outSaved)
{
    HRESULT  hr = S_OK;



    hr = m_unpacker.Unpack (part.start, m_state);
    CHR (hr);

    hr = MachineHost::ReadSavedMedia (m_state, outSaved);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::RunPart
//
//  A part that loads its keyframe begins the record when none has begun or
//  it must begin again; a part whose keyframe was saved with other disks is
//  passed over, and the next one loaded begins the record again. A part
//  that continues starts where the machine stands.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchCallReplayer::RunPart (
    const CallStackRebuildJob   & job,
    const CallStackRebuildPart  & part)
{
    HRESULT   hr       = S_OK;
    bool      isLoaded = false;
    uint64_t  position = m_scratch.GetMachine()->GetPosition();



    if (!part.isLoaded)
    {
        CBREx (position == part.startPosition, E_UNEXPECTED);
    }
    else
    {
        hr = LoadPart (job, part, isLoaded);
        CHR (hr);

        BAIL_OUT_IF (!isLoaded, S_OK);
    }

    hr = ReplayPart (job, part);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::LoadPart
//
//  The instruction the record holds ran before the keyframe, so it is
//  settled first, from the registers it left; then the keyframe is loaded
//  over the job's disks, which it must have been saved with.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchCallReplayer::LoadPart (
    const CallStackRebuildJob   & job,
    const CallStackRebuildPart  & part,
    bool                        & outIsLoaded)
{
    HRESULT                 hr       = S_OK;
    MachineHost           * machine  = m_scratch.GetMachine();
    Replayer              * replayer = m_scratch.GetReplayer();
    bool                    isSame   = false;
    bool                    isFresh  = false;
    Word                    pc       = 0;
    MachineHost::MediaIds   saved    = {};



    outIsLoaded = false;

    hr = ReadPartMedia (part, saved);
    CHR (hr);

    isSame = IsSavedWith (saved, job.disks);

    if (!isSame)
    {
        m_isRestartDue = true;
        BAIL_OUT_IF (true, S_OK);
    }

    isFresh = !m_isBegun || m_isRestartDue || part.isAfterGap;

    if (!isFresh)
    {
        SettleRecorder();
    }

    hr = m_scratch.MountDisks (job.disks);
    CHR (hr);

    hr = replayer->LoadFrom (m_state, part.startPosition, part.journalIndex);
    CHR (hr);

    if (isFresh)
    {
        pc = machine->GetCpu()->GetPC();

        m_recorder.Begin (pc, PeekByte (pc), (part.startCycle == 0) ? CallBreakKind::PowerOn : CallBreakKind::HistoryBegan);

        m_isBegun      = true;
        m_isRestartDue = false;
        m_recordFrom   = part.startPosition;
    }

    outIsLoaded = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::ReplayPart
//
//  To the part's end in chunks, so a newer job is noticed within one, and
//  the progress moves with each. A replay that ends anywhere but the part's
//  end went wrong.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchCallReplayer::ReplayPart (
    const CallStackRebuildJob   & job,
    const CallStackRebuildPart  & part)
{
    HRESULT        hr          = S_OK;
    MachineHost  * machine     = m_scratch.GetMachine();
    Replayer     * replayer    = m_scratch.GetReplayer();
    EmuCpu       * cpu         = machine->GetCpu();
    uint64_t       position    = machine->GetPosition();
    uint64_t       span        = (m_jobEnd > m_jobStart) ? m_jobEnd - m_jobStart : 0;
    bool           isAbandoned = false;
    ReplayTarget   target;
    ReplayReport   report;



    CBRA (cpu);

    while (position < part.endPosition)
    {
        isAbandoned = IsAbandoned (job);
        BAIL_OUT_IF (isAbandoned, E_ABORT);

        target.position = part.endPosition;
        target.cycle    = cpu->GetTotalCycles() + kChunkCycles;

        hr = replayer->RunTo (target, part.endPosition, this, report);
        CHR (hr);

        position = machine->GetPosition();

        if (span > 0)
        {
            m_progress.store ((float) (position - m_jobStart) / (float) span, std::memory_order_relaxed);
        }
    }

    CBREx (position == part.endPosition, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::Finish
//
//  The record as of where the machine stands, the instruction it holds
//  settled from the registers it left, so the record is whole there.
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::Finish (CallStackRebuildResult & outResult)
{
    MachineHost       * machine   = m_scratch.GetMachine();
    Cpu6502Registers    registers = machine->GetCpu()->GetCpu6502()->GetRegisters();



    SettleRecorder();

    outResult.position      = machine->GetPosition();
    outResult.cycle         = machine->GetCpu()->GetTotalCycles();
    outResult.registers     = registers;
    outResult.journalCursor = m_scratch.GetReplayer()->GetJournalCursor();
    outResult.record        = m_recorder.GetRecord();
    outResult.recordFrom    = m_recordFrom;

    m_progress.store (1.0f, std::memory_order_relaxed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::AttachRecorder
//
//  The recorder reads the machine as the debugger reads the running one:
//  without side effects, through a view of the CPU's address space. The
//  CPU reports the record's opcodes to it, the stack page is watched, and
//  the replayer reports each reset and power cycle it replays.
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::AttachRecorder()
{
    MachineHost  * machine  = m_scratch.GetMachine();
    Replayer     * replayer = m_scratch.GetReplayer();



    m_recorder.SetPeek  ([this] (Word address) { return PeekByte (address); });
    m_recorder.SetClock ([machine] { return machine->GetCpu()->GetTotalCycles(); });

    m_recorder.SetWriterLocator ([this, machine]
    {
        const EmuCpu  * cpu = machine->GetCpu();



        return CallStackRecorder::FindStoreInProgress (cpu->GetCpu6502()->GetInstructionSet(), cpu->GetPC(),
                                                       [this] (Word address) { return PeekByte (address); });
    });

    machine->SetOpcodeWatch (m_opcodes.data(), this);
    machine->GetMemoryBus().SetWatchedPage (kStackPage, true);

    replayer->SetResetCallback ([this] (bool isPowerCycle) { OnReplayedReset (isPowerCycle); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::DetachRecorder
//
//  The record itself is kept, for a job that continues.
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::DetachRecorder()
{
    MachineHost  * machine  = m_scratch.GetMachine();
    Replayer     * replayer = m_scratch.GetReplayer();



    if (machine == nullptr || replayer == nullptr)
    {
        return;
    }

    machine->SetOpcodeWatch (nullptr, nullptr);
    machine->GetMemoryBus().SetWatchedPage (kStackPage, false);

    replayer->SetResetCallback (nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::SettleRecorder
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::SettleRecorder()
{
    Cpu6502Registers  registers = m_scratch.GetMachine()->GetCpu()->GetCpu6502()->GetRegisters();



    m_recorder.Settle (registers.pc, registers.sp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::OnReplayedReset
//
//  A reset or power cycle in the replay reaches the record as one on the
//  running machine reaches the debugger's: every frame before it is void,
//  and a power cycle starts the cycle count the record dates from again.
//
////////////////////////////////////////////////////////////////////////////////

void ScratchCallReplayer::OnReplayedReset (bool isPowerCycle)
{
    Word  pc = m_scratch.GetMachine()->GetCpu()->GetPC();



    m_recorder.OnReset (pc, PeekByte (pc), isPowerCycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::PeekByte
//
//  A byte as the CPU sees it, or zero where it cannot be read without
//  disturbing the machine.
//
////////////////////////////////////////////////////////////////////////////////

Byte ScratchCallReplayer::PeekByte (Word address) const
{
    Byte  value  = 0;
    bool  isRead = false;



    if (m_view != nullptr)
    {
        isRead = m_view->TryPeek (address, value);
        IGNORE_RETURN_VALUE (isRead, false);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::UseQueue
//
//  The queue a test set, or a pool thread of its own, created on first use.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchCallReplayer::UseQueue()
{
    constexpr size_t  kCapacity = 2;
    HRESULT           hr        = S_OK;
    bool              isCreated = m_ownQueue.IsCreated();



    BAIL_OUT_IF (m_queue != nullptr, S_OK);

    if (!isCreated)
    {
        hr = m_ownQueue.Create (kCapacity, L"Casso call stack rebuild");
        CHR (hr);
    }

    m_queue = &m_ownQueue;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::IsAbandoned
//
////////////////////////////////////////////////////////////////////////////////

bool ScratchCallReplayer::IsAbandoned (const CallStackRebuildJob & job) const
{
    return m_latest.load (std::memory_order_acquire) != job.generation;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchCallReplayer::IsSavedWith
//
//  Whether a keyframe whose bays held saved holds the media the disks are,
//  bay for bay, and no others.
//
////////////////////////////////////////////////////////////////////////////////

bool ScratchCallReplayer::IsSavedWith (
    const MachineHost::MediaIds    & saved,
    const std::vector<ReplayDisk>  & disks)
{
    MachineHost::MediaIds  given = {};



    for (const ReplayDisk & disk : disks)
    {
        given[(size_t) disk.slot * DiskImageStore::kDriveCount + (size_t) disk.drive] = disk.mediaId;
    }

    return given == saved;
}






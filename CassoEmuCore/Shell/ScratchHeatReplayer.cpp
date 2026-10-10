#include "Pch.h"

#include "Shell/ScratchHeatReplayer.h"

#include "Debugger/MachineDebugTarget.h"
#include "Shell/MachineHost.h"
#include "Debugger/Reverse/Replayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::ScratchHeatReplayer
//
////////////////////////////////////////////////////////////////////////////////

ScratchHeatReplayer::ScratchHeatReplayer() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::~ScratchHeatReplayer
//
//  The job running is abandoned and waited for before the machine goes.
//
////////////////////////////////////////////////////////////////////////////////

ScratchHeatReplayer::~ScratchHeatReplayer()
{
    Cancel();
    WaitForWork();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::SetMachine
//
////////////////////////////////////////////////////////////////////////////////

void ScratchHeatReplayer::SetMachine (
    const MachineConfig  & config,
    const std::wstring   & name)
{
    m_scratch.SetMachine (config, name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::Submit
//
//  The job waits in the one slot, replacing any there, and the one running
//  is abandoned; a worker is started when none is running.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchHeatReplayer::Submit (std::shared_ptr<const HeatRebuildJob> job)
{
    HRESULT  hr      = S_OK;
    bool     isStart = false;



    CBRAEx (job, E_INVALIDARG);

    hr = UseQueue();
    CHR (hr);

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
//  ScratchHeatReplayer::TryTakeResult
//
////////////////////////////////////////////////////////////////////////////////

bool ScratchHeatReplayer::TryTakeResult (HeatRebuildResult & outResult)
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
//  ScratchHeatReplayer::Cancel
//
//  Nothing waiting is run, the job running stops at its next frame, and the
//  results not yet taken are dropped.
//
////////////////////////////////////////////////////////////////////////////////

void ScratchHeatReplayer::Cancel()
{
    std::lock_guard<std::mutex>  held (m_lock);



    m_next.reset();
    m_results.clear();
    m_latest.store (0, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::WaitForWork
//
////////////////////////////////////////////////////////////////////////////////

void ScratchHeatReplayer::WaitForWork()
{
    if (m_queue != nullptr)
    {
        m_queue->WaitAll();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::UseQueue
//
//  The queue a test set, or a pool thread of its own, created on first use.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchHeatReplayer::UseQueue()
{
    constexpr size_t  kCapacity = 2;
    HRESULT           hr        = S_OK;
    bool              isCreated = m_ownQueue.IsCreated();



    BAIL_OUT_IF (m_queue != nullptr, S_OK);

    if (!isCreated)
    {
        hr = m_ownQueue.Create (kCapacity, L"Casso heat map rebuild");
        CHR (hr);
    }

    m_queue = &m_ownQueue;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::FindAccess
//
//  The job's one part replayed on the calling thread, the access it found
//  taken from the result, and the map's tables let go.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchHeatReplayer::FindAccess (
    const HeatRebuildJob  & job,
    HeatRebuildResult     & outResult)
{
    HRESULT                         hr        = S_OK;
    bool                            isOnePart = job.parts.size() == 1;
    bool                            hasResult = false;
    std::vector<HeatRebuildResult>  results;



    outResult = HeatRebuildResult();

    CBRAEx (isOnePart && job.query.isSet, E_INVALIDARG);

    hr = Rebuild (job, results);
    CHR (hr);

    hasResult = !results.empty();
    CBRA (hasResult);

    outResult = std::move (results.front());

Error:
    m_map.Stop();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::RunJob
//
////////////////////////////////////////////////////////////////////////////////

void ScratchHeatReplayer::RunJob (void * context)
{
    static_cast<ScratchHeatReplayer *> (context)->RunPending();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::RunPending
//
//  Replays the job waiting, then any that arrived meanwhile, and stops when
//  none waits. Each part's result is kept, for the job still wanted only.
//
////////////////////////////////////////////////////////////////////////////////

void ScratchHeatReplayer::RunPending()
{
    std::shared_ptr<const HeatRebuildJob>  job;
    HRESULT                                hr = S_OK;



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

        hr = Run (*job, [this, &job] (HeatRebuildResult && result)
        {
            std::lock_guard<std::mutex>  held (m_lock);

            if (!IsAbandoned (*job))
            {
                m_results.push_back (std::move (result));
            }
        });

        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::Rebuild
//
//  On the calling thread, with no worker running: the job becomes the one
//  wanted, so nothing abandons it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchHeatReplayer::Rebuild (
    const HeatRebuildJob             & job,
    std::vector<HeatRebuildResult>   & outResults)
{
    outResults.clear();

    m_latest.store (job.generation, std::memory_order_release);

    return Run (job, [&outResults] (HeatRebuildResult && result) { outResults.push_back (std::move (result)); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::Run
//
//  The parts, newest first, each handed to onPart when done, with its job's
//  generation and the time it took whether or not its replay got through;
//  its hr says which. A part that fails ends the job, and is its last.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchHeatReplayer::Run (
    const HeatRebuildJob                                 & job,
    const std::function<void (HeatRebuildResult &&)>     & onPart)
{
    HRESULT                                hr    = S_OK;
    size_t                                 part  = 0;
    HeatRebuildResult                      result;
    std::chrono::steady_clock::time_point  began;



    for (part = 0; part < job.parts.size(); part++)
    {
        BAIL_OUT_IF (IsAbandoned (job), E_ABORT);

        began             = std::chrono::steady_clock::now();
        result            = HeatRebuildResult();
        result.generation = job.generation;
        result.part       = part;

        hr = RunPart (job, part, result);

        result.hr     = hr;
        result.isLast = FAILED (hr) || part + 1 == job.parts.size();
        result.ms     = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - began).count();

        onPart (std::move (result));

        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::RunPart
//
//  The first part brings the machine up to date, mounts the job's disks and
//  takes its inputs; every part then replays.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchHeatReplayer::RunPart (
    const HeatRebuildJob  & job,
    size_t                  part,
    HeatRebuildResult     & outResult)
{
    HRESULT  hr = S_OK;



    if (part == 0)
    {
        hr = m_scratch.Build();
        CHR (hr);

        hr = m_scratch.MountDisks (job.disks);
        CHR (hr);

        m_scratch.GetMachine()->GetInputJournal().LoadRecords (job.inputsFrom, job.inputs);
    }

    hr = Replay (job, job.parts[part], outResult);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::Replay
//
//  One part, from its keyframe to its end, a frame of machine time at a
//  time: each stop at the first instruction at or after a frame boundary
//  folds the map there, and the stop at the end folds it once more, so the
//  heat is what a map folded at every frame from the keyframe on would hold.
//  A replay that ends anywhere but the end's cycle went wrong.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchHeatReplayer::Replay (
    const HeatRebuildJob   & job,
    const HeatRebuildPart  & part,
    HeatRebuildResult      & outResult)
{
    HRESULT        hr          = S_OK;
    MachineHost  * machine     = m_scratch.GetMachine();
    Replayer     * replayer    = m_scratch.GetReplayer();
    EmuCpu       * cpu         = (machine != nullptr) ? machine->GetCpu() : nullptr;
    uint64_t       cycle       = 0;
    uint64_t       frameEnd    = 0;
    uint64_t       before      = 0;
    bool           isConnected = false;
    ReplayTarget   target;
    ReplayReport   report;



    CBRA (cpu);
    CBRA (replayer);

    hr = m_unpacker.Unpack (part.start, m_state);
    CHR (hr);

    hr = replayer->LoadFrom (m_state, part.startPosition, part.journalIndex);
    CHR (hr);

    m_map.Stop();
    m_map.SetPositionSource (machine->GetPositionPtr());
    m_map.SetFadeSeconds    (job.fadeSeconds);
    m_map.Start             (cpu->GetCpu6502()->GetInstructionSet(), cpu->GetTotalCycles());
    m_map.SetCountFrom      (job.countFrom);
    m_map.SetKeptBits    (part.kept);

    isConnected = MachineDebugTarget::TryConnectHeatMap (*machine, &m_map);
    CBRA (isConnected);

    before = replayer->GetReplayedCount();
    cycle  = cpu->GetTotalCycles();

    while (machine->GetPosition() < part.endPosition)
    {
        BAIL_OUT_IF (IsAbandoned (job), E_ABORT);

        frameEnd        = (cycle / AccessHeatMap::kCyclesPerFrame + 1) * AccessHeatMap::kCyclesPerFrame;
        target.position = part.endPosition;
        target.cycle    = frameEnd;

        hr = replayer->RunTo (target, part.endPosition, nullptr, report);
        CHR (hr);

        cycle = cpu->GetTotalCycles();

        if (cycle >= frameEnd)
        {
            m_map.Fold (cycle);
        }
    }

    m_map.Fold (cycle);

    CBREx (cycle == part.endCycle, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outResult.cycle        = cycle;
    outResult.instructions = replayer->GetReplayedCount() - before;

    if (job.query.isSet)
    {
        outResult.hasAccess = m_map.GetLastAccess (job.query.space, job.query.isWrite, job.query.address, outResult.access) == HeatAccessState::Found;
    }
    else
    {
        outResult.heat = m_map.GetHeatTable();
    }

Error:
    if (machine != nullptr)
    {
        isConnected = MachineDebugTarget::TryConnectHeatMap (*machine, nullptr);
        IGNORE_RETURN_VALUE (isConnected, false);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer::IsAbandoned
//
////////////////////////////////////////////////////////////////////////////////

bool ScratchHeatReplayer::IsAbandoned (const HeatRebuildJob & job) const
{
    return m_latest.load (std::memory_order_acquire) != job.generation;
}






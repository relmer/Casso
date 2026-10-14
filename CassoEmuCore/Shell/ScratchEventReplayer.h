#pragma once

#include "Pch.h"

#include "Core/IWorkQueue.h"
#include "Core/MachineConfig.h"
#include "Core/ThreadPoolWorkQueue.h"
#include "Debugger/Events/DebugEventJob.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"
#include "Debugger/Reverse/Replayer.h"
#include "Shell/MachineHost.h"
#include "Shell/ScratchReplayMachine.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchEventReplayer
//
//  Rebuilds a view's events by replaying history on a second machine of its
//  own (ScratchReplayMachine), with the view's probe attached, so the running
//  machine is never touched and the machine's thread never waits on it.
//  Each stretch of a job, newest first, is loaded from its keyframe over the
//  job's disks, mounted from their images and never written back, and
//  replayed to its end with the journal's inputs, as the heat map's rebuild
//  replays. A stretch whose keyframe was taken with other disks is not
//  replayed, and its result is marked unavailable. Each stretch's result is
//  queued for TryTakeResult as soon as it is done.
//
//  Jobs run one at a time on a pool thread of its own, or on a queue a test
//  sets with SetWorkQueue. A job submitted while another waits replaces it,
//  and one being replayed is abandoned within about kChunkCycles, so only
//  the newest finishes.
//
//  SetMachine is called on the thread that builds the running machine;
//  Submit, TryTakeResult and Cancel on the thread that runs it; the replay
//  on one worker at a time, or in Rebuild.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
class ScratchEventReplayer : public IDebugEventRunner<T>
{
public:
    //  Cycles replayed between checks for a newer job, about ten
    //  milliseconds of replay.
    static constexpr uint64_t  kChunkCycles = 1'000'000;

    explicit ScratchEventReplayer (std::unique_ptr<IDebugEventProbe<T>> probe)
        : m_probe (std::move (probe))
    {
    }

    ~ScratchEventReplayer() override
    {
        Cancel();
        WaitForWork();
    }

    ScratchEventReplayer             (const ScratchEventReplayer &) = delete;
    ScratchEventReplayer & operator= (const ScratchEventReplayer &) = delete;

    void  SetMachine   (const MachineConfig & config, const std::wstring & name) { m_scratch.SetMachine (config, name); }
    void  SetWorkQueue (IWorkQueue * queue)                                        { m_queue = queue; }

    void  WaitForWork()
    {
        if (m_queue != nullptr)
        {
            m_queue->WaitAll();
        }
    }

    // IDebugEventRunner: the job waits in the one slot, replacing any there,
    // and the one running is abandoned; a worker starts when none runs.
    HRESULT  Submit (std::shared_ptr<const DebugEventJob> job) override
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

    bool  TryTakeResult (DebugEventStretchResult<T> & outResult) override
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

    //  Nothing waiting runs, the job running stops at its next chunk, and the
    //  results not yet taken are dropped.
    void  Cancel() override
    {
        std::lock_guard<std::mutex>  held (m_lock);



        m_next.reset();
        m_results.clear();
        m_latest.store (0, std::memory_order_release);
    }

    //  The replay itself, on the calling thread, each stretch's events in
    //  turn. The job's generation becomes the latest, so it is not abandoned.
    HRESULT  Rebuild (const DebugEventJob & job, std::vector<DebugEventStretchResult<T>> & outResults)
    {
        outResults.clear();

        m_latest.store (job.generation, std::memory_order_release);

        return Run (job, [&outResults] (DebugEventStretchResult<T> && result) { outResults.push_back (std::move (result)); });
    }

private:
    static void  RunJob (void * context)
    {
        static_cast<ScratchEventReplayer *> (context)->RunPending();
    }

    //  Replays the job waiting, then any that arrived meanwhile, and stops
    //  when none waits. A stretch's result is queued only for the latest
    //  job.
    void  RunPending()
    {
        std::shared_ptr<const DebugEventJob>  job;
        HRESULT                               hr = S_OK;



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

            hr = Run (*job, [this, &job] (DebugEventStretchResult<T> && result)
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

    //  The queue a test set, or a pool thread of its own, created on first
    //  use.
    HRESULT  UseQueue()
    {
        constexpr size_t  kCapacity = 2;
        HRESULT           hr        = S_OK;



        BAIL_OUT_IF (m_queue != nullptr, S_OK);

        if (!m_ownQueue.IsCreated())
        {
            hr = m_ownQueue.Create (kCapacity, L"Casso debug event rebuild");
            CHR (hr);
        }

        m_queue = &m_ownQueue;

    Error:
        return hr;
    }

    //  The stretches, newest first, each passed to onStretch when done. An
    //  abandoned job passes nothing more; a stretch that fails is passed with
    //  its hr and ends the job.
    HRESULT  Run (const DebugEventJob & job, const std::function<void (DebugEventStretchResult<T> &&)> & onStretch)
    {
        HRESULT                     hr = S_OK;
        DebugEventStretchResult<T>  result;



        hr = m_scratch.Build();
        CHR (hr);

        m_scratch.GetMachine()->GetInputJournal().LoadRecords (job.inputsFrom, job.inputs);

        for (const DebugEventStretch & stretch : job.stretches)
        {
            BAIL_OUT_IF (IsAbandoned (job), E_ABORT);

            result               = DebugEventStretchResult<T>();
            result.generation    = job.generation;
            result.startPosition = stretch.startPosition;
            result.endPosition   = stretch.endPosition;

            hr = RunStretch (job, stretch, result);

            BAIL_OUT_IF (hr == E_ABORT, E_ABORT);

            result.hr = hr;
            onStretch (std::move (result));

            CHR (hr);
        }

    Error:
        return hr;
    }

    //  From the stretch's keyframe, loaded over the job's disks when it was
    //  saved with them, to its end, with the probe attached.
    HRESULT  RunStretch (const DebugEventJob & job, const DebugEventStretch & stretch, DebugEventStretchResult<T> & outResult)
    {
        HRESULT                 hr         = S_OK;
        MachineHost           * machine    = m_scratch.GetMachine();
        Replayer              * replayer   = m_scratch.GetReplayer();
        EmuCpu                * cpu        = (machine != nullptr) ? machine->GetCpu() : nullptr;
        bool                    isAttached = false;
        bool                    isAtEnd    = false;
        MachineHost::MediaIds   saved      = {};
        ReplayTarget            target;
        ReplayReport            report;



        CBRA (cpu);
        CBRA (replayer);

        hr = m_unpacker.Unpack (stretch.start, m_state);
        CHR (hr);

        hr = MachineHost::ReadSavedMedia (m_state, saved);
        CHR (hr);

        if (!ScratchReplayMachine::IsSavedWith (saved, job.disks))
        {
            outResult.isUnavailable = true;
            BAIL_OUT_IF (true, S_OK);
        }

        hr = m_scratch.MountDisks (job.disks);
        CHR (hr);

        hr = replayer->LoadFrom (m_state, stretch.startPosition, stretch.journalIndex);
        CHR (hr);

        hr = m_probe->Attach (*machine, outResult.entries);
        CHR (hr);

        isAttached = true;

        while (machine->GetPosition() < stretch.endPosition)
        {
            BAIL_OUT_IF (IsAbandoned (job), E_ABORT);

            target.position = stretch.endPosition;
            target.cycle    = cpu->GetTotalCycles() + kChunkCycles;

            hr = replayer->RunTo (target, stretch.endPosition, m_probe->GetStopTest(), report);
            CHR (hr);
        }

        //  A replay that ends anywhere but the stretch's end went wrong; one
        //  before a gap has no end cycle to check.
        isAtEnd = machine->GetPosition() == stretch.endPosition && (stretch.endCycle == 0 || cpu->GetTotalCycles() == stretch.endCycle);
        CBREx (isAtEnd, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    Error:
        if (isAttached)
        {
            m_probe->Detach (*machine);
        }

        return hr;
    }

    bool  IsAbandoned (const DebugEventJob & job) const
    {
        return m_latest.load (std::memory_order_acquire) != job.generation;
    }

    //  The jobs, between the threads.
    std::mutex                                     m_lock;
    std::shared_ptr<const DebugEventJob>           m_next;
    std::deque<DebugEventStretchResult<T>>         m_results;
    bool                                           m_isRunning = false;
    std::atomic<uint64_t>                          m_latest    = 0;      // the latest job's generation, 0 for none

    ThreadPoolWorkQueue                            m_ownQueue;
    IWorkQueue                                   * m_queue     = nullptr;

    //  The worker's own.
    std::unique_ptr<IDebugEventProbe<T>>           m_probe;
    ScratchReplayMachine                           m_scratch;
    KeyframeUnpacker                               m_unpacker;
    std::vector<Byte>                              m_state;
};

#include "Pch.h"

#include "Shell/ProfileHistory.h"

#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MakeProbe
//
//  The probe for the runner to own, and the same probe for the history to
//  set the period on and read the progress from.
//
////////////////////////////////////////////////////////////////////////////////

static std::unique_ptr<IDebugEventProbe<ProfileCountsPtr>> MakeProbe (ProfileEventProbe *& outProbe)
{
    auto  probe = std::make_unique<ProfileEventProbe>();



    outProbe = probe.get();
    return probe;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::ProfileHistory
//
//  Blocks are replayed on half the processors, up to kMaxWorkers, at once.
//
////////////////////////////////////////////////////////////////////////////////

ProfileHistory::ProfileHistory (MachineHost & machine, DebugEventHistory & history) :
    m_machine (machine),
    m_history (history)
{
    size_t  count = std::clamp<size_t> (std::thread::hardware_concurrency() / 2, 1, kMaxWorkers);



    for (size_t i = 0; i < count; i++)
    {
        Worker  worker;

        worker.runner = std::make_unique<ScratchEventReplayer<ProfileCountsPtr>> (MakeProbe (worker.probe));
        m_workers.push_back (std::move (worker));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::~ProfileHistory
//
////////////////////////////////////////////////////////////////////////////////

ProfileHistory::~ProfileHistory()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::Start
//
//  A new machine's history has none of the blocks cached for the old one.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileHistory::Start (ReverseController * controller)
{
    Stop();

    m_controller = controller;
    m_cached.clear();

    for (Worker & worker : m_workers)
    {
        worker.runner->SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::Stop
//
////////////////////////////////////////////////////////////////////////////////

void ProfileHistory::Stop()
{
    CancelAll();
    m_remaining  = 0;
    m_isBuilding = false;
    m_blockCounts.clear();
    m_controller = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::CancelAll
//
////////////////////////////////////////////////////////////////////////////////

void ProfileHistory::CancelAll()
{
    for (Worker & worker : m_workers)
    {
        worker.runner->Cancel();
        worker.generation = 0;
        worker.blocks.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::SetWorkQueue
//
////////////////////////////////////////////////////////////////////////////////

void ProfileHistory::SetWorkQueue (IWorkQueue * queue)
{
    for (Worker & worker : m_workers)
    {
        worker.runner->SetWorkQueue (queue);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::WaitForWork
//
////////////////////////////////////////////////////////////////////////////////

void ProfileHistory::WaitForWork()
{
    for (Worker & worker : m_workers)
    {
        worker.runner->WaitForWork();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::IsStopped
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileHistory::IsStopped() const
{
    return !m_isStopped || m_isStopped();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::GetBounds
//
//  History's stretches from the keyframe at or before fromCycle to the
//  machine's position, the one it is in cut there, joined wherever no gap
//  divides them and the second does not start at a boundary keyframe, whose
//  state does not follow from the one before it; then the stretch since the
//  newest keyframe, unless the machine runs live, so it grows, or recording
//  is paused, so it ends where history has no record.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileHistory::GetBounds (uint64_t fromCycle, std::vector<DebugEventBounds> & outBounds) const
{
    uint64_t                         position  = m_machine.GetPosition();
    const KeyframeStore            * keyframes = (m_controller != nullptr) ? &m_controller->GetKeyframes() : nullptr;
    size_t                           count     = (keyframes != nullptr && m_controller->IsRecording()) ? keyframes->GetCount() : 0;
    size_t                           first     = 0;
    uint64_t                         start     = 0;
    uint64_t                         newest    = 0;
    std::vector<DebugEventBounds>    stretches;
    auto                             isBoundary = [keyframes] (uint64_t at)
    {
        size_t  index = 0;



        return keyframes->TryFindByPosition (at, index) && keyframes->GetInfo (index).position == at && keyframes->GetInfo (index).isBoundary;
    };



    outBounds.clear();

    if (count == 0)
    {
        return;
    }

    start = keyframes->TryFindAtOrBefore (fromCycle, first) ? keyframes->GetInfo (first).position : keyframes->GetInfo (0).position;

    m_history.GetStretches (stretches);

    for (DebugEventBounds bounds : stretches)
    {
        if (bounds.endPosition <= start)
        {
            continue;
        }

        if (bounds.startPosition >= position)
        {
            break;
        }

        if (bounds.endPosition > position)
        {
            bounds.endPosition = position;
            bounds.hasGapAfter = false;
        }

        if (!outBounds.empty() && !outBounds.back().hasGapAfter && outBounds.back().endPosition == bounds.startPosition && !isBoundary (bounds.startPosition))
        {
            outBounds.back().endPosition = bounds.endPosition;
            outBounds.back().hasGapAfter = bounds.hasGapAfter;
        }
        else
        {
            outBounds.push_back (bounds);
        }
    }

    newest = keyframes->GetInfo (count - 1).position;

    if (newest < position && newest >= start && !(m_controller->IsRecordingLive() && !IsStopped()) && !m_controller->IsPaused())
    {
        if (!outBounds.empty() && !outBounds.back().hasGapAfter && outBounds.back().endPosition == newest && !isBoundary (newest))
        {
            outBounds.back().endPosition = position;
        }
        else
        {
            outBounds.push_back ({ newest, position, false });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::GetBlocks
//
//  Each stretch cut at the keyframes where the grid of m_blockPositions
//  instructions turns over. A block is cached for later builds when it ends
//  at a keyframe and starts at or after fromCycle, so no part of it was
//  left out of the period.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileHistory::GetBlocks (
    uint64_t                  fromCycle,
    std::vector<Block>      & outBlocks,
    std::optional<uint64_t>   toCycle) const
{
    const KeyframeStore            * keyframes = (m_controller != nullptr) ? &m_controller->GetKeyframes() : nullptr;
    std::vector<DebugEventBounds>    spans;
    size_t                           index     = 0;
    auto                             cycleAt   = [&] (uint64_t position) -> std::optional<uint64_t>
    {
        size_t  at = 0;



        return keyframes->TryFindByPosition (position, at) && keyframes->GetInfo (at).position == position
                   ? std::optional<uint64_t> (keyframes->GetInfo (at).cycle) : std::nullopt;
    };



    outBlocks.clear();
    GetBounds (fromCycle, spans);

    for (const DebugEventBounds & span : spans)
    {
        Block  block;

        if (keyframes == nullptr || !keyframes->TryFindByPosition (span.startPosition, index))
        {
            continue;
        }

        block.bounds        = { span.startPosition, span.endPosition, span.hasGapAfter };
        block.startChecksum = keyframes->GetInfo (index).checksum;
        block.isCacheable        = keyframes->GetInfo (index).cycle >= fromCycle;

        for (size_t next = index + 1; next < keyframes->GetCount() && keyframes->GetInfo (next).position < span.endPosition; next++)
        {
            const KeyframeInfo  & info = keyframes->GetInfo (next);

            if (info.position / m_blockPositions == block.bounds.startPosition / m_blockPositions)
            {
                continue;
            }

            block.bounds.endPosition = info.position;
            block.bounds.hasGapAfter = false;
            block.endChecksum        = info.checksum;
            outBlocks.push_back (block);

            block               = Block();
            block.bounds        = { info.position, span.endPosition, span.hasGapAfter };
            block.startChecksum = info.checksum;
            block.isCacheable        = true;
        }

        block.endChecksum = 0;
        block.isCacheable      = block.isCacheable && keyframes->TryFindByPosition (span.endPosition, index) && !span.hasGapAfter;
        block.endChecksum = block.isCacheable ? keyframes->GetInfo (index).checksum : 0;
        outBlocks.push_back (block);
    }

    if (!toCycle.has_value() || keyframes == nullptr)
    {
        return;
    }

    std::erase_if (outBlocks, [&] (const Block & block) { return cycleAt (block.bounds.startPosition).value_or (0) >= *toCycle; });

    for (Block & block : outBlocks)
    {
        block.isCacheable = block.isCacheable && cycleAt (block.bounds.endPosition).value_or (UINT64_MAX) <= *toCycle;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::GetKey
//
////////////////////////////////////////////////////////////////////////////////

ProfileHistory::BlockKey ProfileHistory::GetKey (const Block & block)
{
    return { block.bounds.startPosition, block.bounds.endPosition, block.startChecksum, block.endChecksum };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::Build
//
//  The blocks cached from an earlier build are used as they are; the rest
//  are replayed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProfileHistory::Build (
    uint64_t                  fromCycle,
    std::string             & error,
    std::optional<uint64_t>   toCycle)
{
    HRESULT              hr       = S_OK;
    uint64_t             endCycle = toCycle.value_or ((m_machine.GetCpu() != nullptr) ? m_machine.GetCpu()->GetHistoryCycles() : 0);
    std::vector<size_t>  pending;
    bool                 isAny    = false;



    GetBlocks (fromCycle, m_blocks, toCycle);
    isAny = !m_blocks.empty();
    CBRF (isAny, error = "There is no history to profile.");

    CancelAll();
    WaitForWork();

    m_blockCounts.assign (m_blocks.size(), nullptr);
    m_blockAt.clear();
    m_totalSpan  = 0;
    m_doneSpan   = 0;
    m_reused     = 0;
    m_isBuilding = true;

    for (size_t i = 0; i < m_blocks.size(); i++)
    {
        auto      cached = m_blocks[i].isCacheable ? m_cached.find (GetKey (m_blocks[i])) : m_cached.end();
        uint64_t  span   = m_blocks[i].bounds.endPosition - m_blocks[i].bounds.startPosition;

        m_blockAt[m_blocks[i].bounds.startPosition] = i;
        m_totalSpan += span;

        if (cached != m_cached.end())
        {
            m_blockCounts[i]  = cached->second;
            m_doneSpan       += span;
            m_reused++;
        }
        else
        {
            pending.push_back (i);
        }
    }

    m_remaining = pending.size();

    hr = SubmitBlocks (pending, fromCycle, endCycle);

    //  A run that could not start cancels the runs that did, so the profile
    //  is not left building with nothing to finish it.
    if (FAILED (hr))
    {
        CancelAll();
        m_isBuilding = false;
        m_remaining  = 0;
        m_blockCounts.clear();
    }

    CHRF (hr, error = "The profile could not be started.");

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::SubmitBlocks
//
//  The blocks to replay, in as many runs of consecutive blocks as there are
//  workers, each run of about the same length; each run's job has its
//  blocks newest first, with the journal's inputs over just that run.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProfileHistory::SubmitBlocks (const std::vector<size_t> & pending, uint64_t fromCycle, uint64_t toCycle)
{
    HRESULT   hr      = S_OK;
    size_t    runs    = std::min (m_workers.size(), pending.size());
    uint64_t  total   = 0;
    uint64_t  counted = 0;
    size_t    next    = 0;



    for (size_t i : pending)
    {
        total += m_blocks[i].bounds.endPosition - m_blocks[i].bounds.startPosition;
    }

    for (size_t run = 0; run < runs; run++)
    {
        std::vector<DebugEventBounds>  bounds;
        DebugEventJob                  job;
        uint64_t                       share = total * (run + 1) / runs;

        while (next < pending.size() && (bounds.empty() || counted < share || run + 1 == runs))
        {
            const Block  & block = m_blocks[pending[next++]];

            bounds.push_back (block.bounds);
            counted += block.bounds.endPosition - block.bounds.startPosition;
        }

        if (bounds.empty())
        {
            continue;
        }

        std::ranges::reverse (bounds);

        hr = m_history.BuildJob (bounds, job);
        CHR (hr);

        for (const DebugEventStretch & stretch : job.stretches)
        {
            m_workers[run].blocks.insert (m_blockAt[stretch.startPosition]);
        }

        m_workers[run].probe->Begin (fromCycle, toCycle);
        m_workers[run].generation = job.generation;

        hr = m_workers[run].runner->Submit (std::make_shared<const DebugEventJob> (std::move (job)));
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::Pump
//
//  Each block's result has that block's counts, which are cached when
//  the block is cacheable; once the last is in, the blocks' counts are added up in
//  order, oldest first. A block that could not be replayed adds nothing,
//  and since a failed stretch ends its job, neither do the job's blocks
//  after it.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileHistory::Pump (ProfileCountsPtr & outCounts)
{
    DebugEventStretchResult<ProfileCountsPtr>  result;
    std::shared_ptr<ProfileCounts>             sum;



    for (Worker & worker : m_workers)
    {
        while (m_remaining > 0 && worker.runner->TryTakeResult (result))
        {
            auto  found = m_blockAt.find (result.startPosition);

            if (result.generation != worker.generation || found == m_blockAt.end())
            {
                continue;
            }

            if (!result.entries.empty() && SUCCEEDED (result.hr) && !result.isUnavailable)
            {
                m_blockCounts[found->second] = result.entries.back().event;

                if (m_blocks[found->second].isCacheable && m_cached.size() < kMaxCached)
                {
                    m_cached[GetKey (m_blocks[found->second])] = result.entries.back().event;
                }
            }

            worker.blocks.erase (found->second);
            m_doneSpan += m_blocks[found->second].bounds.endPosition - m_blocks[found->second].bounds.startPosition;
            m_remaining--;

            if (FAILED (result.hr))
            {
                for (size_t block : worker.blocks)
                {
                    m_doneSpan += m_blocks[block].bounds.endPosition - m_blocks[block].bounds.startPosition;
                    m_remaining--;
                }

                worker.blocks.clear();
            }
        }
    }

    if (!m_isBuilding || m_remaining > 0)
    {
        return false;
    }

    sum = std::make_shared<ProfileCounts>();
    sum->routines.SetFramesCoverAll (true);

    for (const ProfileCountsPtr & counts : m_blockCounts)
    {
        if (counts != nullptr)
        {
            sum->table.Add       (counts->table);
            sum->routines.Merge  (counts->routines);
        }
    }

    m_isBuilding = false;
    m_blockCounts.clear();
    outCounts = std::move (sum);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory::GetProgress
//
//  The blocks done by their share of the period; with one block to replay,
//  how far its replay is.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<double> ProfileHistory::GetProgress() const
{
    if (!m_isBuilding)
    {
        return std::nullopt;
    }

    if (m_remaining == 1 && m_reused == 0 && m_blocks.size() == 1)
    {
        return m_workers.front().probe->GetProgress();
    }

    return (m_totalSpan == 0) ? 0.0 : (double) m_doneSpan / (double) m_totalSpan;
}

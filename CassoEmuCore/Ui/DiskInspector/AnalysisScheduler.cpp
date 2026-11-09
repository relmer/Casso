#include "Pch.h"

#include "Ui/DiskInspector/AnalysisScheduler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AnalysisScheduler::AnalysisScheduler
//
////////////////////////////////////////////////////////////////////////////////

AnalysisScheduler::AnalysisScheduler (Notify notify) :
    m_notify (std::move (notify))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AnalysisScheduler::~AnalysisScheduler
//
//  Jobs still queued return at once, so closing the window does not wait for
//  the rest of the disk; the queue then joins its thread.
//
////////////////////////////////////////////////////////////////////////////////

AnalysisScheduler::~AnalysisScheduler()
{
    m_isStopping = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AnalysisScheduler::Restart
//
//  A new disk, or new decode settings: every request and result so far is
//  dropped and every record of the copy is requested.
//
////////////////////////////////////////////////////////////////////////////////

void AnalysisScheduler::Restart (std::shared_ptr<const DiskCopy> copy, const DecodeSettings & settings)
{
    std::scoped_lock  lock (m_lock);
    int               slot = 0;



    m_mediaId  = copy->mediaId;
    m_settings = settings;
    m_pending.clear();
    m_wanted.clear();
    m_results.clear();

    for (slot = 0; slot < static_cast<int> (copy->tracks.size()); slot++)
    {
        RequestLocked (copy, slot);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AnalysisScheduler::Update
//
//  A newer copy of the given records, as after the guest writes. A copy of
//  another disk is ignored; Restart switches disks.
//
////////////////////////////////////////////////////////////////////////////////

void AnalysisScheduler::Update (std::shared_ptr<const DiskCopy> copy, std::span<const int> slots)
{
    std::scoped_lock  lock (m_lock);



    if (copy->mediaId == m_mediaId)
    {
        for (int slot : slots)
        {
            RequestLocked (copy, slot);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AnalysisScheduler::IsPending
//
//  True while a record is waiting or being analyzed, which the window shows
//  as "Analyzing".
//
////////////////////////////////////////////////////////////////////////////////

bool AnalysisScheduler::IsPending (int slot) const
{
    std::scoped_lock  lock (m_lock);



    return m_running == slot || m_pending.contains (slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AnalysisScheduler::HasPending
//
////////////////////////////////////////////////////////////////////////////////

bool AnalysisScheduler::HasPending() const
{
    std::scoped_lock  lock (m_lock);



    return m_running >= 0 || !m_pending.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AnalysisScheduler::TakeResults
//
//  The results finished since the last call, oldest first.
//
////////////////////////////////////////////////////////////////////////////////

void AnalysisScheduler::TakeResults (vector<RecordResult> & outResults)
{
    std::scoped_lock  lock (m_lock);



    outResults.clear();
    outResults.swap (m_results);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AnalysisScheduler::RequestLocked
//
//  Replaces any pending request for the record and posts a job. Jobs take the
//  lowest pending record when they run, so a replaced request costs one job
//  that finds nothing to do.
//
////////////////////////////////////////////////////////////////////////////////

void AnalysisScheduler::RequestLocked (std::shared_ptr<const DiskCopy> copy, int slot)
{
    m_generation++;
    m_pending[slot] = Request { std::move (copy), m_generation };
    m_wanted[slot]  = m_generation;

    m_queue.Post ([this] () { RunOne(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  AnalysisScheduler::RunOne
//
//  Analyzes the lowest pending record outside the lock, then keeps the result
//  only if no newer request for that record, and no other disk, came in
//  meanwhile.
//
////////////////////////////////////////////////////////////////////////////////

void AnalysisScheduler::RunOne()
{
    Request         request;
    DecodeSettings  settings;
    RecordResult    result;
    bool            hasWork  = false;
    bool            isKept   = false;



    if (!m_isStopping)
    {
        std::scoped_lock  lock (m_lock);

        if (!m_pending.empty())
        {
            result.slot    = m_pending.begin()->first;
            request        = std::move (m_pending.begin()->second);
            settings       = m_settings;
            m_running      = result.slot;
            hasWork        = true;
            m_pending.erase (m_pending.begin());
        }
    }

    if (hasWork)
    {
        result.mediaId  = request.copy->mediaId;
        result.analysis = DiskAnalyzer::AnalyzeRecord (*request.copy, result.slot, settings);
        result.copy     = request.copy;

        {
            std::scoped_lock  lock (m_lock);

            m_running = -1;
            isKept    = !m_isStopping && result.mediaId == m_mediaId && m_wanted[result.slot] == request.generation;

            if (isKept)
            {
                m_results.push_back (std::move (result));
            }
        }

        if (isKept && m_notify)
        {
            m_notify();
        }
    }
}

#include "Pch.h"

#include "TreeChildChecks.h"

#include "Core/ThreadName.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TreeChildChecks::~TreeChildChecks
//
////////////////////////////////////////////////////////////////////////////////

TreeChildChecks::~TreeChildChecks()
{
    {
        std::lock_guard<std::mutex>  guard (m_state->lock);

        m_state->stopping = true;
        m_state->queue.clear();
    }

    m_state->queued.notify_all();

    if (m_worker.joinable())
    {
        m_worker.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeChildChecks::SetTarget
//
//  The worker starts with the first window to post to.
//
////////////////////////////////////////////////////////////////////////////////

void TreeChildChecks::SetTarget (HWND hwnd, UINT message)
{
    {
        std::lock_guard<std::mutex>  guard (m_state->lock);

        m_state->hwnd    = hwnd;
        m_state->message = message;
    }

    if (hwnd != nullptr && !m_worker.joinable())
    {
        m_worker = std::thread (RunWorker, m_state);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeChildChecks::TryGet
//
////////////////////////////////////////////////////////////////////////////////

bool TreeChildChecks::TryGet (const std::wstring & path, int64_t modifiedUnix, bool & outHasChildren)
{
    auto  stored = m_stored.find (path);



    outHasChildren = false;

    if (stored != m_stored.end() && stored->second.modifiedUnix == modifiedUnix)
    {
        outHasChildren = stored->second.hasChildren;
        return true;
    }

    if (!m_manual && !m_worker.joinable())
    {
        outHasChildren = m_state->checker ? m_state->checker (path) : false;
        m_state->checkCount++;
        m_stored[path] = Stored { modifiedUnix, outHasChildren };
        return true;
    }

    {
        std::lock_guard<std::mutex>  guard (m_state->lock);
        auto                         pending = m_state->pending.find (path);

        if (pending != m_state->pending.end() && pending->second == modifiedUnix)
        {
            return false;
        }

        m_state->pending[path] = modifiedUnix;
        m_state->queue.push_back (Request { path, modifiedUnix, m_state->generation });
    }

    m_state->queued.notify_one();
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeChildChecks::TakeResults
//
////////////////////////////////////////////////////////////////////////////////

std::vector<TreeChildChecks::Result> TreeChildChecks::TakeResults()
{
    std::vector<Finished>  finished;
    std::vector<Result>    results;
    uint64_t               generation = 0;



    {
        std::lock_guard<std::mutex>  guard (m_state->lock);

        finished.swap (m_state->finished);
        generation = m_state->generation;
    }

    for (const Finished & done : finished)
    {
        if (done.request.generation != generation)
        {
            continue;
        }

        m_stored[done.request.path] = Stored { done.request.modifiedUnix, done.hasChildren };
        results.push_back (Result { done.request.path, done.hasChildren });
    }

    return results;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeChildChecks::RunQueued
//
////////////////////////////////////////////////////////////////////////////////

void TreeChildChecks::RunQueued()
{
    std::deque<Request>  queue;



    {
        std::lock_guard<std::mutex>  guard (m_state->lock);

        queue.swap (m_state->queue);
    }

    for (const Request & request : queue)
    {
        RunOne (*m_state, request);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeChildChecks::Clear
//
////////////////////////////////////////////////////////////////////////////////

void TreeChildChecks::Clear()
{
    std::lock_guard<std::mutex>  guard (m_state->lock);



    m_stored.clear();
    m_state->queue.clear();
    m_state->pending.clear();
    m_state->finished.clear();
    m_state->generation++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeChildChecks::RunWorker
//
////////////////////////////////////////////////////////////////////////////////

void TreeChildChecks::RunWorker (std::shared_ptr<State> state)
{
    HRESULT  hrName = ThreadName::SetForCurrentThread (L"Tree child checks");
    Request  request;



    IGNORE_RETURN_VALUE (hrName, S_OK);

    for (;;)
    {
        {
            std::unique_lock<std::mutex>  guard (state->lock);

            state->queued.wait (guard, [&state]() { return state->stopping || !state->queue.empty(); });

            if (state->stopping)
            {
                break;
            }

            request = std::move (state->queue.front());
            state->queue.pop_front();
        }

        RunOne (*state, request);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeChildChecks::RunOne
//
////////////////////////////////////////////////////////////////////////////////

void TreeChildChecks::RunOne (State & state, const Request & request)
{
    bool  hasChildren = state.checker ? state.checker (request.path) : false;
    HWND  hwnd        = nullptr;
    UINT  message     = 0;



    state.checkCount++;

    {
        std::lock_guard<std::mutex>  guard (state.lock);
        auto                         pending = state.pending.find (request.path);

        if (pending != state.pending.end() && pending->second == request.modifiedUnix)
        {
            state.pending.erase (pending);
        }

        if (request.generation == state.generation)
        {
            state.finished.push_back (Finished { request, hasChildren });
        }

        hwnd    = state.hwnd;
        message = state.message;
    }

    if (hwnd != nullptr)
    {
        PostMessageW (hwnd, message, 0, 0);
    }
}

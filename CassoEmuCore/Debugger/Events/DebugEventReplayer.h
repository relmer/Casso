#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventCache.h"
#include "Debugger/Events/DebugEventJob.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventReplayer
//
//  A view's events for any span of history: from the event cache where it
//  is complete, and before that from stretches of history replayed by a
//  runner with the view's probe (FR-210 to FR-212). A request queues the
//  stretches the span overlaps that are not rebuilt yet, newest first, then
//  kPrefetch more before them, as one job; a request for other stretches
//  replaces the job waiting. Rebuilt stretches go in a small LRU, so
//  scrolling back and forth replays nothing twice.
//
//  A stretch whose keyframe was taken with other disks in the bays cannot
//  be replayed; its span has an Unavailable entry at its start. A stretch
//  that ends at a gap in recording is followed by a Gap entry where
//  recording resumed.
//
//  Used on the thread that runs the machine.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
class DebugEventReplayer
{
public:
    using Entry = DebugEventEntry<T>;

    static constexpr size_t  kLruStretches = 64;
    static constexpr size_t  kPrefetch     = 2;

    DebugEventReplayer (const DebugEventCache<T> & cache, IDebugEventHistory & history, IDebugEventRunner<T> & runner)
        : m_cache   (cache),
          m_history (history),
          m_runner  (runner)
    {
    }

    //  The span [from, to) of a view. The stretches it overlaps that are not
    //  rebuilt or on their way are queued, newest first, with the prefetch.
    HRESULT  Request (uint64_t from, uint64_t to)
    {
        HRESULT                        hr     = S_OK;
        std::vector<DebugEventBounds>  wanted;
        DebugEventJob                  job;



        m_history.GetStretches (m_stretches);

        FindWanted (from, to, wanted);
        BAIL_OUT_IF (wanted.empty() || IsPending (wanted), S_OK);

        hr = m_history.BuildJob (wanted, job);
        CHR (hr);

        m_pending    = std::move (wanted);
        m_generation = job.generation;

        hr = m_runner.Submit (std::make_shared<const DebugEventJob> (std::move (job)));
        CHR (hr);

    Error:
        return hr;
    }

    //  Takes the stretches the runner finished into the LRU. True when any
    //  came in.
    bool  Pump()
    {
        DebugEventStretchResult<T>  result;
        bool                        isAny = false;



        while (m_runner.TryTakeResult (result))
        {
            if (result.generation != m_generation)
            {
                continue;
            }

            Remember (std::move (result));
            isAny = true;
        }

        return isAny;
    }

    //  The entries of [from, to), oldest first: rebuilt stretches before
    //  where the cache is complete, then the cache. False until every
    //  stretch the span overlaps is rebuilt.
    bool  GetSpan (uint64_t from, uint64_t to, std::vector<Entry> & out)
    {
        uint64_t  covered    = m_cache.GetCoveredFrom();
        bool      isComplete = true;
        size_t    i          = 0;



        out.clear();

        for (i = 0; i < m_stretches.size(); i++)
        {
            const DebugEventBounds  & bounds  = m_stretches[i];
            Rebuilt                 * rebuilt = nullptr;

            if (!IsNeeded (bounds, from, std::min (to, covered)))
            {
                continue;
            }

            rebuilt = Find (bounds);

            if (rebuilt == nullptr)
            {
                isComplete = false;
                continue;
            }

            rebuilt->lastUse = ++m_useClock;

            AppendRebuilt (*rebuilt, from, std::min (to, covered), out);

            //  Recording resumed where the next stretch begins; a gap within
            //  the cache's span is an entry in the cache.
            if (bounds.hasGapAfter && i + 1 < m_stretches.size() && IsWithin (m_stretches[i + 1].startPosition, from, std::min (to, covered)))
            {
                out.push_back (Entry { DebugEventEntryKind::Gap, m_stretches[i + 1].startPosition, 0, T {} });
            }
        }

        m_cache.CopySpan (std::max (from, covered), to, out);

        return isComplete;
    }

    //  History from position on is gone, after a change made in the past:
    //  the stretches rebuilt there go, and so does the job.
    void  OnHistoryCut (uint64_t position)
    {
        std::erase_if (m_rebuilt, [position] (const Rebuilt & rebuilt) { return rebuilt.result.endPosition > position; });

        m_pending.clear();
        m_generation = 0;
        m_runner.Cancel();
    }

    size_t  GetRebuiltCount () const { return m_rebuilt.size(); }

private:
    struct Rebuilt
    {
        DebugEventStretchResult<T>  result;
        uint64_t                    lastUse = 0;
    };

    static bool  IsWithin (uint64_t position, uint64_t from, uint64_t to)
    {
        return position >= from && position < to;
    }

    //  A stretch overlapping [from, to).
    static bool  IsNeeded (const DebugEventBounds & bounds, uint64_t from, uint64_t to)
    {
        return bounds.startPosition < to && bounds.endPosition > from;
    }

    //  The stretches the span overlaps that are not rebuilt, newest first,
    //  then those of the kPrefetch before the oldest of them that are not
    //  rebuilt.
    void  FindWanted (uint64_t from, uint64_t to, std::vector<DebugEventBounds> & outWanted)
    {
        uint64_t  end    = std::min (to, m_cache.GetCoveredFrom());
        size_t    oldest = m_stretches.size();
        size_t    i      = 0;
        size_t    taken  = 0;



        outWanted.clear();

        for (i = m_stretches.size(); i > 0; i--)
        {
            if (!IsNeeded (m_stretches[i - 1], from, end))
            {
                continue;
            }

            oldest = i - 1;

            if (Find (m_stretches[i - 1]) == nullptr)
            {
                outWanted.push_back (m_stretches[i - 1]);
            }
        }

        for (i = oldest; i > 0 && taken < kPrefetch && oldest < m_stretches.size(); i--)
        {
            if (Find (m_stretches[i - 1]) == nullptr)
            {
                outWanted.push_back (m_stretches[i - 1]);
            }

            taken++;
        }
    }

    //  Whether the job on its way has every stretch given.
    bool  IsPending (const std::vector<DebugEventBounds> & wanted) const
    {
        return std::ranges::all_of (wanted, [this] (const DebugEventBounds & bounds)
        {
            return std::ranges::find (m_pending, bounds) != m_pending.end();
        });
    }

    Rebuilt *  Find (const DebugEventBounds & bounds)
    {
        auto  it = std::ranges::find_if (m_rebuilt, [&bounds] (const Rebuilt & rebuilt)
        {
            return rebuilt.result.startPosition == bounds.startPosition && rebuilt.result.endPosition == bounds.endPosition;
        });

        return (it != m_rebuilt.end()) ? &*it : nullptr;
    }

    //  The least recently used goes when the LRU is full.
    void  Remember (DebugEventStretchResult<T> && result)
    {
        auto  oldest = m_rebuilt.begin();



        std::erase_if (m_pending, [&result] (const DebugEventBounds & bounds) { return bounds.startPosition == result.startPosition; });

        if (m_rebuilt.size() >= kLruStretches)
        {
            oldest = std::ranges::min_element (m_rebuilt, {}, &Rebuilt::lastUse);
            m_rebuilt.erase (oldest);
        }

        m_rebuilt.push_back (Rebuilt { std::move (result), ++m_useClock });
    }

    //  A stretch that failed or could not be replayed is one Unavailable
    //  entry at its start.
    static void  AppendRebuilt (const Rebuilt & rebuilt, uint64_t from, uint64_t to, std::vector<Entry> & out)
    {
        if (rebuilt.result.isUnavailable || FAILED (rebuilt.result.hr))
        {
            if (IsWithin (rebuilt.result.startPosition, from, to))
            {
                out.push_back (Entry { DebugEventEntryKind::Unavailable, rebuilt.result.startPosition, 0, T {} });
            }

            return;
        }

        for (const Entry & entry : rebuilt.result.entries)
        {
            if (IsWithin (entry.position, from, to))
            {
                out.push_back (entry);
            }
        }
    }

    const DebugEventCache<T>       & m_cache;
    IDebugEventHistory             & m_history;
    IDebugEventRunner<T>           & m_runner;
    std::vector<DebugEventBounds>    m_stretches;
    std::vector<DebugEventBounds>    m_pending;
    std::vector<Rebuilt>             m_rebuilt;
    uint64_t                         m_generation = 0;
    uint64_t                         m_useClock   = 0;
};

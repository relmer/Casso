#pragma once

#include "Pch.h"



////////////////////////////////////////////////////////////////////////////////
//
//  PerfStats
//
//  Tiny rolling-average timer registry. Scoped RAII helpers stuff
//  per-label deltas into a process-wide singleton; debug overlays / log
//  taps read back the smoothed numbers.
//
//  Threading: the rolling-average map is guarded by a mutex. The hot path
//  is one map lookup + one atomic-style replacement; not free, but cheap
//  enough to keep in the per-frame UI composite path. The renderer-thread
//  consumer reads under the same lock.
//
////////////////////////////////////////////////////////////////////////////////

class PerfStats
{
public:

    struct Stat
    {
        double  avgMs   = 0.0;
        double  lastMs  = 0.0;
        double  maxMs   = 0.0;
    };

    static PerfStats &                     Instance ();

    void                                   Record   (const char * label, double ms);
    Stat                                   Get      (const char * label) const;
    std::unordered_map<std::string, Stat>  GetAll   () const;

private:

    PerfStats() = default;

    mutable std::mutex                      m_mutex;
    std::unordered_map<std::string, Stat>   m_stats;
};



////////////////////////////////////////////////////////////////////////////////
//
//  ScopedPerfTimer
//
//  RAII wrapper around QueryPerformanceCounter. Records the elapsed ms
//  into PerfStats::Instance() on destruction.
//
////////////////////////////////////////////////////////////////////////////////

class ScopedPerfTimer
{
public:

    explicit ScopedPerfTimer (const char * label) noexcept
        : m_label (label)
    {
        QueryPerformanceCounter (&m_start);
    }

    ~ScopedPerfTimer ()
    {
        LARGE_INTEGER  end;
        LARGE_INTEGER  freq;

        QueryPerformanceCounter   (&end);
        QueryPerformanceFrequency (&freq);

        double  ms = (double) (end.QuadPart - m_start.QuadPart) * 1000.0
                   / (double) freq.QuadPart;

        PerfStats::Instance().Record (m_label, ms);
    }

    ScopedPerfTimer (const ScopedPerfTimer &) = delete;
    ScopedPerfTimer & operator= (const ScopedPerfTimer &) = delete;

private:

    const char    * m_label = nullptr;
    LARGE_INTEGER   m_start = {};
};

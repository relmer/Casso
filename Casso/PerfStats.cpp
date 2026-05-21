#include "Pch.h"

#include "PerfStats.h"



////////////////////////////////////////////////////////////////////////////////
//
//  PerfStats::Instance
//
////////////////////////////////////////////////////////////////////////////////

PerfStats & PerfStats::Instance()
{
    static PerfStats  s_inst;
    return s_inst;
}



////////////////////////////////////////////////////////////////////////////////
//
//  PerfStats::Record
//
////////////////////////////////////////////////////////////////////////////////

void PerfStats::Record (const char * label, double ms)
{
    constexpr double         kAlpha = 0.1;
    std::lock_guard<std::mutex>  lock (m_mutex);

    if (label == nullptr)
    {
        return;
    }

    Stat &  s = m_stats[label];

    s.lastMs = ms;
    s.avgMs  = (s.avgMs == 0.0) ? ms : (s.avgMs * (1.0 - kAlpha) + ms * kAlpha);
    if (ms > s.maxMs)
    {
        s.maxMs = ms;
    }
}



////////////////////////////////////////////////////////////////////////////////
//
//  PerfStats::Get
//
////////////////////////////////////////////////////////////////////////////////

PerfStats::Stat PerfStats::Get (const char * label) const
{
    std::lock_guard<std::mutex>  lock (m_mutex);

    if (label == nullptr)
    {
        return Stat {};
    }

    auto  it = m_stats.find (label);
    if (it == m_stats.end())
    {
        return Stat {};
    }
    return it->second;
}



////////////////////////////////////////////////////////////////////////////////
//
//  PerfStats::GetAll
//
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, PerfStats::Stat> PerfStats::GetAll () const
{
    std::lock_guard<std::mutex>  lock (m_mutex);
    return m_stats;
}

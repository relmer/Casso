#include "Pch.h"

#include "Ui/Debugger/StopChanges.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StopChanges::Update
//
////////////////////////////////////////////////////////////////////////////////

void StopChanges::Update (bool isPaused, Values values)
{
    m_isPaused = isPaused;

    if (isPaused && values != m_stop)
    {
        m_previous = std::move (m_stop);
        m_stop     = std::move (values);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopChanges::IsChanged
//
//  A value the previous stop did not show at all is new, not changed.
//
////////////////////////////////////////////////////////////////////////////////

bool StopChanges::IsChanged (const std::string & key) const
{
    auto  now = m_stop.find (key);
    auto  was = m_previous.find (key);



    return m_isPaused && now != m_stop.end() && was != m_previous.end() && now->second != was->second;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopChanges::GetDelta
//
//  Both stops' values read as whole numbers; a value that does not read as
//  one has no delta.
//
////////////////////////////////////////////////////////////////////////////////

std::string StopChanges::GetDelta (const std::string & key) const
{
    auto     now      = m_stop.find (key);
    auto     was      = m_previous.find (key);
    int64_t  nowValue = 0;
    int64_t  wasValue = 0;
    int64_t  delta    = 0;



    if (!m_isPaused || now == m_stop.end() || was == m_previous.end())
    {
        return std::string();
    }

    if (std::from_chars (now->second.data(), now->second.data() + now->second.size(), nowValue).ec != std::errc() ||
        std::from_chars (was->second.data(), was->second.data() + was->second.size(), wasValue).ec != std::errc())
    {
        return std::string();
    }

    delta = nowValue - wasValue;

    if (delta == 0)
    {
        return std::string();
    }

    return std::format ("{:+}", delta);
}





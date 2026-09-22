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

#include "Pch.h"

#include "Debugger/Project/ProjectSwitchGuard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSwitchGuard::TryAllow
//
//  A scope outside the paused ones ends the pause, since the signals have moved
//  on from the projects that were switching back and forth.
//
////////////////////////////////////////////////////////////////////////////////

bool ProjectSwitchGuard::TryAllow (const ProjectScope & scope, uint64_t nowMs)
{
    if (m_isHeld && std::ranges::find (m_held, scope) != m_held.end())
    {
        return false;
    }

    if (m_isHeld)
    {
        m_isHeld = false;
        m_held.clear();
        m_switches.clear();
    }

    while (!m_switches.empty() && nowMs - m_switches.front().first > kWindowMs)
    {
        m_switches.pop_front();
    }

    if (m_switches.size() >= kMaxSwitches)
    {
        m_isHeld = true;
        m_held.push_back (scope);

        for (const auto & [at, each] : m_switches)
        {
            m_held.push_back (each);
        }

        return false;
    }

    m_switches.emplace_back (nowMs, scope);
    return true;
}

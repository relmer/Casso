#pragma once

#include "Pch.h"

#include "Debugger/Project/ProjectScope.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSwitchGuard
//
//  Whether the debugger may switch to the project a scope chooses. Two
//  projects that each load what makes the other's scope the choice would
//  switch back and forth every service tick, the window redrawn each time.
//  After kMaxSwitches switches within kWindowMs, switching is paused: no
//  scope among them is switched to again until a scope outside them is
//  chosen.
//
////////////////////////////////////////////////////////////////////////////////

class ProjectSwitchGuard
{
public:
    static constexpr uint64_t  kWindowMs    = 2000;
    static constexpr size_t    kMaxSwitches = 4;

    //  True, with the switch counted, when a switch to `scope` may go ahead.
    bool  TryAllow (const ProjectScope & scope, uint64_t nowMs);
    bool  IsHeld   () const { return m_isHeld; }

private:
    std::deque<std::pair<uint64_t, ProjectScope>>  m_switches;
    std::vector<ProjectScope>                      m_held;
    bool                                           m_isHeld = false;
};

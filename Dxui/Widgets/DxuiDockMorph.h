#pragma once

#include "Pch.h"
#include "Core/DxuiRectMorph.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockMorph
//
//  The rect a dock site draws as a pane is pinned or unpinned: it shrinks
//  from where the pane was into its edge tab, or grows from the edge tab to
//  the pane's group. Time comes from GetTickCount64 unless SetClock replaces
//  it, and the system's animation setting applies unless SetAnimationsEnabled
//  overrides it. Visual only: the layout is changed before a morph starts,
//  and no state depends on it ending.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockMorph
{
public:
    using ClockFn = std::function<int64_t ()>;

    void  SetClock             (ClockFn fn) { m_clock = std::move (fn); }
    void  SetAnimationsEnabled (bool on)    { m_animations = on; }

    void  Start   (const RECT & from, const RECT & to);
    bool  IsOn    () const;
    RECT  GetRect () const;
    bool  Tick    ();

private:
    DxuiRectMorph        m_morph;
    bool                 m_isOn       = false;
    std::optional<bool>  m_animations;
    ClockFn              m_clock      = [] { return (int64_t) GetTickCount64(); };
};

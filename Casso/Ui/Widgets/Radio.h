#pragma once

#include "Pch.h"

#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Radio  +  RadioGroup
//
//  Pure-logic mutually-exclusive option set. Each `Radio` is a small
//  immutable record (rect + label) owned by the surrounding
//  `RadioGroup`; the group tracks which index is currently selected
//  and routes mouse / keyboard / focus state to the active option.
//
//  Keyboard contract on the focused group:
//      Left  / Up   -> select previous (wraps)
//      Right / Down -> select next     (wraps)
//      Space        -> no-op (already selected)
//      Enter        -> commit (no-op when no change handler)
//
////////////////////////////////////////////////////////////////////////////////

struct RadioOption
{
    RECT          rect = {};
    std::wstring  label;
};


class RadioGroup
{
public:
    using ChangeFn = std::function<void (int newIndex)>;

    void  SetOptions (std::vector<RadioOption> options) { m_options = std::move (options); }
    void  SetSelected (int index);
    void  SetEnabled  (bool enabled) { m_enabled = enabled; }
    void  SetFocused  (bool focused) { m_focused = focused; }
    void  SetOnChange (ChangeFn fn)  { m_change = std::move (fn); }

    const std::vector<RadioOption> & Options    () const { return m_options;  }
    int                              Selected   () const { return m_selected; }
    bool                             Enabled    () const { return m_enabled;  }
    bool                             Focused    () const { return m_focused;  }
    int                              HoverIndex () const { return m_hover;    }

    int   HitTest        (int x, int y) const;
    void  SetMouseHover  (int x, int y);
    bool  OnLButtonDown  (int x, int y);
    bool  OnLButtonUp    (int x, int y);
    bool  OnKey          (WPARAM vk);
    void  Paint          (DxUiPainter & painter, DwriteTextRenderer & text) const;

private:
    void  Commit (int newIndex);


    std::vector<RadioOption>  m_options;
    ChangeFn                  m_change;
    int                       m_selected   = -1;
    int                       m_hover      = -1;
    int                       m_pressedIdx = -1;
    bool                      m_enabled    = true;
    bool                      m_focused    = false;
};

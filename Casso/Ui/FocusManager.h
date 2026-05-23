#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FocusManager
//
//  Owns the tab-order list for the current focus context. Widgets
//  register a focusable id when they want to be reachable by keyboard
//  navigation; `FocusManager` handles Tab / Shift+Tab advance, Esc
//  dismissal, and Enter / Space activation. Visible focus cues are
//  rendered by the consumer via the registered callback.
//
////////////////////////////////////////////////////////////////////////////////

enum class FocusKey
{
    None       = 0,
    Tab        = 1,
    ShiftTab   = 2,
    Enter      = 3,
    Space      = 4,
    Escape     = 5,
};


class FocusManager
{
public:
    using ActivateFn = std::function<void (int focusId)>;
    using DismissFn  = std::function<void ()>;


    FocusManager  () = default;
    ~FocusManager () = default;

    void   Clear             ();
    void   RegisterFocusable (int focusId);
    int    Current           () const;
    int    Count             () const { return (int) m_order.size(); }

    bool   SetFocus          (int focusId);
    bool   AdvanceForward    ();
    bool   AdvanceBackward   ();

    bool   HandleKey         (FocusKey key);

    void   SetActivateHandler (ActivateFn fn) { m_activate = std::move (fn); }
    void   SetDismissHandler  (DismissFn  fn) { m_dismiss  = std::move (fn); }

    static FocusKey  ClassifyKey      (UINT vkCode, bool shiftHeld);

private:
    std::vector<int>  m_order;
    int               m_currentIndex = -1;
    ActivateFn        m_activate;
    DismissFn         m_dismiss;
};

#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UiInput
//
//  Translates raw Win32 messages into a typed `UiEvent` stream that the
//  rest of the native UI runtime consumes. Maintains modifier state
//  (Shift / Ctrl / Alt) and the last known mouse position so consumers
//  don't have to re-derive them from each event.
//
////////////////////////////////////////////////////////////////////////////////

enum class UiEventType
{
    None             = 0,
    MouseMove        = 1,
    MouseDown        = 2,
    MouseUp          = 3,
    MouseWheel       = 4,
    KeyDown          = 5,
    KeyUp            = 6,
    Char             = 7,
    FocusGained      = 8,
    FocusLost        = 9,
};


enum class UiMouseButton
{
    None    = 0,
    Left    = 1,
    Middle  = 2,
    Right   = 3,
};


struct UiModifierState
{
    bool  shift  = false;
    bool  ctrl   = false;
    bool  alt    = false;
};


struct UiEvent
{
    UiEventType      type     = UiEventType::None;
    UiMouseButton    button   = UiMouseButton::None;
    int              x        = 0;
    int              y        = 0;
    int              wheel    = 0;
    UINT             keyCode  = 0;
    wchar_t          ch       = 0;
    UiModifierState  mods;
};


class UiInput
{
public:
    UiInput  () = default;
    ~UiInput () = default;

    bool                   Translate          (UINT msg, WPARAM wParam, LPARAM lParam, UiEvent & outEvent);
    void                   PushEvent          (const UiEvent & ev);
    bool                   PopEvent           (UiEvent & outEvent);
    const UiModifierState & Modifiers         () const { return m_mods; }
    int                    MouseX             () const { return m_mouseX; }
    int                    MouseY             () const { return m_mouseY; }
    void                   Clear              ();

private:
    void                   RefreshModifiers   ();


    std::deque<UiEvent>  m_queue;
    UiModifierState      m_mods;
    int                  m_mouseX = 0;
    int                  m_mouseY = 0;
};

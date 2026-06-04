#pragma once

////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInput
//
//  Translates raw Win32 messages into a typed `DxuiEvent` stream that the
//  rest of the native UI runtime consumes. Maintains modifier state
//  (Shift / Ctrl / Alt) and the last known mouse position so consumers
//  don't have to re-derive them from each event.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiEventType
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


enum class DxuiMouseButton
{
    None    = 0,
    Left    = 1,
    Middle  = 2,
    Right   = 3,
};


struct DxuiModifierState
{
    bool  shift  = false;
    bool  ctrl   = false;
    bool  alt    = false;
};


struct DxuiEvent
{
    DxuiEventType      type     = DxuiEventType::None;
    DxuiMouseButton    button   = DxuiMouseButton::None;
    int              x        = 0;
    int              y        = 0;
    int              wheel    = 0;
    UINT             keyCode  = 0;
    wchar_t          ch       = 0;
    DxuiModifierState  mods;
};


class DxuiInput
{
public:
    DxuiInput  () = default;
    ~DxuiInput() = default;

    bool                   Translate          (UINT msg, WPARAM wParam, LPARAM lParam, DxuiEvent & outEvent);
    void                   PushEvent          (const DxuiEvent & ev);
    bool                   PopEvent           (DxuiEvent & outEvent);
    const DxuiModifierState & Modifiers         () const { return m_mods; }
    int                    MouseX             () const { return m_mouseX; }
    int                    MouseY             () const { return m_mouseY; }
    void                   Clear              ();

private:
    void                   RefreshModifiers   ();


    std::deque<DxuiEvent>  m_queue;
    DxuiModifierState      m_mods;
    int                  m_mouseX = 0;
    int                  m_mouseY = 0;
};

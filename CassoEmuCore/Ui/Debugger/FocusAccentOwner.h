#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  FocusAccentOwner
//
//  Which of the debugger's windows, the main one or a floating pane's, shows
//  the focus accent: the one that took the keyboard focus last. The main
//  window taking the focus takes the accent back from a float. A window
//  losing the focus, to another application or to another of these windows,
//  changes nothing, since the window taking it reports that itself. A float
//  that docks gives the accent up.
//
////////////////////////////////////////////////////////////////////////////////

class FocusAccentOwner
{
public:
    void  OnMainFocusChanged  (bool focused);
    void  OnFloatFocusChanged (const std::wstring & pane, bool focused);
    void  OnFloatDocked       (const std::wstring & pane);

    //  The floating pane whose window shows the accent; empty while the main
    //  window shows it.
    const std::wstring &  GetFocusedFloat() const { return m_focusedFloat; }

private:
    std::wstring  m_focusedFloat;
};

#pragma once

#include "Pch.h"
#include "Window.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugConsole
//
//  In-app debug log window (opened via Ctrl+D).
//
////////////////////////////////////////////////////////////////////////////////

class DebugConsole : public Window
{
public:
    DebugConsole ();
    ~DebugConsole ();

    void Show (HINSTANCE hInstance);
    void Hide ();
    bool IsVisible () const { return m_visible; }

    void Log (const wstring & message);
    void LogConfig (const string & summary);

protected:
    // Window message handler overrides
    LRESULT OnCreate  (HWND hwnd, CREATESTRUCT * pcs) override;
    bool    OnClose   (HWND hwnd) override;
    bool    OnSize    (HWND hwnd, UINT width, UINT height) override;

private:
    bool    m_visible  = false;
    HWND    m_editCtrl = nullptr;
};






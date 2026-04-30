#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugConsole
//
//  In-app debug log window (opened via Ctrl+D).
//
////////////////////////////////////////////////////////////////////////////////

class DebugConsole
{
public:
    DebugConsole ();
    ~DebugConsole ();

    void Show (HINSTANCE hInstance);
    void Hide ();
    bool IsVisible () const { return m_visible; }

    void Log (const std::wstring & message);
    void LogConfig (const std::string & summary);

private:
    static LRESULT CALLBACK WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool    m_visible;
    HWND    m_hwnd;
    HWND    m_editCtrl;
};

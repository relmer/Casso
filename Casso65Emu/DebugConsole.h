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

    bool Show (HINSTANCE hInstance);
    void Hide ();
    bool IsVisible () const;

    void Log (const wstring & message);
    void LogConfig (const string & summary);

protected:
    LRESULT OnCreate  (HWND hwnd, CREATESTRUCT * pcs) override;
    bool    OnClose   (HWND hwnd) override;
    bool    OnSize    (HWND hwnd, UINT width, UINT height) override;

private:
    HRESULT InitializeConsole (HINSTANCE hInstance);

    HWND    m_editCtrl = nullptr;
};






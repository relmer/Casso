#pragma once

#include "Pch.h"
#include "Window/DxuiDialogWindow.h"


class DxuiPanel;




////////////////////////////////////////////////////////////////////////////////
//
//  MessageDialog
//
//  Simple message / task dialog: a pre-built content panel (wrapped body
//  runs + optional icon, via DialogBodyContent) above a right-aligned row
//  of action buttons, shown modally through DxuiWindow::ShowDialog. This
//  is the DxuiWindow-based replacement for the DxuiDialog + DxuiDialog-
//  Manager path that EmulatorShell::ShowModalDialog used.
//
//  Result mapping: each button carries a caller-defined resultCode. The
//  window is driven by Win32 command ids under the hood -- command buttons
//  get synthetic ids offset from IDOK / IDCANCEL, the isCancel button (if
//  any) gets IDCANCEL so Escape / the close-box fire it -- and
//  TranslateResult() maps a ShowDialog() return back to the resultCode (or
//  the closeBoxResult for a bare close).
//
////////////////////////////////////////////////////////////////////////////////

class MessageDialog : public DxuiDialogWindow
{
public:
    struct Button
    {
        std::wstring  label;
        int           resultCode = 0;
        bool          isDefault  = false;
        bool          isCancel   = false;
    };


    //
    //  Configure before Create(): the pre-built content panel, the action
    //  buttons (left-to-right in order), and the result returned on a bare
    //  close-box / Escape when no cancel button is present.
    //
    void  Configure (std::unique_ptr<DxuiPanel>  content,
                     std::vector<Button>         buttons,
                     int                         closeBoxResult);

    //
    //  Command id for ShowDialog(): the default button's, or 0 (no default).
    //
    int   DefaultCommandId () const { return m_defaultCommandId; }

    //
    //  Translate a ShowDialog() return code into the configured button's
    //  resultCode (or the closeBoxResult / cancel button's resultCode).
    //
    int   TranslateResult (int dialogResult) const;


protected:
    void  OnCreate () override;


private:
    static constexpr int  s_kCommandBase = 1000;   // synthetic command-id base (avoids IDOK/IDCANCEL)

    static int  CommandIdFor (const Button & button, int index);


    std::unique_ptr<DxuiPanel>  m_pendingContent;
    std::vector<Button>         m_buttons;
    int                         m_closeBoxResult   = -1;
    int                         m_defaultCommandId = 0;
};

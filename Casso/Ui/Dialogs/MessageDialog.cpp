#include "Pch.h"

#include "MessageDialog.h"

#include "Core/DxuiPanel.h"




////////////////////////////////////////////////////////////////////////////////
//
//  MessageDialog::CommandIdFor
//
//  The cancel button maps to IDCANCEL (so Escape / the close-box fire it);
//  every other button gets a synthetic id offset past IDOK / IDCANCEL so
//  it never collides with the window-level cancel result.
//
////////////////////////////////////////////////////////////////////////////////

int MessageDialog::CommandIdFor (const Button & button, int index)
{
    return button.isCancel ? IDCANCEL : (s_kCommandBase + index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MessageDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void MessageDialog::Configure (std::unique_ptr<DxuiPanel>  content,
                               std::vector<Button>         buttons,
                               int                         closeBoxResult)
{
    int  i = 0;


    m_pendingContent = std::move (content);
    m_buttons        = std::move (buttons);
    m_closeBoxResult = closeBoxResult;

    for (i = 0; i < (int) m_buttons.size(); ++i)
    {
        if (m_buttons[(size_t) i].isDefault)
        {
            m_defaultCommandId = CommandIdFor (m_buttons[(size_t) i], i);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MessageDialog::OnCreate
//
//  Installs the pre-built content and one command button per configured
//  Button. Command buttons auto-close via EndDialog(id); the isCancel
//  button doubles as the Escape / close-box target (IDCANCEL).
//
////////////////////////////////////////////////////////////////////////////////

void MessageDialog::OnCreate ()
{
    int  i = 0;


    if (m_pendingContent != nullptr)
    {
        SetDialogContentOwned (std::move (m_pendingContent));
    }

    for (i = 0; i < (int) m_buttons.size(); ++i)
    {
        AddDialogButton (m_buttons[(size_t) i].label, CommandIdFor (m_buttons[(size_t) i], i));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MessageDialog::TranslateResult
//
////////////////////////////////////////////////////////////////////////////////

int MessageDialog::TranslateResult (int dialogResult) const
{
    int     result = m_closeBoxResult;
    size_t  idx    = 0;


    if (dialogResult >= s_kCommandBase && dialogResult < s_kCommandBase + (int) m_buttons.size())
    {
        result = m_buttons[(size_t) (dialogResult - s_kCommandBase)].resultCode;
    }
    else
    {
        for (idx = 0; idx < m_buttons.size(); ++idx)
        {
            if (m_buttons[idx].isCancel)
            {
                result = m_buttons[idx].resultCode;
                break;
            }
        }
    }

    return result;
}

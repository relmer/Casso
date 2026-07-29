#include "Pch.h"

#include "PickerDialog.h"

#include "Core/DxuiPanel.h"




////////////////////////////////////////////////////////////////////////////////
//
//  ConfigurePicker
//
////////////////////////////////////////////////////////////////////////////////

void PickerDialog::ConfigurePicker (std::unique_ptr<DxuiPanel>          content,
                                    IDxuiControl *                     initialFocus,
                                    const std::vector<DialogButton> &  buttons,
                                    int                                closeBoxResult)
{
    m_pendingContent = std::move (content);
    m_pendingFocus   = initialFocus;
    m_buttons        = buttons;
    m_closeBoxResult = closeBoxResult;
}




////////////////////////////////////////////////////////////////////////////////
//
//  MapResult
//
////////////////////////////////////////////////////////////////////////////////

int PickerDialog::MapResult (int dialogResult) const
{
    int     result = m_closeBoxResult;
    size_t  idx    = 0;


    if (dialogResult >= kRowResultBase)
    {
        result = dialogResult - kRowResultBase;
    }
    else if (dialogResult == IDCANCEL)
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
    else
    {
        result = dialogResult;
    }

    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void PickerDialog::OnCreate()
{
    size_t  i = 0;


    if (m_pendingContent != nullptr)
    {
        SetDialogContentOwned (std::move (m_pendingContent));
    }

    for (i = 0; i < m_buttons.size(); ++i)
    {
        int                    commandId = m_buttons[i].isCancel ? IDCANCEL : m_buttons[i].resultCode;
        DxuiButtonRow::Anchor  anchor    = m_buttons[i].anchorLeft ? DxuiButtonRow::Anchor::Left
                                                                   : DxuiButtonRow::Anchor::Right;

        AddDialogButton (m_buttons[i].label, commandId, anchor);

        if (m_buttons[i].isDefault)
        {
            m_defaultCommandId = commandId;
        }
    }

    SetInitialFocus (m_pendingFocus);
}

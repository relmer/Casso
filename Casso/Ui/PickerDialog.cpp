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
        for (auto & button : m_buttons)
        {
            if (button.isCancel)
            {
                result = button.resultCode;
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

    for (auto & button : m_buttons)
    {
        int                    commandId = button.isCancel ? IDCANCEL : button.resultCode;
        DxuiButtonRow::Anchor  anchor    = button.anchorLeft ? DxuiButtonRow::Anchor::Left
                                                                   : DxuiButtonRow::Anchor::Right;

        AddDialogButton (button.label, commandId, anchor);

        if (button.isDefault)
        {
            m_defaultCommandId = commandId;
        }
    }

    SetInitialFocus (m_pendingFocus);
}

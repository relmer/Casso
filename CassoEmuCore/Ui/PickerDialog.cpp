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
//  Translates a raw dialog result into the caller's result vocabulary.
//
//  Row selections are encoded as an index ADDED to a base far above any button
//  id, so a click on the list and a click on a button can share one result
//  channel without colliding. This is where that encoding is undone.
//
//  IDCANCEL arrives from Escape and from the caption close box as well as from
//  a Cancel button, so it is mapped to whichever button was declared the
//  cancel one -- letting the caller define what dismissal MEANS instead of
//  matching a Win32 constant.
//
//  With no cancel button declared, the configured close-box result is used, so
//  a dialog that dismisses to something other than cancel still gets a
//  sensible answer.
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
//  Realizes the declaratively-configured dialog once its window exists.
//
//  Content and buttons are DECLARED before the dialog is shown and applied
//  here, because neither can be added until there is a window to add them to.
//  That split is what lets a caller build a picker in one expression and show
//  it, rather than interleaving configuration with window lifetime.
//
//  Cancel buttons are registered under IDCANCEL rather than their own result
//  code, so Escape and the caption close box route through the same command as
//  the button and MapResult can translate all three identically.
//
//  The default button is recorded as it is added, so Enter activates it
//  without a second declaration.
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

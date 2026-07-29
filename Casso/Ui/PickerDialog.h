#pragma once

#include "Pch.h"
#include "Window/DxuiDialogWindow.h"

#include "Dialogs/DialogDefinition.h"


class DxuiPanel;
class IDxuiControl;




////////////////////////////////////////////////////////////////////////////////
//
//  PickerDialog
//
//  DxuiDialogWindow hosting a pre-built picker body (search + list) and its
//  action buttons. Non-cancel buttons carry their real (negative) result
//  codes as command ids, so a click ends the modal with that code directly;
//  the cancel button maps to IDCANCEL so Escape / the close-box fire it.
//  Row activation ends the modal with the row result offset past
//  kRowResultBase; MapResult un-offsets it and translates IDCANCEL back to
//  the cancel button's real code (or the close-box result when no cancel
//  button exists).
//
////////////////////////////////////////////////////////////////////////////////

class PickerDialog : public DxuiDialogWindow
{
public:
    // Row results are offset past the button / IDCANCEL codes so the two
    // ranges can never collide.
    static constexpr int  kRowResultBase = 100000;

    void  ConfigurePicker (std::unique_ptr<DxuiPanel>          content,
                           IDxuiControl *                     initialFocus,
                           const std::vector<DialogButton> &  buttons,
                           int                                closeBoxResult);

    int   DefaultCommandId() const { return m_defaultCommandId; }

    // Translate a raw modal result into the caller's result code: un-offset
    // a row activation, map IDCANCEL onto the cancel button's real code, and
    // fall back to the close-box result when there is no cancel button.
    int   MapResult (int dialogResult) const;

protected:
    void  OnCreate() override;

private:
    std::unique_ptr<DxuiPanel>  m_pendingContent;
    IDxuiControl *              m_pendingFocus     = nullptr;
    std::vector<DialogButton>   m_buttons;
    int                         m_closeBoxResult   = -1;
    int                         m_defaultCommandId = 0;
};

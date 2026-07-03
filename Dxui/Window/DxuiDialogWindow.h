#pragma once

#include "Pch.h"
#include "Window/DxuiWindow.h"


class DxuiButton;
class DxuiPanel;
class DxuiDpiScaler;




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogWindow
//
//  Small shared base for the common dialog shape: a single fill content
//  panel above a bottom, right-aligned row of action buttons, hosted in a
//  DxuiWindow whose caption the host owns. Subclasses build their content
//  (a message body, a picker list, a progress panel, ...) plus their
//  buttons in OnCreate(), then the consumer calls ShowDialog(defaultId).
//
//  This is the "message / task dialog" helper -- it is NOT a DxuiDialog
//  revival: a dialog IS a DxuiWindow, and this base only factors out the
//  content + button-row geometry every simple dialog repeats. Paged
//  Settings uses the separate DxuiPropertySheet.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDialogWindow : public DxuiWindow
{
public:
    DxuiDialogWindow  () = default;
    ~DxuiDialogWindow () override = default;


protected:
    //
    //  Create the single fill content panel as a child of this window and
    //  record it for layout (below the host caption, above the button row,
    //  inset by a standard pad). Call once from OnCreate(), then configure
    //  the returned panel. Mirrors DxuiPanel::CreateChild.
    //
    template <typename T, typename... Args>
    T *  CreateDialogContent (Args &&... args)
    {
        T *  content = this->template CreateChild<T> (std::forward<Args> (args)...);

        m_content = content;
        return content;
    }

    //
    //  Install a pre-built content panel (constructed before Create(),
    //  e.g. using external resources): the base retains ownership and
    //  adopts it into the tree for layout / paint / input. Call once from
    //  OnCreate().
    //
    void  SetDialogContentOwned (std::unique_ptr<DxuiPanel> content);

    //
    //  Append a right-aligned action button carrying a Win32 command id
    //  (IDOK / IDCANCEL / IDYES / ...). In a modal ShowDialog the base
    //  auto-closes via EndDialog(commandId) unless the caller sets a
    //  custom SetOnClick (a button that must keep the dialog open, e.g.
    //  Download). Buttons are laid left-to-right in registration order,
    //  right-aligned as a group. Returns the live button.
    //
    DxuiButton *  AddDialogButton (const std::wstring & label, int commandId);

    DxuiPanel  *  DialogContent   () const { return m_content; }

    //
    //  Geometry: content fills the client (inset by pad, above the button
    //  row); the buttons sit in a fixed-height bottom strip, right-aligned.
    //
    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;


private:
    DxuiPanel *                m_content = nullptr;
    std::unique_ptr<DxuiPanel> m_contentOwned;
    std::vector<DxuiButton *>  m_dialogButtons;
};

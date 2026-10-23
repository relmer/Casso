#pragma once

#include "Pch.h"
#include "Window/DxuiWindow.h"
#include "Window/DxuiButtonRow.h"


class DxuiButton;
class DxuiPanel;
class DxuiDpiScaler;
class IDxuiTextRenderer;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiMeasuredContent
//
//  A dialog's content that measures its own height at a given width, with
//  the window's renderer and theme. DxuiDialogWindow sets its window's
//  height from that measurement before the window appears, rather than from
//  an estimate made before there was a renderer to measure with, and grows
//  the window at every layout after that where the content does not fit,
//  as after a move to a monitor with another DPI. Content with any text that
//  wraps implements it.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiMeasuredContent
{
public:
    virtual ~IDxuiMeasuredContent() = default;

    // The content's height in pixels at `widthPx`. On false the window's
    // size is left as created.
    virtual bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogWindow
//
//  Small shared base for the common dialog shape: a single fill content
//  panel above a bottom, right-aligned row of action buttons, hosted in a
//  DxuiWindow whose caption the host owns. Subclasses build their content
//  (a message body, a picker list, a progress panel, ...) plus their
//  buttons in OnCreate(), then the consumer calls ShowModalDialog(defaultId)
//  (or ShowModelessDialog(defaultId)).
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
    //  Append an action button carrying a Win32 command id (IDOK / IDCANCEL /
    //  IDYES / ...). Under ShowModalDialog / ShowModelessDialog the base
    //  auto-closes via EndDialog(commandId) unless the caller sets a custom
    //  SetOnClick (a button that must keep the dialog open, e.g. Download).
    //  By default the button joins the right-aligned primary group, ordered
    //  canonically (OK, Cancel, Apply, ...); pass Anchor::Left for a
    //  secondary / navigation button (e.g. "Browse...") pinned bottom-left.
    //  Returns the live button.
    //
    DxuiButton *  AddDialogButton (const std::wstring &     label,
                                   int                      commandId,
                                   DxuiButtonRow::Anchor    anchor = DxuiButtonRow::Anchor::Right);

    DxuiPanel  *  GetDialogContent   () const { return m_content; }

    //
    //  Geometry: content fills the client (inset by pad, above the button
    //  row); the buttons sit in a fixed-height bottom strip, right-aligned.
    //
    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

    //
    //  Fits the window's height to content that implements
    //  IDxuiMeasuredContent; for other content the window's size is left
    //  as created.
    //
    void  OnDialogShowing () override;

    //
    //  Grows the window by deltaPx, keeping its center, for content whose
    //  painted height is more than the room it was given. Nothing happens
    //  for a delta that is not positive, or while the window is growing.
    //
    HRESULT  GrowHeightBy (int deltaPx);


private:
    HRESULT  FitHeightToContent ();
    HRESULT  ResizeHeightBy     (int deltaPx);

    //  Grows the window when measured content is taller than `content`, its
    //  rectangle in this layout; true when the window was resized. A layout
    //  the growing itself causes is not checked again.
    bool     GrowToContent      (const RECT & content);

    struct ButtonEntry
    {
        DxuiButton *           button    = nullptr;
        int                    commandId = 0;
        DxuiButtonRow::Anchor  anchor    = DxuiButtonRow::Anchor::Right;
    };

    DxuiPanel *                m_content = nullptr;
    std::unique_ptr<DxuiPanel> m_contentOwned;
    std::vector<ButtonEntry>   m_dialogButtons;
    bool                       m_growing = false;
};

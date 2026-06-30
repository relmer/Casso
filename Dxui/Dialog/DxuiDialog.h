#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"


class DxuiCaptionBar;
class DxuiSystemButton;
class DxuiLabel;
class DxuiButton;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog
//
//  Composable modal-dialog control. Inherits all `DxuiPanel` semantics
//  (children, layout, paint fan-out) and adds a fixed three-region
//  layout assembled at `Build()` time:
//
//      [ DxuiCaptionBar (title + Close system button)        ]  (Top)
//      [ consumer-supplied content DxuiPanel                 ]  (Fill)
//      [ button row (DxuiPanel of DxuiButton, right-aligned) ]  (Bottom)
//
//  The dialog itself owns no Win32 surface. `DxuiDialogManager` is
//  responsible for hosting it in a `DxuiHostWindow`, painting the
//  optional modal scrim, and resolving the dialog's return code on
//  close.
//
//  Construction is two-phase: callers configure title, content, and
//  buttons, then call `Build()` to materialize the child panels and
//  install the `DxuiDockLayout`. This keeps construction headless-
//  testable -- a test can inspect button metadata, drive keyboard
//  activation, and verify close-handler dispatch without ever paint-
//  ing or laying the dialog out.
//
//  Default-button activation:
//      Enter key  → invokes the first button marked `isDefault`.
//      Escape key → invokes the first button marked `isCancel`; if
//                   none is explicitly marked, falls through so the
//                   manager can treat it as a close gesture.
//
//  All public methods are called on the UI thread (FR-083); each
//  entry point asserts this in debug builds.
//
////////////////////////////////////////////////////////////////////////////////



struct DxuiDialogButton
{
    std::wstring  label;
    int           returnCode = 0;
    bool          isDefault  = false;
    bool          isCancel   = false;
};



class DxuiDialog : public DxuiPanel
{
public:
    using CloseHandler = std::function<void (int returnCode)>;


    DxuiDialog  ();
    ~DxuiDialog() override;

    //
    //  Configuration (pre-Build). Calling any of these after Build()
    //  asserts in debug; release builds silently no-op.
    //
    void  SetTitle    (const std::wstring & title);
    void  SetContent  (std::unique_ptr<DxuiPanel> content);
    void  AddButton   (const std::wstring & label,
                       int                  returnCode,
                       bool                 isDefault = false,
                       bool                 isCancel  = false);

    //
    //  When false (pre-Build), the dialog does NOT build its own caption
    //  bar -- the hosting window supplies the standard caption (title +
    //  close) via DxuiHostWindow::captionStyle, and the dialog is just a
    //  content + button-row panel. Default true keeps the dialog a self-
    //  contained three-region control for unhosted / standalone use.
    //
    void  SetOwnCaption  (bool ownCaption);

    //
    //  Materialize the dialog's child panels (caption bar, content,
    //  button row) and install the dock layout. Idempotent: a second
    //  call asserts in debug, no-ops in release.
    //
    void  Build       ();
    bool  IsBuilt     () const { return m_built; }

    //
    //  Close-handler hook -- invoked by ActivateDefault / Activate-
    //  Cancel / button clicks with the chosen button's returnCode.
    //  The manager installs this when it owns a dialog so the close
    //  semantics route into the stack pop.
    //
    void  SetCloseHandler  (CloseHandler handler);
    bool  HasCloseHandler  () const { return (bool) m_onClose; }

    //
    //  Keyboard activation. Returns the chosen returnCode iff a
    //  matching button exists. The close handler (when set) is also
    //  invoked.
    //
    std::optional<int>  ActivateDefault();
    std::optional<int>  ActivateCancel  ();

    //
    //  Inspection accessors (used by tests + the manager).
    //
    const std::wstring          &  Title          () const { return m_title;   }
    size_t                         ButtonCount    () const { return m_buttons.size(); }
    const DxuiDialogButton      &  ButtonAt       (size_t idx) const { return m_buttons.at (idx); }
    int                            DefaultIndex   () const;
    int                            CancelIndex    () const;
    const DxuiPanel             *  ContentPanel   () const { return m_content; }
    const DxuiCaptionBar        *  CaptionBar     () const { return m_caption; }
    const DxuiPanel             *  ButtonRow      () const { return m_buttonRow; }

    //
    //  IDxuiControl override -- Enter / Escape map to Activate-
    //  Default / ActivateCancel before forwarding to children.
    //
    bool                OnKey          (const DxuiKeyEvent & ev) override;
    DxuiAccessibleRole  AccessibleRole() const                  override { return DxuiAccessibleRole::Dialog; }
    std::wstring        AccessibleName() const                  override { return m_title; }


private:
    void  InvokeClose          (int returnCode);
    void  HandleButtonClicked  (size_t index);


    std::wstring                   m_title;
    std::vector<DxuiDialogButton>  m_buttons;
    std::unique_ptr<DxuiPanel>     m_contentOwned;     // pre-Build storage; transferred to base on Build
    DxuiPanel *                    m_content     = nullptr;
    DxuiCaptionBar *               m_caption     = nullptr;
    DxuiPanel *                    m_buttonRow   = nullptr;
    DxuiLabel *                    m_titleLabel  = nullptr;
    DxuiSystemButton *             m_closeBtn    = nullptr;
    std::vector<DxuiButton *>      m_buttonWidgets;
    CloseHandler                   m_onClose;
    bool                           m_built       = false;
    bool                           m_ownCaption  = true;

    //
    //  Owner storage for the composite child controls (caption bar,
    //  title label, close button, content panel, button row).
    //  DxuiPanel does not expose a public "append owned IDxuiControl"
    //  entry point, so the dialog Adopts the live instances and keeps
    //  ownership here.
    //
    std::vector<std::unique_ptr<IDxuiControl>>  m_ownedComposites;
};

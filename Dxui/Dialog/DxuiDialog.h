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
    //  Close-handler hook -- invoked by TriggerDefault / TriggerCancel /
    //  button clicks with the chosen button's returnCode. The manager
    //  installs this when it owns a dialog so the close semantics route
    //  into the stack pop.
    //
    void  SetCloseHandler  (CloseHandler handler);
    bool  HasCloseHandler  () const { return (bool) m_onClose; }

    //
    //  Close the dialog with an arbitrary return code, from content (e.g.
    //  a list row activation) rather than a button. Routes through the
    //  same close handler the buttons use.
    //
    void  CloseWithResult  (int returnCode);

    //
    //  Optional control to focus when the dialog is first shown (e.g. a
    //  picker's search box). Default null = focus nothing, so Enter
    //  activates the default button.
    //
    void            SetInitialFocus (IDxuiControl * ctl) { m_initialFocus = ctl; }
    IDxuiControl *  InitialFocus    () const             { return m_initialFocus; }

    //
    //  Per-click hook. When set, a button click calls it with the button
    //  index; returning true closes the dialog with that button's return
    //  code, false keeps the dialog open (e.g. to start an in-place
    //  download). When unset, every click closes.
    //
    using ButtonActivatedFn = std::function<bool (size_t index)>;
    void  SetOnButtonActivated (ButtonActivatedFn fn) { m_onButtonActivated = std::move (fn); }

    //
    //  Periodic tick hook. When intervalMs > 0 the hosting ShowModal drives
    //  a Win32 timer that calls Tick() (which invokes the hook) every
    //  interval; the host repaints afterwards.
    //
    void      SetOnTick        (std::function<void()> fn, unsigned intervalMs);
    unsigned  TickIntervalMs   () const { return m_tickIntervalMs; }
    void      Tick             ();

    //
    //  Button mutation (post-Build). Out-of-range indices are ignored.
    //
    void  SetButtonLabel   (size_t index, const std::wstring & label);
    void  SetButtonEnabled (size_t index, bool enabled);
    void  SetButtonVisible (size_t index, bool visible);

    //
    //  Keyboard-activation entry points. TriggerDefault fires the button
    //  flagged isDefault (Enter); TriggerCancel fires the button flagged
    //  isCancel (Escape / close gesture). Firing routes through the normal
    //  button-click path -- and thus the close handler, which delivers the
    //  button's returnCode to the manager. Each returns true iff such a
    //  button exists (and was therefore fired); false means none is defined
    //  and the caller must handle the key / close itself.
    //
    bool  TriggerDefault();
    bool  TriggerCancel  ();

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
    //  IDxuiControl override -- Enter / Escape map to TriggerDefault /
    //  TriggerCancel before forwarding to children.
    //
    bool                OnKey          (const DxuiKeyEvent & ev) override;
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
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
    ButtonActivatedFn              m_onButtonActivated;
    std::function<void()>          m_onTick;
    unsigned                       m_tickIntervalMs = 0;
    IDxuiControl *                 m_initialFocus = nullptr;
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

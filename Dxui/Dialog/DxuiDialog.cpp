#include "Pch.h"

#include "Dialog/DxuiDialog.h"

#include "Core/DxuiDockLayout.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiLabel.h"
#include "Window/DxuiCaptionBar.h"
#include "Window/DxuiSystemButton.h"


static constexpr LONG  s_kCaptionHeightDip     = 32;
static constexpr LONG  s_kButtonRowHeightDip   = 44;
static constexpr int   s_kButtonWidthDip       = 96;
static constexpr int   s_kButtonHeightDip      = 28;
static constexpr int   s_kButtonGapDip         = 8;
static constexpr int   s_kButtonRowEdgePadDip  = 16;
static constexpr int   s_kContentPadDip        = 16;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::DxuiDialog
//
////////////////////////////////////////////////////////////////////////////////

DxuiDialog::DxuiDialog()
{
    DXUI_ASSERT_UI_THREAD();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::~DxuiDialog
//
////////////////////////////////////////////////////////////////////////////////

DxuiDialog::~DxuiDialog()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::SetTitle
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::SetTitle (const std::wstring & title)
{
    HRESULT  hr = S_OK;



    DXUI_ASSERT_UI_THREAD();
    CBRA (!m_built);

    m_title = title;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::SetContent
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::SetContent (std::unique_ptr<DxuiPanel> content)
{
    HRESULT  hr = S_OK;



    DXUI_ASSERT_UI_THREAD();
    CBRA (!m_built);

    m_contentOwned = std::move (content);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::AddButton
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::AddButton (const std::wstring & label,
                            int                  returnCode,
                            bool                 isDefault,
                            bool                 isCancel)
{
    HRESULT           hr = S_OK;
    DxuiDialogButton  btn;



    DXUI_ASSERT_UI_THREAD();
    CBRA (!m_built);

    btn.label      = label;
    btn.returnCode = returnCode;
    btn.isDefault  = isDefault;
    btn.isCancel   = isCancel;

    m_buttons.push_back (std::move (btn));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::Build
//
//  Materializes the caption-bar / content / button-row child panels and
//  installs the dock layout. The content panel transfers from the pre-
//  Build storage into the unified child list. Each configured button
//  becomes a DxuiButton inside the button row with a click handler that
//  routes through HandleButtonClicked.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::Build()
{
    HRESULT                            hr            = S_OK;
    std::unique_ptr<DxuiCaptionBar>    captionOwn;
    std::unique_ptr<DxuiLabel>         titleOwn;
    std::unique_ptr<DxuiSystemButton>  closeOwn;
    std::unique_ptr<DxuiPanel>         contentOwn    = std::move (m_contentOwned);
    std::unique_ptr<DxuiPanel>         buttonRowOwn  = std::make_unique<DxuiPanel>();
    std::unique_ptr<DxuiDockLayout>    dockOwn       = std::make_unique<DxuiDockLayout>();
    DxuiCaptionBar *                   captionRaw    = nullptr;
    DxuiLabel *                        titleRaw      = nullptr;
    DxuiSystemButton *                 closeRaw      = nullptr;
    DxuiPanel *                        contentRaw    = nullptr;
    DxuiPanel *                        buttonRowRaw  = buttonRowOwn.get();
    DxuiDockLayout *                   dockRaw       = dockOwn.get();
    RECT                               captionRect   = { 0, 0, 0, s_kCaptionHeightDip };
    RECT                               buttonRowRect = { 0, 0, 0, s_kButtonRowHeightDip };



    DXUI_ASSERT_UI_THREAD();
    CBRA (!m_built);

    //
    //  Synthesize an empty content panel when the consumer did not
    //  provide one so the dock layout's Fill slot still has a child.
    //
    if (!contentOwn)
    {
        contentOwn = std::make_unique<DxuiPanel>();
    }

    contentRaw = contentOwn.get();

    //
    //  Caption-bar children (only when the dialog owns its caption).
    //  When a host supplies the standard caption (SetOwnCaption(false)),
    //  the dialog is just content + button-row and these are skipped.
    //
    if (m_ownCaption)
    {
        captionOwn = std::make_unique<DxuiCaptionBar>();
        titleOwn   = std::make_unique<DxuiLabel>();
        closeOwn   = std::make_unique<DxuiSystemButton> (DxuiSystemButtonKind::Close);
        captionRaw = captionOwn.get();
        titleRaw   = titleOwn.get();
        closeRaw   = closeOwn.get();

        titleRaw->SetText   (m_title);
        titleRaw->SetVAlign (DxuiTextVAlign::Center);
        titleRaw->SetHAlign (DxuiTextHAlign::Left);

        captionRaw->SetBounds (captionRect);
        captionRaw->Adopt     (*titleRaw);
        captionRaw->Adopt     (*closeRaw);
    }

    //
    //  Button row: one DxuiButton per configured DxuiDialogButton. The
    //  buttons are sized and right-aligned in Layout(); the row carries
    //  no layout of its own. The default button gets a themed emphasis
    //  outline.
    //
    buttonRowRaw->SetBounds (buttonRowRect);
    m_buttonWidgets.clear();
    m_buttonWidgets.reserve (m_buttons.size());
    for (size_t i = 0; i < m_buttons.size(); i++)
    {
        DxuiButton  &  btn = buttonRowRaw->Add<DxuiButton>();

        btn.SetLabel    (m_buttons[i].label);
        btn.SetEnabled  (true);
        btn.SetEmphasis (m_buttons[i].isDefault);
        btn.SetOnClick    ([this, i] () { this->HandleButtonClicked (i); });

        m_buttonWidgets.push_back (&btn);
    }

    //
    //  Transfer ownership into long-lived member storage first, then
    //  Adopt the live pointers into the panel tree. The dialog's
    //  m_ownedComposites vector outlives the IDxuiControl tree.
    //
    if (m_ownCaption)
    {
        m_ownedComposites.push_back (std::move (captionOwn));
        m_ownedComposites.push_back (std::move (titleOwn));
        m_ownedComposites.push_back (std::move (closeOwn));
    }
    m_ownedComposites.push_back (std::move (contentOwn));
    m_ownedComposites.push_back (std::move (buttonRowOwn));

    if (m_ownCaption)
    {
        Adopt (*captionRaw);
    }
    Adopt (*contentRaw);
    Adopt (*buttonRowRaw);

    m_caption     = captionRaw;
    m_titleLabel  = titleRaw;
    m_closeBtn    = closeRaw;
    m_content     = contentRaw;
    m_buttonRow   = buttonRowRaw;

    //
    //  Install the dock layout on the dialog itself. The caption (when
    //  owned) docks Top; the button row docks Bottom; content fills.
    //
    if (m_ownCaption)
    {
        dockRaw->SetDock (*captionRaw, DxuiDock::Top);
    }
    dockRaw->SetDock (*buttonRowRaw, DxuiDock::Bottom);
    dockRaw->SetDock (*contentRaw,   DxuiDock::Fill);
    SetLayout (std::move (dockOwn));

    m_built = true;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::Layout
//
//  The host now lays the dialog (root) out in physical pixels. The fixed
//  caption / button-row slab heights are authored in DIP, so scale them to
//  px here before the dock arranges; the incoming bounds pass through.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    DXUI_ASSERT_UI_THREAD();

    if (m_caption != nullptr)
    {
        m_caption->SetBounds ({ 0, 0, 0, scaler.Px ((int) s_kCaptionHeightDip) });
    }

    if (m_buttonRow != nullptr)
    {
        m_buttonRow->SetBounds ({ 0, 0, 0, scaler.Px ((int) s_kButtonRowHeightDip) });
    }

    DxuiPanel::Layout (boundsPx, scaler);

    //
    //  Inset the content region by a standard padding so body text and
    //  controls don't touch the window edges.
    //
    if (m_content != nullptr)
    {
        RECT  c   = m_content->Bounds();
        int   pad = scaler.Px (s_kContentPadDip);

        c.left   += pad;
        c.top    += pad;
        c.right  -= pad;
        c.bottom -= pad;
        m_content->Layout (c, scaler);
    }

    //
    //  Size + right-align the action buttons within the button row, in
    //  registration order (first button leftmost, last rightmost).
    //
    if (m_buttonRow != nullptr && !m_buttonWidgets.empty())
    {
        RECT  row     = m_buttonRow->Bounds();
        int   wPx     = scaler.Px (s_kButtonWidthDip);
        int   hPx     = scaler.Px (s_kButtonHeightDip);
        int   gapPx   = scaler.Px (s_kButtonGapDip);
        int   edgePx  = scaler.Px (s_kButtonRowEdgePadDip);
        int   botPad  = scaler.Px (s_kContentPadDip);
        int   count   = (int) m_buttonWidgets.size();
        int   total   = count * wPx + (count - 1) * gapPx;
        int   x       = row.right - edgePx - total;
        int   y       = row.bottom - botPad - hPx;

        for (int i = 0; i < count; i++)
        {
            RECT  b = { x, y, x + wPx, y + hPx };

            m_buttonWidgets[(size_t) i]->Layout (b);
            m_buttonWidgets[(size_t) i]->SetDpi (scaler.Dpi());
            x += wPx + gapPx;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::SetCloseHandler
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::SetCloseHandler (CloseHandler handler)
{
    DXUI_ASSERT_UI_THREAD();
    m_onClose = std::move (handler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::CloseWithResult
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::CloseWithResult (int returnCode)
{
    DXUI_ASSERT_UI_THREAD();
    InvokeClose (returnCode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::SetOwnCaption
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::SetOwnCaption (bool ownCaption)
{
    HRESULT  hr = S_OK;



    DXUI_ASSERT_UI_THREAD();
    CBRA (!m_built);

    m_ownCaption = ownCaption;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::DefaultIndex
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDialog::DefaultIndex() const
{
    for (size_t i = 0; i < m_buttons.size(); i++)
    {
        if (m_buttons[i].isDefault)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::CancelIndex
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDialog::CancelIndex() const
{
    for (size_t i = 0; i < m_buttons.size(); i++)
    {
        if (m_buttons[i].isCancel)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::TriggerDefault
//
//  Fires the button flagged isDefault (the Enter target), if one exists,
//  via the normal click path. Returns true iff a default button exists
//  and was therefore fired; false means no default is defined. The fired
//  button's returnCode reaches the caller through the close handler, not
//  this return value.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialog::TriggerDefault()
{
    HRESULT  hr        = S_OK;
    bool     isHandled = false;
    int      idx       = DefaultIndex();



    DXUI_ASSERT_UI_THREAD();

    BAIL_OUT_IF (idx < 0, S_OK);

    //
    //  HandleButtonClicked routes through the close handler, which the
    //  manager uses to pop (and destroy) this dialog -- so touching any
    //  member after the call would be a use-after-free. isHandled is a
    //  local, set before the call, and is the only thing read at Error.
    //
    isHandled = true;
    HandleButtonClicked ((size_t) idx);

Error:
    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::TriggerCancel
//
//  Fires the button flagged isCancel (the Escape / close-gesture target),
//  if one exists, via the normal click path. Returns true iff a cancel
//  button exists and was therefore fired; false means no cancel button is
//  defined and the caller must handle the close itself. The fired button's
//  returnCode reaches the caller through the close handler, not this
//  return value.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialog::TriggerCancel()
{
    HRESULT  hr        = S_OK;
    bool     isHandled = false;
    int      idx       = CancelIndex();



    DXUI_ASSERT_UI_THREAD();

    BAIL_OUT_IF (idx < 0, S_OK);

    //  Same use-after-free hazard as TriggerDefault: isHandled is a local
    //  set before the click routes through the close handler.
    isHandled = true;
    HandleButtonClicked ((size_t) idx);

Error:
    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::OnKey
//
//  Standard dialog key routing. The focused control (reached through the
//  panel-tree fan-out) gets first crack at every key -- a focused button
//  activates on Enter, an open dropdown closes on Escape, a text field
//  keeps its editing keys. Only when nothing consumes the key do Enter /
//  Escape fall back to the default / cancel button. Returns true iff the
//  key was handled (by a control or by the default/cancel fallback).
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialog::OnKey (const DxuiKeyEvent & ev)
{
    bool  isHandled = false;


    DXUI_ASSERT_UI_THREAD();

    isHandled = DxuiPanel::OnKey (ev);

    if (!isHandled && ev.kind == DxuiKeyEventKind::Down)
    {
        if (ev.vk == VK_RETURN)
        {
            isHandled = TriggerDefault();
        }
        else if (ev.vk == VK_ESCAPE)
        {
            isHandled = TriggerCancel();
        }
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::HandleButtonClicked
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::HandleButtonClicked (size_t index)
{
    HRESULT  hr          = S_OK;
    bool     shouldClose = true;


    CBRA (index < m_buttons.size());

    if (m_onButtonActivated)
    {
        shouldClose = m_onButtonActivated (index);
    }

    if (shouldClose)
    {
        InvokeClose (m_buttons[index].returnCode);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::SetOnTick / Tick
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::SetOnTick (std::function<void()> fn, unsigned intervalMs)
{
    DXUI_ASSERT_UI_THREAD();
    m_onTick         = std::move (fn);
    m_tickIntervalMs = intervalMs;
}


void DxuiDialog::Tick ()
{
    if (m_onTick)
    {
        m_onTick();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::SetButtonLabel / SetButtonEnabled / SetButtonVisible
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::SetButtonLabel (size_t index, const std::wstring & label)
{
    DXUI_ASSERT_UI_THREAD();

    if (index < m_buttonWidgets.size() && m_buttonWidgets[index] != nullptr)
    {
        m_buttonWidgets[index]->SetLabel (label);
    }
}


void DxuiDialog::SetButtonEnabled (size_t index, bool enabled)
{
    DXUI_ASSERT_UI_THREAD();

    if (index < m_buttonWidgets.size() && m_buttonWidgets[index] != nullptr)
    {
        m_buttonWidgets[index]->SetEnabled (enabled);
    }
}


void DxuiDialog::SetButtonVisible (size_t index, bool visible)
{
    DXUI_ASSERT_UI_THREAD();

    if (index < m_buttonWidgets.size() && m_buttonWidgets[index] != nullptr)
    {
        m_buttonWidgets[index]->SetVisible (visible);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::InvokeClose
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::InvokeClose (int returnCode)
{
    CloseHandler  cb = m_onClose;



    if (cb)
    {
        cb (returnCode);
    }
}

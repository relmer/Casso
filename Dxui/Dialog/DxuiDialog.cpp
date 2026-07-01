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
static constexpr int   s_kButtonRowEdgePadDip  = 12;
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
    DXUI_ASSERT_UI_THREAD();
    assert (!m_built && "SetTitle called after Build");

    if (m_built)
    {
        return;
    }

    m_title = title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::SetContent
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::SetContent (std::unique_ptr<DxuiPanel> content)
{
    DXUI_ASSERT_UI_THREAD();
    assert (!m_built && "SetContent called after Build");

    if (m_built)
    {
        return;
    }

    m_contentOwned = std::move (content);
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
    DxuiDialogButton  btn;



    DXUI_ASSERT_UI_THREAD();
    assert (!m_built && "AddButton called after Build");

    if (m_built)
    {
        return;
    }

    btn.label      = label;
    btn.returnCode = returnCode;
    btn.isDefault  = isDefault;
    btn.isCancel   = isCancel;

    m_buttons.push_back (std::move (btn));
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
    assert (!m_built && "Build called twice");

    if (m_built)
    {
        return;
    }

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
        btn.SetClick    ([this, i] () { this->HandleButtonClicked (i); });

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

    IGNORE_RETURN_VALUE (hr, S_OK);
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
        RECT  row    = m_buttonRow->Bounds();
        int   wPx    = scaler.Px (s_kButtonWidthDip);
        int   hPx    = scaler.Px (s_kButtonHeightDip);
        int   gapPx  = scaler.Px (s_kButtonGapDip);
        int   edgePx = scaler.Px (s_kButtonRowEdgePadDip);
        int   count  = (int) m_buttonWidgets.size();
        int   total  = count * wPx + (count - 1) * gapPx;
        int   x      = row.right - edgePx - total;
        int   y      = row.top + ((row.bottom - row.top) - hPx) / 2;

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
    DXUI_ASSERT_UI_THREAD();
    assert (!m_built && "SetOwnCaption called after Build");

    if (m_built)
    {
        return;
    }

    m_ownCaption = ownCaption;
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
//  DxuiDialog::ActivateDefault
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int> DxuiDialog::ActivateDefault()
{
    int  idx = DefaultIndex();
    int  rc  = 0;



    DXUI_ASSERT_UI_THREAD();

    if (idx < 0)
    {
        return std::nullopt;
    }

    //
    //  Capture the return code BEFORE invoking the click handler:
    //  HandleButtonClicked routes through the close handler which the
    //  manager uses to pop (and destroy) this dialog. Touching
    //  `this->m_buttons` after the call would be a use-after-free.
    //
    rc = m_buttons[(size_t) idx].returnCode;
    HandleButtonClicked ((size_t) idx);
    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::ActivateCancel
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int> DxuiDialog::ActivateCancel()
{
    int  idx = CancelIndex();
    int  rc  = 0;



    DXUI_ASSERT_UI_THREAD();

    if (idx < 0)
    {
        return std::nullopt;
    }

    //
    //  Capture the return code BEFORE invoking the click handler --
    //  same use-after-free hazard as ActivateDefault.
    //
    rc = m_buttons[(size_t) idx].returnCode;
    HandleButtonClicked ((size_t) idx);
    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::OnKey
//
//  Enter → ActivateDefault; Escape → ActivateCancel. Either consumes
//  the event when a matching button exists; otherwise the base panel
//  fan-out runs so embedded text inputs / lists keep their keys.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialog::OnKey (const DxuiKeyEvent & ev)
{
    DXUI_ASSERT_UI_THREAD();

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        if (ev.vk == VK_RETURN)
        {
            std::optional<int>  rc = ActivateDefault();

            if (rc.has_value())
            {
                return true;
            }
        }
        else if (ev.vk == VK_ESCAPE)
        {
            std::optional<int>  rc = ActivateCancel();

            if (rc.has_value())
            {
                return true;
            }
        }
    }

    return DxuiPanel::OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialog::HandleButtonClicked
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::HandleButtonClicked (size_t index)
{
    bool  shouldClose = true;


    if (index < m_buttons.size())
    {
        if (m_onButtonActivated)
        {
            shouldClose = m_onButtonActivated (index);
        }

        if (shouldClose)
        {
            InvokeClose (m_buttons[index].returnCode);
        }
    }
    else
    {
        assert (false && "Button index out of range");
    }
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

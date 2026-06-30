#include "Pch.h"

#include "Dialog/DxuiDialog.h"

#include "Core/DxuiDockLayout.h"
#include "Core/DxuiStackLayout.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiLabel.h"
#include "Window/DxuiCaptionBar.h"
#include "Window/DxuiSystemButton.h"


static constexpr LONG  s_kCaptionHeightDip    = 32;
static constexpr LONG  s_kButtonRowHeightDip  = 44;
static constexpr float s_kButtonRowPaddingDip = 8.0f;





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
    //  Button row: one DxuiButton per configured DxuiDialogButton.
    //  Layout positioning (right-aligned strip with horizontal
    //  spacing) is applied at first Layout() pass via a stack layout
    //  installed on the row.
    //
    buttonRowRaw->SetBounds (buttonRowRect);
    m_buttonWidgets.clear();
    m_buttonWidgets.reserve (m_buttons.size());
    for (size_t i = 0; i < m_buttons.size(); i++)
    {
        DxuiButton  &  btn = buttonRowRaw->Add<DxuiButton>();

        btn.SetLabel   (m_buttons[i].label);
        btn.SetEnabled (true);
        btn.SetClick   ([this, i] () { this->HandleButtonClicked (i); });

        m_buttonWidgets.push_back (&btn);
    }

    {
        std::unique_ptr<DxuiStackLayout>  rowLayout = std::make_unique<DxuiStackLayout> (
            DxuiStackLayout::Orientation::Horizontal,
            s_kButtonRowPaddingDip,
            DxuiStackLayout::Align::Stretch);

        buttonRowRaw->SetLayout (std::move (rowLayout));
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
//  The host lays the dialog (root) out in DIP, but the dialog's leaf
//  widgets paint their bounds as physical pixels. Convert the incoming
//  DIP rect to px and scale the fixed caption / button-row slab heights
//  so the dock arranges in pixel space and the content fills the whole
//  physical client region.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialog::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    RECT  px = {};


    DXUI_ASSERT_UI_THREAD();

    px.left   = scaler.Px ((int) boundsDip.left);
    px.top    = scaler.Px ((int) boundsDip.top);
    px.right  = scaler.Px ((int) boundsDip.right);
    px.bottom = scaler.Px ((int) boundsDip.bottom);

    if (m_caption != nullptr)
    {
        m_caption->SetBounds ({ 0, 0, 0, scaler.Px ((int) s_kCaptionHeightDip) });
    }

    if (m_buttonRow != nullptr)
    {
        m_buttonRow->SetBounds ({ 0, 0, 0, scaler.Px ((int) s_kButtonRowHeightDip) });
    }

    DxuiPanel::Layout (px, scaler);
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
    if (index >= m_buttons.size())
    {
        assert (false && "Button index out of range");
        return;
    }

    InvokeClose (m_buttons[index].returnCode);
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

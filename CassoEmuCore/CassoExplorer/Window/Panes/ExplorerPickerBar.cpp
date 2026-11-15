#include "Pch.h"

#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerPickerMode.h"
#include "CassoExplorer/Window/Panes/ExplorerPickerBar.h"
#include "CassoExplorer/Window/Panes/ExplorerPickerChips.h"
#include "Devices/Disk/DriveImageRule.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::ExplorerPickerBar
//
////////////////////////////////////////////////////////////////////////////////

ExplorerPickerBar::ExplorerPickerBar (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::GetBandDip
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerPickerBar::GetBandDip() const
{
    return m_context.IsPicker() ? kBandDip : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::Create
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerBar::Create()
{
    const PickerRequest *  request = m_context.app.picker;



    if (request == nullptr)
    {
        return;
    }

    m_header = m_context.window.CreateChild<DxuiLabel> (m_context.pickerMode.GetHeaderText());
    m_header->SetElide (DxuiElide::Tail);

    if (request->receivingDrive.has_value())
    {
        m_chips = m_context.window.CreateChild<ExplorerPickerChips>();
        m_chips->SetTerms (DriveImageRule::GetAcceptedTerms (*request->receivingDrive));
    }

    if (request->IsSave())
    {
        m_nameLabel = m_context.window.CreateChild<DxuiLabel> (L"File name:");
        m_name      = m_context.window.CreateChild<DxuiTextInput>();

        m_context.window.SetMouseHandler (m_name, [this] (const DxuiMouseEvent & ev) { return HandleNameMouse (ev); });

        m_name->SetText     (request->defaultFileName);
        m_name->SetOnChange ([this] (const std::wstring &) { Update(); });
    }

    if (m_context.pickerMode.GetFilterCount() > 1)
    {
        m_filter = m_context.window.CreateChild<DxuiButton> (std::wstring());
        m_filter->SetElide   (DxuiElide::Tail);
        m_filter->SetOnClick ([this] () { ShowFilterMenu(); });
    }

    if (m_context.pickerMode.CanCreateNew())
    {
        m_create = m_context.window.CreateChild<DxuiButton> ((request->receivingDrive.has_value() && request->receivingDrive->kind == ReceivingDriveKind::Recorder) ? L"New blank tape" : L"Create new disk...");
        m_create->SetOnClick ([this] () { m_context.pickerMode.CreateNew(); });
    }

    if (!request->skipLabel.empty())
    {
        m_skip = m_context.window.CreateChild<DxuiButton> (request->skipLabel);
        m_skip->SetOnClick ([this] () { m_context.pickerMode.Skip(); });
    }

    m_confirm = m_context.window.CreateChild<DxuiButton> (m_context.pickerMode.GetConfirmLabel());
    m_confirm->SetVariant (DxuiButton::Variant::Primary);
    m_confirm->SetOnClick ([this] () { m_context.pickerMode.Confirm(); });

    m_cancel = m_context.window.CreateChild<DxuiButton> (L"Cancel");
    m_cancel->SetOnClick ([this] () { m_context.pickerMode.Cancel(); });

    Update();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::Layout
//
//  Cancel and the confirm button at the right, the skip button, the create
//  command and the filter choice before them, and the header in what is
//  left.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerBar::Layout (const RECT & band)
{
    const DxuiDpiScaler &  scaler = m_context.scaler;
    int                    height = scaler.ToPx (kButtonDip);
    int                    top    = band.top + ((band.bottom - band.top) - height) / 2;
    int                    right  = band.right - scaler.ToPx (kEdgePadDip);
    int                    gap    = scaler.ToPx (kGapDip);
    int                    width  = 0;



    if (m_confirm == nullptr)
    {
        return;
    }

    for (DxuiButton * button : { m_cancel, m_confirm, m_skip, m_create, m_filter })
    {
        if (button == nullptr)
        {
            continue;
        }

        width = scaler.ToPx ((button == m_filter) ? kFilterMaxDip : DxuiButtonRow::GetWidthForLabel (button->GetAccessibleName()));

        button->Layout (RECT { right - width, top, right, top + height }, scaler);
        right -= width + gap;
    }

    if (m_name == nullptr)
    {
        m_header->Layout (RECT { band.left + scaler.ToPx (kEdgePadDip), band.top, (std::max) (band.left, (LONG) right), band.bottom }, scaler);
        return;
    }

    //  A save form has the file name box where a drive's picker has its
    //  header.
    if (!m_isNameConfigured && m_context.window.GetHwnd() != nullptr)
    {
        m_name->SetTextRenderer   (m_context.window.GetTextRenderer());
        m_name->SetHwnd           (m_context.window.GetHwnd());
        m_name->SetFont           (DxuiAddressBar::kVariableTextFace, DxuiAddressBar::kFontDip);
        m_name->SetExplorerChrome (true);

        m_isNameConfigured = true;
    }

    {
        int  left  = band.left + scaler.ToPx (kEdgePadDip);
        int  label = left + scaler.ToPx (kNameLabelDip);

        m_header->Layout    (RECT { left, band.top, left, band.bottom }, scaler);
        m_nameLabel->Layout (RECT { left, band.top, label, band.bottom }, scaler);
        m_name->Layout      (RECT { label, top, (std::max) (label + scaler.ToPx (kNameMinDip), right), top + height }, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::Update
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerBar::Update()
{
    if (m_confirm == nullptr)
    {
        return;
    }

    //  Selecting a file puts its name in a save form's box, once for each
    //  selection, so typing over it is not undone by a repaint.
    if (m_name != nullptr)
    {
        std::wstring  selected;

        if (m_context.pickerMode.TryGetSelectedFile (selected) && selected != m_copiedFrom)
        {
            m_copiedFrom = selected;
            m_name->SetText (std::filesystem::path (selected).filename().wstring());
        }
    }

    m_confirm->SetEnabled (m_context.pickerMode.CanConfirm());

    if (m_filter != nullptr)
    {
        m_filter->SetLabel (m_context.pickerMode.GetFilterLabel (m_context.pickerMode.GetActiveFilter()));
    }

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::IsConfirmEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerBar::IsConfirmEnabled() const
{
    return m_confirm != nullptr && m_confirm->IsEnabled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::GetNameText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerPickerBar::GetNameText() const
{
    return (m_name != nullptr) ? m_name->GetText() : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::SetNameText
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerBar::SetNameText (const std::wstring & text)
{
    if (m_name != nullptr)
    {
        m_name->SetText (text);
        Update();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::HandleNameMouse
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerBar::HandleNameMouse (const DxuiMouseEvent & ev)
{
    if (m_name == nullptr)
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_context.input.SetFocusPane (ExplorerPane::PickerName);
    }

    m_name->OnMouse (ev);
    m_context.window.Invalidate();

    return ev.kind != DxuiMouseEventKind::Leave;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::HandleNameKey
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerBar::HandleNameKey (const DxuiKeyEvent & ev)
{
    if (m_name == nullptr)
    {
        return false;
    }

    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_RETURN && !ev.ctrl && !ev.alt)
    {
        m_context.pickerMode.Confirm();
        return true;
    }

    return m_name->OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::HandleFocusChanged
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerBar::HandleFocusChanged (bool isFocused)
{
    if (m_name != nullptr)
    {
        m_name->SetFocused (isFocused);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::IsCaretOn
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerBar::IsCaretOn() const
{
    return m_name != nullptr && m_name->IsCaretOn();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::ShowFilterMenu
//
//  The filter entries, as a menu under the filter button.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerBar::ShowFilterMenu()
{
    std::vector<DxuiPopupMenuItem>  items;



    if (m_context.window.GetPopupHost() == nullptr)
    {
        return;
    }

    m_context.menus.GetMenuCommands().clear();

    for (size_t index = 0; index < m_context.pickerMode.GetFilterCount(); index++)
    {
        m_context.menus.AddMenuCommand (items, m_context.pickerMode.GetFilterLabel (index).c_str(), [this, index] () { m_context.pickerMode.SetActiveFilter (index); });
    }

    DxuiContextMenu::ShowUnder (*m_context.window.GetPopupHost(), m_filter->GetBounds(), std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::LayoutChips
//
//  The capsules start a gap past the divider between the tree and the list,
//  or past View when View ends to the right of the divider.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerBar::LayoutChips (const RECT & freeArea, int listLeft)
{
    RECT  bounds = freeArea;



    if (m_chips == nullptr)
    {
        return;
    }

    bounds.left = (std::max) (freeArea.left, (LONG) (listLeft + m_context.scaler.ToPx (kGapDip)));

    m_chips->SetVisible (bounds.right > bounds.left && bounds.bottom > bounds.top);
    m_chips->Layout     (bounds, m_context.scaler);
}

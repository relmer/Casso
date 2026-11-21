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
    int  rows = 0;



    if (!m_context.IsPicker())
    {
        return 0;
    }

    if (m_name == nullptr)
    {
        return kBandDip;
    }

    //  A save form: the name row, the type row and its note, and the
    //  button row.
    rows = kButtonDip + kGapDip + kButtonDip;

    if (m_filter != nullptr)
    {
        rows += kButtonDip + kGapDip;
    }

    if (m_filterNote != nullptr)
    {
        rows += kNoteDip;
    }

    return kRowPadDip + rows + kRowPadDip;
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

    if (request->receivingDrive.has_value())
    {
        m_chips = m_context.window.CreateChild<ExplorerPickerChips>();
        m_chips->SetTerms (DriveImageRule::GetAcceptedTerms (*request->receivingDrive));
    }

    if (request->IsSave())
    {
        m_nameLabel = m_context.window.CreateChild<DxuiLabel> (L"File name:");
        m_name      = m_context.window.CreateChild<DxuiTextInput>();

        m_nameLabel->SetTextAlign   (DxuiTextHAlign::Right, DxuiTextVAlign::Center);
        m_nameLabel->SetFontFace    (kDialogFace);
        m_nameLabel->SetFontSizeDip (kDialogFontDip);

        m_context.window.SetMouseHandler (m_name, [this] (const DxuiMouseEvent & ev) { return HandleNameMouse (ev); });

        m_name->SetText     (request->defaultFileName);
        m_name->SetOnChange ([this] (const std::wstring &) { Update(); });
    }

    if (request->offerVerify)
    {
        m_verify = m_context.window.CreateChild<DxuiCheckbox> (request->verifyLabel.empty() ? std::wstring (L"Verify") : request->verifyLabel, request->verify);
        m_verify->SetSingleLineLabel (true);
    }

    //  Only a save form chooses a file type; an open form lists every
    //  image the request takes.
    if (request->IsSave() && request->filters.size() > 1)
    {
        m_filterLabel = m_context.window.CreateChild<DxuiLabel> (request->isImageContent ? L"Disk image type:" : L"Save as type:");
        m_filter      = m_context.window.CreateChild<DxuiComboBox>();

        m_filterLabel->SetTextAlign   (DxuiTextHAlign::Right, DxuiTextVAlign::Center);
        m_filterLabel->SetFontFace    (kDialogFace);
        m_filterLabel->SetFontSizeDip (kDialogFontDip);
        m_filter->SetElide     (DxuiElide::Middle);
        m_filter->SetPopupHost (m_context.window.GetPopupHost());

        FillFilterList();

        if (HasFilterNotes())
        {
            m_filterNote = m_context.window.CreateChild<DxuiLabel> (std::wstring());
            m_filterNote->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
            m_filterNote->SetElide     (DxuiElide::Tail);
        }
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
    m_confirm->SetEmphasis (true);
    m_confirm->SetOnClick ([this] () { m_context.pickerMode.Confirm(); });

    m_cancel = m_context.window.CreateChild<DxuiButton> (L"Cancel");
    m_cancel->SetOnClick ([this] () { m_context.pickerMode.Cancel(); });

    Update();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::Layout
//
//  An open form is one row: the buttons at the right and the Verify box
//  before them. A save form is the standard Save As dialog's stack: the
//  labeled file name box and file type list, both starting at the list's
//  left edge, the type's note under the list, and the buttons in a row
//  of their own at the bottom right.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerBar::Layout (const RECT & band, int listLeft)
{
    const DxuiDpiScaler &  scaler = m_context.scaler;
    int                    height = scaler.ToPx (kButtonDip);
    int                    pad    = scaler.ToPx (kEdgePadDip);
    int                    top    = band.top + (scaler.ToPx (kBandDip) - height) / 2;
    int                    right  = 0;



    if (m_confirm == nullptr)
    {
        return;
    }

    if (m_name != nullptr)
    {
        LayoutSaveRows (band, listLeft);
        top = band.bottom - scaler.ToPx (kRowPadDip) - height;
    }

    right = LayoutButtons (top, band.right - pad);

    //  An open form's Verify box is before the buttons; a save form's is at
    //  the left of the button row, under the file type list, and extends
    //  to the buttons so a caller's longer label fits.
    if (m_verify != nullptr && m_name == nullptr)
    {
        int  width = scaler.ToPx (kVerifyDip);

        m_verify->Layout (RECT { right - width, top, right, top + height }, scaler);
    }
    else if (m_verify != nullptr)
    {
        int  left = GetSaveBoxLeft (band, listLeft);

        m_verify->Layout (RECT { left, top, (std::max) (left, right), top + height }, scaler);
    }

}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::LayoutButtons
//
//  Cancel, the confirm button, the skip button and the create command,
//  right to left from `right`. Returns the left edge of the last one placed,
//  less a gap.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerPickerBar::LayoutButtons (int top, int right)
{
    const DxuiDpiScaler &  scaler = m_context.scaler;
    int                    height = scaler.ToPx (kButtonDip);
    int                    gap    = scaler.ToPx (kGapDip);



    for (DxuiButton * button : { m_cancel, m_confirm, m_skip, m_create })
    {
        int  width = 0;



        if (button == nullptr)
        {
            continue;
        }

        width = scaler.ToPx (DxuiButtonRow::GetWidthForLabel (button->GetAccessibleName()));

        button->Layout (RECT { right - width, top, right, top + height }, scaler);
        right -= width + gap;
    }

    return right;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::GetSaveBoxLeft
//
//  The list's left edge, or far enough right for the labels when the tree
//  is too narrow for them.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerPickerBar::GetSaveBoxLeft (const RECT & band, int listLeft) const
{
    const DxuiDpiScaler &  scaler = m_context.scaler;



    return (std::max) (listLeft, (int) band.left + scaler.ToPx (kEdgePadDip + kSaveLabelDip + kGapDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::LayoutSaveRows
//
//  The file name row, then the file type row with the type's note under it.
//  Each label is right-aligned just before its box.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerBar::LayoutSaveRows (const RECT & band, int listLeft)
{
    const DxuiDpiScaler &  scaler = m_context.scaler;
    int                    height = scaler.ToPx (kButtonDip);
    int                    gap    = scaler.ToPx (kGapDip);
    int                    left   = GetSaveBoxLeft (band, listLeft);
    int                    right  = band.right - scaler.ToPx (kEdgePadDip);
    int                    label  = left - gap - scaler.ToPx (kSaveLabelDip);
    int                    top    = band.top + scaler.ToPx (kRowPadDip);
    int                    width  = 0;



    if (!m_isNameConfigured && m_context.window.GetHwnd() != nullptr)
    {
        m_name->SetTextRenderer   (m_context.window.GetTextRenderer());
        m_name->SetHwnd           (m_context.window.GetHwnd());
        m_name->SetFont           (kDialogFace, kDialogFontDip);
        m_name->SetExplorerChrome (false);

        m_isNameConfigured = true;
    }

    m_nameLabel->Layout (RECT { label, top, left - gap, top + height }, scaler);
    m_name->Layout      (RECT { left, top, (std::max) (left + scaler.ToPx (kNameMinDip), right), top + height }, scaler);

    if (m_filter == nullptr)
    {
        return;
    }

    top += height + gap;

    //  As wide as the name box, as in the standard dialog.
    width = (std::max) (scaler.ToPx (kNameMinDip), right - left);

    m_filterLabel->Layout (RECT { label, top, left - gap, top + height }, scaler);
    m_filter->Layout      (RECT { left, top, left + width, top + height }, scaler);

    if (m_filterNote != nullptr)
    {
        int  inset = scaler.ToPx (DxuiComboBox::kTextInsetDip);

        m_filterNote->Layout (RECT { left + inset, top + height + gap / 2, right, top + height + scaler.ToPx (kNoteDip) }, scaler);
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
        auto  row = std::find (m_filterRows.begin(), m_filterRows.end(), (int) m_context.pickerMode.GetActiveFilter());

        m_filter->SetSelected ((row != m_filterRows.end()) ? (int) (row - m_filterRows.begin()) : -1);
    }

    if (m_filterNote != nullptr)
    {
        size_t  active = m_context.pickerMode.GetActiveFilter();

        m_filterNote->SetText ((active < m_context.app.picker->filters.size()) ? m_context.app.picker->filters[active].note : std::wstring());
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
//  ExplorerPickerBar::GetButtons
//
//  Left to right, as Layout places them from the right edge in reverse.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiButton *> ExplorerPickerBar::GetButtons() const
{
    std::vector<DxuiButton *>  buttons;



    for (DxuiButton * button : { m_create, m_skip, m_confirm, m_cancel })
    {
        if (button != nullptr)
        {
            buttons.push_back (button);
        }
    }

    return buttons;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::GetButtonCount
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerPickerBar::GetButtonCount() const
{
    return (int) GetButtons().size() + GetLeadStops();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::IsButtonEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerBar::IsButtonEnabled (int index) const
{
    std::vector<DxuiButton *>  buttons = GetButtons();



    if (index >= 0 && index < GetLeadStops())
    {
        return true;
    }

    index -= GetLeadStops();

    return index >= 0 && index < (int) buttons.size() && buttons[(size_t) index]->IsEnabled() && buttons[(size_t) index]->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::SetButtonFocus
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerBar::SetButtonFocus (int index)
{
    std::vector<DxuiButton *>  buttons = GetButtons();



    if (m_verify != nullptr)
    {
        m_verify->SetFocused (index == GetVerifyStop());
    }

    if (m_filter != nullptr)
    {
        m_filter->SetFocused (index == GetFilterStop());
    }

    for (size_t i = 0; i < buttons.size(); i++)
    {
        buttons[i]->SetFocused ((int) i == index - GetLeadStops());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::HandleButtonKey
//
//  Enter or Space presses the focused button or checks the box, and the
//  filter list takes the keys a list does.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerBar::HandleButtonKey (int index, const DxuiKeyEvent & ev)
{
    bool  handled = false;



    if (index >= 0 && index == GetFilterStop() && ev.kind == DxuiKeyEventKind::Down && ev.vk != VK_TAB)
    {
        handled = m_filter->HandleKey (ev.vk);
        m_context.window.Invalidate();
        return handled;
    }

    if (!IsButtonEnabled (index) || ev.kind != DxuiKeyEventKind::Down || (ev.vk != VK_RETURN && ev.vk != VK_SPACE))
    {
        return false;
    }

    if (index == GetVerifyStop())
    {
        m_verify->SetChecked (!m_verify->IsChecked());
        m_context.window.Invalidate();
        return true;
    }

    GetButtons()[(size_t) (index - GetLeadStops())]->Click();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::FillFilterList
//
//  The filter entries, each under the heading that starts its group, with
//  its note under it in the open list, as Casso Explorer's Options list
//  image types.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerBar::FillFilterList()
{
    const std::vector<FileDialogFilter> &  filters = m_context.app.picker->filters;
    std::vector<std::wstring>              rows;
    std::vector<bool>                      headings;



    m_filterRows.clear();

    for (size_t index = 0; index < filters.size(); index++)
    {
        const FileDialogFilter &  filter = filters[index];

        if (!filter.heading.empty())
        {
            rows.push_back (filter.heading);
            headings.push_back (true);
            m_filterRows.push_back (-1);
        }

        //  Entries with notes are listed by name alone, and the selected
        //  entry's note is shown under the list.
        rows.push_back (HasFilterNotes() ? filter.name : m_context.pickerMode.GetFilterLabel (index));
        headings.push_back (false);
        m_filterRows.push_back ((int) index);
    }

    m_filter->SetItems            (rows);
    m_filter->SetItemHeadings     (headings);
    m_filter->SetSelect           ([this] (int row)
    {
        if (row >= 0 && row < (int) m_filterRows.size() && m_filterRows[(size_t) row] >= 0)
        {
            m_context.pickerMode.SetActiveFilter ((size_t) m_filterRows[(size_t) row]);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar::HasFilterNotes
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerBar::HasFilterNotes() const
{
    const PickerRequest *  request = m_context.app.picker;



    return request != nullptr && request->IsSave() && request->filters.size() > 1
        && std::any_of (request->filters.begin(), request->filters.end(), [] (const FileDialogFilter & filter) { return !filter.note.empty(); });
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

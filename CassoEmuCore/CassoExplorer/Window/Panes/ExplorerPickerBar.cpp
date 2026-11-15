#include "Pch.h"

#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerPickerMode.h"
#include "CassoExplorer/Window/Panes/ExplorerPickerBar.h"





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

    if (m_context.pickerMode.GetFilterCount() > 1)
    {
        m_filter = m_context.window.CreateChild<DxuiButton> (std::wstring());
        m_filter->SetElide   (DxuiElide::Tail);
        m_filter->SetOnClick ([this] () { ShowFilterMenu(); });
    }

    if (request->onCreateNew)
    {
        m_create = m_context.window.CreateChild<DxuiButton> ((request->receivingDrive.has_value() && request->receivingDrive->kind == ReceivingDriveKind::Recorder) ? L"New blank tape" : L"Create new disk...");
        m_create->SetOnClick ([this] () { m_context.pickerMode.CreateNew(); });
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
//  Cancel and the confirm button at the right, the create command and the
//  filter choice before them, and the header in what is left.
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

    for (DxuiButton * button : { m_cancel, m_confirm, m_create, m_filter })
    {
        if (button == nullptr)
        {
            continue;
        }

        width = scaler.ToPx ((button == m_filter) ? kFilterMaxDip : DxuiButtonRow::GetWidthForLabel (button->GetAccessibleName()));

        button->Layout (RECT { right - width, top, right, top + height }, scaler);
        right -= width + gap;
    }

    m_header->Layout (RECT { band.left + scaler.ToPx (kEdgePadDip), band.top, (std::max) (band.left, (LONG) right), band.bottom }, scaler);
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

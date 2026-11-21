#include "Pch.h"

#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Managers/ExplorerPickerMode.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Panes/ExplorerPickerBar.h"
#include "CassoExplorer/Window/Panes/ExplorerPickerSearch.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch::ExplorerPickerSearch
//
////////////////////////////////////////////////////////////////////////////////

ExplorerPickerSearch::ExplorerPickerSearch (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch::Create
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerSearch::Create()
{
    //  A save form has the window's usual search box beside the address
    //  bar instead.
    if (!m_context.IsPicker() || m_context.app.picker->IsSave())
    {
        return;
    }

    m_box = m_context.window.CreateChild<DxuiTextInput>();

    m_context.window.SetMouseHandler (m_box, [this] (const DxuiMouseEvent & ev) { return HandleMouse (ev); });

    m_box->SetPlaceholder (L"Search by name");
    m_box->SetOnChange    ([this] (const std::wstring & text) { ApplyFilter (text); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch::Layout
//
////////////////////////////////////////////////////////////////////////////////

RECT ExplorerPickerSearch::Layout (const RECT & rect)
{
    const DxuiDpiScaler &  scaler = m_context.scaler;
    RECT                   below  = rect;
    int                    band   = scaler.ToPx (kBandDip);
    int                    height = scaler.ToPx (kBoxDip);
    int                    pad    = scaler.ToPx (kEdgePadDip);
    int                    top    = 0;



    if (m_box == nullptr)
    {
        return rect;
    }

    if (!m_isConfigured && m_context.window.GetHwnd() != nullptr)
    {
        m_box->SetTextRenderer   (m_context.window.GetTextRenderer());
        m_box->SetHwnd           (m_context.window.GetHwnd());
        m_box->SetFont           (DxuiAddressBar::kVariableTextFace, DxuiAddressBar::kFontDip);
        m_box->SetExplorerChrome (true);
        m_box->SetTrailingGlyph  (s_kpszMdl2Search, DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace) ? DxuiToolbar::kFluentIconFace : DxuiToolbar::kMdl2IconFace);
        m_box->SetClearGlyph     (s_kpszMdl2Cancel);

        m_isConfigured = true;
    }

    band        = (std::min) (band, (int) (rect.bottom - rect.top));
    top         = rect.top + (band - height) / 2;
    below.top   = rect.top + band;

    m_box->Layout (RECT { rect.left + pad, top, (std::max) (rect.left + pad, (LONG) (rect.right - pad)), top + height }, scaler);

    return below;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch::Focus
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerSearch::Focus()
{
    if (m_box == nullptr)
    {
        return;
    }

    m_context.input.SetFocusPane (ExplorerPane::PickerSearch);
    m_box->SetSelection (0, m_box->GetText().size());
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch::GetText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerPickerSearch::GetText() const
{
    return (m_box != nullptr) ? m_box->GetText() : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch::HandleMouse
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerSearch::HandleMouse (const DxuiMouseEvent & ev)
{
    if (m_box == nullptr)
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_context.input.SetFocusPane (ExplorerPane::PickerSearch);
    }

    m_box->OnMouse (ev);
    m_context.window.Invalidate();

    return ev.kind != DxuiMouseEventKind::Leave;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch::HandleKey
//
//  Down moves to the list's first row. Enter inserts the only match, or
//  moves to the list when there are several. Escape empties the box, and
//  closes the picker when it is empty.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerSearch::HandleKey (const DxuiKeyEvent & ev)
{
    bool  isDown = ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt;



    if (m_box == nullptr)
    {
        return false;
    }

    if (isDown && ev.vk == VK_DOWN)
    {
        MoveToList();
        return true;
    }

    if (isDown && ev.vk == VK_RETURN)
    {
        if (m_context.browser.GetRows().size() == 1)
        {
            m_context.listPane.SetListSelection ({ 0 });
            m_context.pickerMode.Confirm();
        }
        else
        {
            MoveToList();
        }

        return true;
    }

    if (isDown && ev.vk == VK_ESCAPE && !m_box->GetText().empty())
    {
        m_box->SetText (L"");
        ApplyFilter (L"");
        return true;
    }

    return m_box->OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch::HandleFocusChanged
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerSearch::HandleFocusChanged (bool isFocused)
{
    if (m_box != nullptr)
    {
        m_box->SetFocused (isFocused);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch::IsCaretOn
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerSearch::IsCaretOn() const
{
    return m_box != nullptr && m_box->IsCaretOn();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch::ApplyFilter
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerSearch::ApplyFilter (const std::wstring & text)
{
    m_context.browser.SetNameFilter (text);
    m_context.listPane.FillList();
    m_context.pickerBar.Update();
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerSearch::MoveToList
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerSearch::MoveToList()
{
    if (m_context.browser.GetRows().empty())
    {
        return;
    }

    m_context.input.SetFocusPane (ExplorerPane::List);

    if (!m_context.listPane.HasSelection())
    {
        m_context.listPane.SetListSelection ({ 0 });
    }
}

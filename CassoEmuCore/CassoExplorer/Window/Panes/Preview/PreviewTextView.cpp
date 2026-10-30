#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewTextView.h"
#include "CassoExplorer/Model/PreviewText.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Panes/ExplorerPane.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerCommands.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::PreviewTextView
//
////////////////////////////////////////////////////////////////////////////////

PreviewTextView::PreviewTextView (ExplorerPreviewManager & manager, ExplorerContext & context)
    : PreviewView (manager, context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::Create
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::Create()
{
    m_text = m_context.window.CreateChild<DxuiTextView>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::Configure
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::Configure()
{
    m_text->SetOwnerWindow   (m_context.window.GetHwnd());
    m_text->SetIconFace      (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                              ? DxuiToolbar::kFluentIconFace
                              : DxuiToolbar::kMdl2IconFace);
    m_text->SetOnContextMenu ([this] (POINT at) { ShowContextMenu (at.x, at.y); });
    ExplorerTextStyles::Apply (*m_text, ExplorerTextStyles::kPreviewText);
    m_text->SetZoom          ((float) m_context.prefs.previewZoom / 100.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::Layout (const RECT & content)
{
    m_text->Layout (content, m_context.scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::Hide
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::Hide()
{
    m_text->SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::UpdateScrollbarHover
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewTextView::UpdateScrollbarHover (POINT at)
{
    return m_text->SetScrollbarHover (at);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewTextView::TickScrollbars (int64_t nowMs)
{
    return m_text->TickScrollbars (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::Fill
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::Fill (const PreviewContent & preview)
{
    //  An image's details are metadata, which reads better in the body face
    //  than in the fixed-width face an Apple's text is drawn in.
    m_text->SetProportional (preview.kind == PreviewContent::Kind::Details);
    m_text->SetRows (PreviewText::BuildTextRows (preview, m_context.prefs.lineAddresses, PreviewText::GetSourceColors (m_context.theme)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::SetVisible
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::SetVisible (bool visible)
{
    m_text->SetVisible (visible);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::SetZoom
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::SetZoom (float zoom)
{
    m_text->SetZoom (zoom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::IsShowing
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewTextView::IsShowing() const
{
    return m_context.prefs.previewVisible && m_text->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::HandleCapturedMouse
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewTextView::HandleCapturedMouse (const DxuiMouseEvent & ev)
{
    if (!m_text->IsInteracting())
    {
        return false;
    }

    m_text->OnMouse (ev);
    m_context.statusBar.Fill();
    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::HandleMouse
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewTextView::HandleMouse (const DxuiMouseEvent & ev)
{
    if (!m_text->IsVisible() || !ExplorerLayout::Contains (m_text->GetBounds(), ev.positionDip))
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
    }

    m_text->OnMouse (ev);
    m_context.statusBar.Fill();
    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::GetCursorForPoint
//
//  The text selects as text does, so the cursor over it is the I-beam,
//  except over its scrollbars.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR PreviewTextView::GetCursorForPoint (POINT clientPx) const
{
    bool  over = m_text->IsVisible() && ExplorerLayout::Contains (m_text->GetBounds(), clientPx) && !m_text->IsOverScrollbar (clientPx);



    return over ? IDC_IBEAM : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::TryGetScrollbarMenu
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewTextView::TryGetScrollbarMenu (const DxuiMouseEvent & ev, std::vector<DxuiPopupMenuItem> & outItems) const
{
    if (!m_text->IsVisible() || !ExplorerLayout::Contains (m_text->GetBounds(), ev.positionDip))
    {
        return false;
    }

    outItems = m_text->GetScrollbarMenu (ev.positionDip);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::TryGetSelectionDetail
//
//  Characters selected, as the status bar's detail.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewTextView::TryGetSelectionDetail (std::wstring & outDetail) const
{
    size_t  chars = 0;



    if (!IsShowing() || !m_text->HasSelection())
    {
        return false;
    }

    for (wchar_t ch : m_text->GetSelectionText())
    {
        chars += (ch != L'\r' && ch != L'\n') ? 1 : 0;
    }

    outDetail = (chars == 1) ? std::wstring (L"1 character selected") : std::format (L"{} characters selected", chars);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::ToggleLineAddresses
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::ToggleLineAddresses()
{
    m_context.prefs.lineAddresses = !m_context.prefs.lineAddresses;

    Fill (m_context.browser.GetPreview());
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::ShowContextMenu
//
//  Copy and Select all, which reach the text view as the focused control.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::ShowContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    for (int id : { (int) CassoExplorerCommands::kCopy, (int) CassoExplorerCommands::kSelectAll })
    {
        std::shared_ptr<const DxuiCommand>  command = m_context.commands.Find (id);

        if (command != nullptr)
        {
            items.push_back (DxuiPopupMenuItem::ForCommand (command));
        }
    }

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}

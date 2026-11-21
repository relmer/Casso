#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewTextView.h"
#include "CassoExplorer/Model/PreviewText.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Window/Managers/ExplorerTips.h"
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

    m_context.window.SetMouseHandler (m_text, [this] (const DxuiMouseEvent & ev) { return HandleTextMouse (ev); });
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
    const PreviewContent  & preview = m_context.browser.GetPreview();
    bool                    tables  = preview.kind == PreviewContent::Kind::Document
                                   && std::any_of (preview.paragraphs.begin(), preview.paragraphs.end(), [] (const Paragraph & p) { return p.table != nullptr; });



    m_text->Layout (content, m_context.scaler);

    //  A document's tables change form with the width, so its rows are built
    //  again when the width changes.
    if (tables && m_text->IsVisible() && content.right - content.left != m_tableWidth)
    {
        FillRows (preview);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::IsWide
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewTextView::IsWide() const
{
    RECT  bounds = m_text->GetBounds();



    return (bounds.right - bounds.left) >= m_context.scaler.ToPx (s_kWideTableDip);
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
    m_text->SetProportional (preview.kind == PreviewContent::Kind::Details || preview.kind == PreviewContent::Kind::Document);

    //  A catalog is a table, so its rows stay whole and scroll sideways.
    m_text->SetNoWrap (preview.kind == PreviewContent::Kind::Catalog);
    FillRows (preview);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::FillRows
//
//  A document's record tables as tables where the pane is wide enough and
//  every column fits at its natural width, and as cards where not. The rows
//  are measured as tables, and built as cards when a cell would wrap.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::FillRows (const PreviewContent & preview)
{
    SourceSyntax::Colors  colors = PreviewText::GetSourceColors (m_context.theme);
    RECT                  bounds = m_text->GetBounds();



    m_wide       = IsWide();
    m_tableWidth = bounds.right - bounds.left;
    m_rowParagraph.clear();

    if (m_crossReference)
    {
        m_text->SetRows (PreviewText::BuildCrossReferenceRows (preview, m_context.prefs.lineAddresses, colors));
        return;
    }

    m_text->SetRows (PreviewText::BuildTextRows (preview, m_context.prefs.lineAddresses, colors, m_wide, &m_rowParagraph));

    if (m_wide && m_context.theme != nullptr && m_context.window.GetTextRenderer() != nullptr)
    {
        m_text->Measure (*m_context.window.GetTextRenderer(), *m_context.theme);

        if (!m_text->DoColumnsFit())
        {
            m_wide = false;
            m_rowParagraph.clear();
            m_text->SetRows (PreviewText::BuildTextRows (preview, m_context.prefs.lineAddresses, colors, false, &m_rowParagraph));
        }
    }
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
    return m_context.preview.IsShown() && m_text->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::HandleTextMouse
//
//  The text's mouse handler. A press moves focus to the preview; the status
//  bar follows the selection.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewTextView::HandleTextMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
    }

    handled = m_text->OnMouse (ev);

    if (ev.kind != DxuiMouseEventKind::Move || m_text->IsInteracting())
    {
        m_context.statusBar.Fill();
    }

    return handled || ev.kind != DxuiMouseEventKind::Move;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::UpdatePictureTipAt
//
//  The tip is anchored on the pointer. Each line has a swatch of the color
//  the picture draws that thing in.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::UpdatePictureTipAt (const DxuiMouseEvent & ev)
{
    const PreviewContent            & preview = m_context.browser.GetPreview();
    const PictureTip                * tip     = nullptr;
    int                               row     = 0;
    int                               x       = 0;
    int                               y       = 0;
    std::wstring                      text;
    std::vector<DxuiTooltipSwatch>    swatches;
    RECT                              cell    = {};
    bool                              over    = m_text->IsVisible() && ev.kind == DxuiMouseEventKind::Move && preview.kind == PreviewContent::Kind::Document
                                             && ExplorerLayout::Contains (m_text->GetBounds(), ev.positionDip);



    if (over && m_text->HitTestPicture (ev.positionDip, row, x, y) && row < (int) m_rowParagraph.size() && m_rowParagraph[(size_t) row] < (int) preview.paragraphs.size())
    {
        const Paragraph &  paragraph = preview.paragraphs[(size_t) m_rowParagraph[(size_t) row]];

        tip = paragraph.GetTipAt (x, y);

        if (tip != nullptr)
        {
            cell.left   = paragraph.tipLeft + (x - paragraph.tipLeft) / paragraph.tipCell * paragraph.tipCell;
            cell.top    = paragraph.tipTop  + (y - paragraph.tipTop)  / paragraph.tipCell * paragraph.tipCell;
            cell.right  = cell.left + paragraph.tipCell;
            cell.bottom = cell.top  + paragraph.tipCell;
        }
    }

    //  The cell under the pointer is outlined, as the inspector and the
    //  debugger's heat map outline theirs.
    if (m_text->SetPictureOutline (tip != nullptr ? row : -1, cell))
    {
        m_context.window.Invalidate();
    }

    if (tip == nullptr || tip->lines.empty())
    {
        m_context.tips.HideHoverTip (ExplorerTips::TipOwner::Preview);
        return;
    }

    for (size_t i = 0; i < tip->lines.size(); i++)
    {
        if (tip->lines[i].first != 0)
        {
            swatches.push_back (DxuiTooltipSwatch { (int) i, tip->lines[i].first });
        }

        text += (i > 0 ? L"\n" : L"") + tip->lines[i].second;
    }

    m_context.tips.ShowSwatchTip (ExplorerTips::TipOwner::Preview, RECT { ev.positionDip.x, ev.positionDip.y, ev.positionDip.x + 1, ev.positionDip.y + 1 }, text, swatches);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::GetCursorForPoint
//
//  The text selects as text does, so the cursor over it is the I-beam;
//  over its scrollbars and its pictures, such as a map, it is the arrow.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR PreviewTextView::GetCursorForPoint (POINT clientPx) const
{
    bool  inside  = m_text->IsVisible() && ExplorerLayout::Contains (m_text->GetBounds(), clientPx);
    int   row     = 0;
    int   x       = 0;
    int   y       = 0;
    bool  picture = inside && m_text->HitTestPicture (clientPx, row, x, y);



    return !inside ? nullptr : (picture || m_text->IsOverScrollbar (clientPx)) ? IDC_ARROW : IDC_IBEAM;
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
//  PreviewTextView::ToggleCrossReference
//
//  The choice lasts until the window closes, and applies to each BASIC
//  program previewed.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::ToggleCrossReference()
{
    m_crossReference = !m_crossReference;

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

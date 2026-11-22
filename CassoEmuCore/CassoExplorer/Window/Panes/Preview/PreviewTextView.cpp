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
        FillRows (preview, true);
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
    return m_text->GetScroll().SetScrollbarHover (at);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::IsPinging
//
//  Whether the text shows a picture with a ping, such as a saved game's map
//  with the party marked, which animates on the window's tick.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewTextView::IsPinging() const
{
    return IsShowing() && m_text->GetPictures().IsPinging();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewTextView::TickScrollbars (int64_t nowMs)
{
    bool  moved  = m_text->GetScroll().TickScrollbars (nowMs);
    bool  pinged = m_text->GetPictures().TickPing (nowMs);



    return moved || pinged;
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
//  PreviewTextView::GetLegendEntryCount
//
//  The most entries any legend in a document has, or 0 for none.
//
////////////////////////////////////////////////////////////////////////////////

int PreviewTextView::GetLegendEntryCount (const PreviewContent & preview)
{
    int  most = 0;



    if (preview.kind != PreviewContent::Kind::Document)
    {
        return 0;
    }

    for (const Paragraph & paragraph : preview.paragraphs)
    {
        if (paragraph.isLegend)
        {
            most = (std::max) (most, (int) paragraph.runs.size());
        }
    }

    return most;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::FillRows
//
//  A document's record tables in the widest form whose columns all fit at
//  their natural width: as tables where the pane is wide enough, then as
//  fields four, three and two to a line, then as cards. Each form is
//  measured, and the next narrower one built when a cell would wrap.
//  Without a renderer to measure with, a table or cards by the width alone.
//  When the same preview is laid out again, it is scrolled to the same
//  place in the paragraph that was at the top.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::FillRows (const PreviewContent & preview, bool keepPlace)
{
    SourceSyntax::Colors  colors    = PreviewText::GetSourceColors (m_context.theme);
    RECT                  bounds    = m_text->GetBounds();
    bool                  measure   = m_context.theme != nullptr && m_context.window.GetTextRenderer() != nullptr;
    TableForm             form      = IsWide() ? TableForm::Table : measure ? TableForm::FourFields : TableForm::Cards;
    bool                  done      = false;
    bool                  fits      = false;
    int                   paragraph = -1;
    int                   legend    = GetLegendEntryCount (preview);
    float                 within    = 0.0f;



    if (keepPlace)
    {
        GetPlace (paragraph, within);
    }

    m_tableWidth = bounds.right - bounds.left;

    if (m_crossReference)
    {
        m_rowParagraph.clear();
        m_text->SetRows (PreviewText::BuildCrossReferenceRows (preview, m_context.prefs.lineAddresses, colors));
        return;
    }

    while (!done)
    {
        m_rowParagraph.clear();
        m_text->SetRows (PreviewText::BuildTextRows (preview, m_context.prefs.lineAddresses, colors, form, &m_rowParagraph, 1));

        done = form == TableForm::Cards || !measure;

        if (!done)
        {
            m_text->Measure (*m_context.window.GetTextRenderer(), *m_context.theme);

            done = m_text->DoColumnsFit();
            form = done ? form : (TableForm) ((int) form - 1);
        }
    }

    //  A legend takes as many columns as fit with each entry whole: one an
    //  entry, then one fewer until its rows fit, and one column when none
    //  does. Its columns are apart from the table's, so the form stays.
    for (int columns = legend; measure && !fits && columns > 1; columns--)
    {
        m_rowParagraph.clear();
        m_text->SetRows (PreviewText::BuildTextRows (preview, m_context.prefs.lineAddresses, colors, form, &m_rowParagraph, columns));
        m_text->Measure (*m_context.window.GetTextRenderer(), *m_context.theme);

        fits = m_text->DoColumnsFit();
    }

    if (legend > 1 && measure && !fits)
    {
        m_rowParagraph.clear();
        m_text->SetRows (PreviewText::BuildTextRows (preview, m_context.prefs.lineAddresses, colors, form, &m_rowParagraph, 1));
    }

    if (paragraph >= 0 && measure)
    {
        m_text->Measure (*m_context.window.GetTextRenderer(), *m_context.theme);
        SetPlace (paragraph, within);
    }

    //  A map's ping animates on the window's tick, which otherwise waits for
    //  input to start it.
    if (m_text->GetPictures().IsPinging())
    {
        m_context.window.ArmTick();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::GetPlace
//
//  The paragraph of the top line, and how far into its lines the top line
//  is, from 0 at its first line to 1 past its last. No paragraph at the
//  top of the text or when the rows have no paragraphs.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::GetPlace (int & outParagraph, float & outWithin) const
{
    int  top   = m_text->GetScroll().GetTopLine();
    int  row   = m_text->GetLayout().GetLineRow (top);
    int  first = top;
    int  last  = top;



    outParagraph = -1;
    outWithin    = 0.0f;

    if (top <= 0 || row < 0 || row >= (int) m_rowParagraph.size())
    {
        return;
    }

    outParagraph = m_rowParagraph[(size_t) row];

    while (first > 0 && GetLineParagraph (first - 1) == outParagraph)
    {
        first--;
    }

    while (last + 1 < m_text->GetLayout().GetLineCount() && GetLineParagraph (last + 1) == outParagraph)
    {
        last++;
    }

    outWithin = (float) (top - first) / (float) (last - first + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::SetPlace
//
//  The top line at the same fraction into the paragraph's lines.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewTextView::SetPlace (int paragraph, float within)
{
    int  first = -1;
    int  count = 0;



    for (int line = 0; line < m_text->GetLayout().GetLineCount(); line++)
    {
        if (GetLineParagraph (line) == paragraph)
        {
            first = (first < 0) ? line : first;
            count++;
        }
    }

    if (first >= 0)
    {
        m_text->GetScroll().SetTopLine (first + (int) (within * (float) count));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewTextView::GetLineParagraph
//
////////////////////////////////////////////////////////////////////////////////

int PreviewTextView::GetLineParagraph (int line) const
{
    int  row = m_text->GetLayout().GetLineRow (line);



    return (row >= 0 && row < (int) m_rowParagraph.size()) ? m_rowParagraph[(size_t) row] : -1;
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



    if (over && m_text->GetPictures().HitTest (ev.positionDip, row, x, y) && row < (int) m_rowParagraph.size() && m_rowParagraph[(size_t) row] < (int) preview.paragraphs.size())
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
    if (m_text->GetPictures().SetOutline (tip != nullptr ? row : -1, cell))
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
    bool  picture = inside && m_text->GetPictures().HitTest (clientPx, row, x, y);



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

    outItems = m_text->GetScroll().GetScrollbarMenu (ev.positionDip);
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



    if (!IsShowing() || !m_text->GetSelection().HasSelection())
    {
        return false;
    }

    for (wchar_t ch : m_text->GetSelection().GetSelectionText())
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

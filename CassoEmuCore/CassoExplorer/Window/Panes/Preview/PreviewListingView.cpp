#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewListingView.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::PreviewListingView
//
////////////////////////////////////////////////////////////////////////////////

PreviewListingView::PreviewListingView (ExplorerPreviewManager & manager, ExplorerContext & context)
    : PreviewView (manager, context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::Create
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::Create()
{
    CassoExplorerNamedControl<DxuiListView>  * list = m_context.window.CreateChild<CassoExplorerNamedControl<DxuiListView>>();



    list->SetAccessibleName (L"Preview");
    m_list = list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::Configure
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::Configure()
{
    m_list->SetShowHeader (false);
    m_list->SetColumns ({ DxuiListView::Column { L"", 0, true } });

    //  A hex dump and a disassembly are columns of fixed-width text, and read
    //  as such only in a monospace face on rows close to the line height.
    m_list->SetMonospace (true);
    m_list->SetRowHeightDip (kRowHeightDip);
    m_list->SetHorizontalScrollEnabled (true);
    m_list->SetPreciseAutoFit (true);
    m_list->SetMultiSelect (true);
    m_list->SetAlwaysShowSelection (true);
    m_list->SetTextSelectionColors (true);
    m_list->SetOwnerWindow (m_context.window.GetHwnd());

    //  An archive's files open on a double-click, as the file list's do.
    m_list->SetActivateOnDoubleClick (true);
    m_list->SetOnActivateRow ([this] (int row)
    {
        if (m_isArchive)
        {
            m_archive.OpenRow (row);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::Layout (const RECT & content)
{
    m_list->Layout (content, m_context.scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::Hide
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::Hide()
{
    m_list->SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::UpdateScrollbarHover
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewListingView::UpdateScrollbarHover (POINT at)
{
    return m_list->SetScrollbarHover (at);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::TickScrollbars
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewListingView::TickScrollbars (int64_t nowMs)
{
    return m_list->TickScrollbars (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::Fill
//
//  A catalog of any volume but DOS 3.3 or ProDOS arrives as rows under
//  sortable columns. An archive's rows look as the file list would show them
//  inside it. Any other preview leaves the list empty.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::Fill (const PreviewContent & preview, bool columns)
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;



    if (columns)
    {
        m_list->SetShowHeader      (true);
        m_list->SetMonospace       (!preview.archive);
        m_list->SetExplorerDetails (preview.archive);

        //  An archive's rows move and extend by the keys the file list takes.
        m_list->SetKeyboardColumnNav (preview.archive);
        m_list->SetIconColumn      (preview.archive ? 0 : -1);
        ExplorerTextStyles::Apply (*m_list, preview.archive ? ExplorerTextStyles::kProse : ExplorerTextStyles::kListing);
        m_list->SetRowHeightDip    (kRowHeightDip);
        m_list->SetRowHeightPxFn   (preview.archive ? std::function<int (UINT)> (&ExplorerLayout::GetListRowHeightPx) : nullptr);
        m_list->SetColumns         (preview.archive ? BrowserPreviewer::GetArchivePreviewColumns() : BrowserCells::GetCatalogPreviewColumns());
        m_list->SetTextSelectionColors (!preview.archive);

        for (const CatalogRow & row : preview.rows)
        {
            rows.push_back (preview.archive ? m_context.browser.GetArchivePreviewCells (row) : BrowserCells::ToCatalogPreviewCells (row));
        }
    }

    m_isArchive = columns && preview.archive;
    m_list->SetRows (std::move (rows));
    m_list->ResetAutoFit();
    m_list->UpdateAutoFitFromRows();
    m_list->SetTopRow (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::SetVisible
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::SetVisible (bool visible)
{
    m_list->SetVisible (visible);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::HandleCapturedMouse
//
//  A drag selecting lines keeps the pointer until the button comes up, as
//  the file list's does.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewListingView::HandleCapturedMouse (const DxuiMouseEvent & ev)
{
    if (!m_list->IsInteracting())
    {
        return false;
    }

    m_list->OnMouse (ExplorerLayout::ToLocal (ev, m_list->GetBounds()));
    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::HandleMouse
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewListingView::HandleMouse (const DxuiMouseEvent & ev)
{
    if (!m_list->IsVisible() || !ExplorerLayout::Contains (m_list->GetBounds(), ev.positionDip))
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
    }

    m_list->OnMouse (ExplorerLayout::ToLocal (ev, m_list->GetBounds()));
    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::HandleShortcutKey
//
//  An archive's files copy and cut as files do, for the shell to extract on
//  paste, Ctrl+A selects them all, and Delete deletes them for good, as in
//  the file list.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewListingView::HandleShortcutKey (const DxuiKeyEvent & ev)
{
    if (m_isArchive && ev.ctrl && ev.vk == 'A')
    {
        m_list->SelectAllRows();
        m_context.window.Invalidate();
        return true;
    }

    if (m_isArchive && ev.ctrl && (ev.vk == 'C' || ev.vk == 'X'))
    {
        m_archive.CopySelection (ev.vk == 'X');
        return true;
    }

    if (m_isArchive && !ev.ctrl && ev.vk == VK_DELETE)
    {
        m_archive.DeleteSelection();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::GetCursorForPoint
//
//  The list tests points in its own terms. Over a catalog's lines the cursor
//  is the I-beam, as over text; over an archive's rows it is the arrow, as
//  over the file list's.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR PreviewListingView::GetCursorForPoint (POINT clientPx) const
{
    RECT     list   = m_list->GetBounds();
    POINT    local  = { clientPx.x - list.left, clientPx.y - list.top };
    LPCWSTR  cursor = nullptr;



    if (!m_list->IsVisible() || !ExplorerLayout::Contains (list, clientPx))
    {
        return nullptr;
    }

    cursor = m_list->GetCursorForPoint (local);

    return (cursor != nullptr || m_list->IsOverScrollbar (local)) ? cursor : (m_isArchive ? IDC_ARROW : IDC_IBEAM);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::TryGetScrollbarMenu
//
//  The list takes points relative to its own bounds, as its HandleMouse
//  does.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewListingView::TryGetScrollbarMenu (const DxuiMouseEvent & ev, std::vector<DxuiPopupMenuItem> & outItems) const
{
    if (!m_list->IsVisible() || !ExplorerLayout::Contains (m_list->GetBounds(), ev.positionDip))
    {
        return false;
    }

    outItems = m_list->GetScrollbarMenu (ExplorerLayout::ToLocal (ev, m_list->GetBounds()).positionDip);
    return true;
}

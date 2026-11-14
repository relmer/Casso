#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewListingView.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "CassoExplorer/Model/ApplesoftVariables.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/Browser/BrowserPreviewer.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Panes/ExplorerPane.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "Core/UnicodeSymbols.h"





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
    CassoExplorerNamedControl<DxuiListView>  * list = m_context.window.CreatePaneRoutedChild<CassoExplorerNamedControl<DxuiListView>>();



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
    m_valueTip.SetPopupHost (m_context.window.GetPopupHost());
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

        //  A double-click on an array's row opens or closes it.
        if (m_isVariables)
        {
            ToggleArray (row, !IsArrayOpen (row), false);
        }
    });

    //  A VAR file's variables sort on a click on a column's title.
    m_list->SetOnSortColumn ([this] (int column)
    {
        if (m_isVariables)
        {
            SortVariables (column);
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
    m_valueTip.Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::Hide
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::Hide()
{
    m_list->SetVisible (false);
    m_valueTip.Hide();
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
    bool                                          reset = false;



    if (columns && preview.kind == PreviewContent::Kind::Variables)
    {
        reset         = !m_isVariables || preview.variables.data() != m_varRows || preview.variables.size() != m_varRowCount;
        m_isVariables = true;
        m_isArchive   = false;
        FillVariables (preview, reset);
        return;
    }

    m_isVariables = false;
    m_varRows     = nullptr;
    m_varRowCount = 0;
    m_list->SetGroups        ({});
    m_list->SetSortIndicator (-1, false);

    //  A decoded grid: its headings, and each row's cells as they are.
    if (columns && preview.kind == PreviewContent::Kind::Grid)
    {
        std::vector<DxuiListView::Column>  headings;

        for (const std::wstring & heading : preview.grid.columns)
        {
            headings.push_back (DxuiListView::Column { heading, 0, false, DxuiTextHAlign::Left });
        }

        for (const std::vector<std::wstring> & row : preview.grid.rows)
        {
            std::vector<DxuiListView::Cell>  cells;

            for (size_t at = 0; at < headings.size(); at++)
            {
                cells.push_back (DxuiListView::Cell { (at < row.size()) ? row[at] : std::wstring(), false });
            }

            rows.push_back (std::move (cells));
        }

        m_list->SetShowHeader          (true);
        m_list->SetMonospace           (true);
        m_list->SetExplorerDetails     (false);
        m_list->SetKeyboardColumnNav   (false);
        m_list->SetIconColumn          (-1);
        ExplorerTextStyles::Apply (*m_list, ExplorerTextStyles::kListing);
        m_list->SetRowHeightDip        (kRowHeightDip);
        m_list->SetRowHeightPxFn       (nullptr);
        m_list->SetColumns             (headings);
        m_list->SetTextSelectionColors (true);
        m_isArchive = false;
        m_list->SetRows (std::move (rows));
        m_list->ResetAutoFit();
        m_list->UpdateAutoFitFromRows();
        m_list->SetTopRow (0);
        return;
    }

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
//  PreviewListingView::FillVariables
//
//  Each simple variable is a row. Each array is a row too, its element count
//  in the Length column, with an expand mark before its name; its elements
//  follow it, indented, while it is open. Arrays start closed. While a search
//  filters the rows, every array is open, so each match is in view.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::FillVariables (const PreviewContent & preview, bool reset)
{
    using Column = ApplesoftVariables::Column;

    std::vector<std::vector<DxuiListView::Cell>>  rows;
    std::vector<ApplesoftVariables::Section>      sections;
    std::wstring                                  lower = m_varFilter;

    auto  matchesIn = [&lower] (const std::wstring & text, int offset)
    {
        std::vector<std::pair<int, int>>  ranges;
        std::wstring                      hay = text;

        std::transform (hay.begin(), hay.end(), hay.begin(), [] (wchar_t c) { return (wchar_t) towlower (c); });

        for (size_t at = lower.empty() ? std::wstring::npos : hay.find (lower); at != std::wstring::npos; at = hay.find (lower, at + lower.size()))
        {
            ranges.emplace_back ((int) at + offset, (int) (at + lower.size()) + offset);
        }

        return ranges;
    };



    std::transform (lower.begin(), lower.end(), lower.begin(), [] (wchar_t c) { return (wchar_t) towlower (c); });

    if (reset)
    {
        m_varExpanded.clear();
    }

    m_varRows     = preview.variables.data();
    m_varRowCount = preview.variables.size();
    m_valueTip.Hide();

    m_list->SetShowHeader        (true);
    m_list->SetMonospace         (true);
    m_list->SetExplorerDetails   (false);
    m_list->SetKeyboardColumnNav (true);
    m_list->SetIconColumn        (-1);
    ExplorerTextStyles::Apply    (*m_list, ExplorerTextStyles::kListing);
    m_list->SetRowHeightDip      (kRowHeightDip);
    m_list->SetRowHeightPxFn     (nullptr);
    m_list->SetTextSelectionColors (true);
    m_list->SetColumns           ({ DxuiListView::Column { L"Name",   0, false },
                                    DxuiListView::Column { L"Type",   0, false },
                                    DxuiListView::Column { L"Length", 0, false, DxuiTextRenderer::HAlign::Right },
                                    DxuiListView::Column { L"Value",  0, true } });
    m_list->SetSortIndicator     (m_varColumn, m_varDescending);
    m_list->SetGroups            ({});

    sections = ApplesoftVariables::Arrange (preview.variables, preview.arrays, m_varFilter, (Column) m_varColumn, m_varDescending);

    m_varLines.clear();

    for (const ApplesoftVariables::Section & section : sections)
    {
        if (section.array < 0)
        {
            for (size_t index : section.rows)
            {
                const VariableRow &  row = preview.variables[index];

                rows.push_back ({ { s_kpszVarIndent + row.name, false, matchesIn (row.name, (int) wcslen (s_kpszVarIndent)) },
                                  { row.type,   true },
                                  { row.length, true },
                                  { row.value,  false, matchesIn (row.value, 0) } });
                m_varLines.push_back ({ -1, false, row.name, row.type, row.length, row.value });
            }

            continue;
        }

        const VariableArray &  array = preview.arrays[(size_t) section.array];
        std::wstring           title = array.name + array.dimensions;
        std::wstring           type  = preview.variables[section.rows.front()].type;
        bool                   open  = !m_varFilter.empty() || m_varExpanded.contains (array.name);

        rows.push_back ({ { std::wstring (open ? s_kpszSmallTriangleDown : s_kpszSmallTriangleRight) + L" " + title, false, matchesIn (title, 2) },
                          { type,                           true },
                          { std::to_wstring (array.count),  true },
                          { L"" } });
        m_varLines.push_back ({ section.array, true, title, type, std::to_wstring (array.count), L"" });

        for (size_t index : open ? section.rows : std::vector<size_t>())
        {
            const VariableRow &  row = preview.variables[index];

            rows.push_back ({ { s_kpszElementIndent + row.name, false, matchesIn (row.name, (int) wcslen (s_kpszElementIndent)) },
                              { row.type,   true },
                              { row.length, true },
                              { row.value,  false, matchesIn (row.value, 0) } });
            m_varLines.push_back ({ section.array, false, row.name, row.type, row.length, row.value });
        }
    }

    m_list->SetRows (std::move (rows));
    m_list->ResetAutoFit();
    m_list->UpdateAutoFitFromRows();

    if (reset)
    {
        m_list->SetTopRow (0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::ToggleArray
//
//  Opens or closes the array on a row, or the array an element row is in
//  when `toParent` is set, then selects that array's row. Returns
//  whether the row was an array's.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewListingView::ToggleArray (int row, bool open, bool toParent)
{
    HRESULT        hr     = S_OK;
    bool           result = false;
    int            array  = -1;
    std::wstring   name;



    BAIL_OUT_IF (row < 0 || row >= (int) m_varLines.size(), S_OK);
    BAIL_OUT_IF (!m_varLines[(size_t) row].header && !toParent, S_OK);

    array = m_varLines[(size_t) row].array;
    BAIL_OUT_IF (array < 0, S_OK);

    name   = m_context.browser.GetPreview().arrays[(size_t) array].name;
    result = true;

    if (open)
    {
        m_varExpanded.insert (name);
    }
    else
    {
        m_varExpanded.erase (name);
    }

    FillVariables (m_context.browser.GetPreview(), false);

    for (size_t i = 0; i < m_varLines.size(); i++)
    {
        if (m_varLines[i].header && m_varLines[i].array == array)
        {
            m_list->SetSelectedRow ((int) i);
            break;
        }
    }

    m_context.window.Invalidate();

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::IsArrayOpen
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewListingView::IsArrayOpen (int row) const
{
    return row + 1 < (int) m_varLines.size() && m_varLines[(size_t) row].header
        && !m_varLines[(size_t) row + 1].header && m_varLines[(size_t) row + 1].array == m_varLines[(size_t) row].array;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::HandleVariablesMouse
//
//  A press on an array's expand mark opens or closes it, and a right-click
//  opens the menu for the rows it is on, selecting the row first when it is
//  not already.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewListingView::HandleVariablesMouse (const DxuiMouseEvent & ev)
{
    DxuiMouseEvent  local = ExplorerLayout::ToLocal (ev, m_list->GetBounds());
    int             row   = m_list->HitTestRow (0, local.positionDip.y);
    RECT            cell  = {};
    bool            mark  = false;



    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        return false;
    }

    m_valueTip.Hide();

    if (row < 0 || row >= (int) m_varLines.size())
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && m_varLines[(size_t) row].header && m_list->GetCellTextRectPx (row, 0, cell))
    {
        mark = local.positionDip.x >= cell.left && local.positionDip.x < cell.left + 2 * GetCharWidthPx();
    }

    if (mark)
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
        return ToggleArray (row, !IsArrayOpen (row), false);
    }

    if (ev.kind == DxuiMouseEventKind::Up && ev.button == DxuiMouseButton::Right)
    {
        if (!m_list->IsRowSelected (row))
        {
            m_list->SetSelectedRow (row);
        }

        ShowVariablesMenu (ev.positionDip.x, ev.positionDip.y);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::HandleVariablesKey
//
//  Right opens the selected array and Left closes it, or moves from an element
//  to its array, as a tree's keys do. Ctrl+C copies the selected rows.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewListingView::HandleVariablesKey (const DxuiKeyEvent & ev)
{
    int   row    = m_list->GetSelectedRow();
    bool  header = row >= 0 && row < (int) m_varLines.size() && m_varLines[(size_t) row].header;



    if (ev.kind != DxuiKeyEventKind::Down || row < 0 || row >= (int) m_varLines.size())
    {
        return false;
    }

    if (ev.ctrl && ev.vk == 'C')
    {
        CopyVariables (VarPart::Row);
        return true;
    }

    if (!ev.ctrl && ev.vk == VK_RIGHT && header && !IsArrayOpen (row))
    {
        return ToggleArray (row, true, false);
    }

    if (!ev.ctrl && ev.vk == VK_LEFT && (header ? IsArrayOpen (row) : m_varLines[(size_t) row].array >= 0))
    {
        return ToggleArray (row, false, !header);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::ShowVariablesMenu
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::ShowVariablesMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    m_context.menus.GetMenuCommands().clear();

    m_context.menus.AddMenuCommand (items, L"Copy &name",  [this]() { CopyVariables (VarPart::Name);  });
    m_context.menus.AddMenuCommand (items, L"Copy &value", [this]() { CopyVariables (VarPart::Value); });
    m_context.menus.AddMenuCommand (items, L"Copy &row",   [this]() { CopyVariables (VarPart::Row);   });

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::CopyVariables
//
//  The selected rows' Name or Value text, or whole rows with their columns
//  separated by tabs, one row to a line.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::CopyVariables (VarPart part)
{
    std::vector<int>  selected = m_list->GetSelectedRows();
    std::wstring      text;



    std::sort (selected.begin(), selected.end());

    for (int row : selected)
    {
        if (row < 0 || row >= (int) m_varLines.size())
        {
            continue;
        }

        const VarLine &  line = m_varLines[(size_t) row];

        if (!text.empty())
        {
            text += L"\r\n";
        }

        text += (part == VarPart::Name)  ? line.name
              : (part == VarPart::Value) ? line.value
                                         : line.name + L"\t" + line.type + L"\t" + line.length + L"\t" + line.value;
    }

    DxuiClipboard::SetText (m_context.window.GetHwnd(), text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::UpdateValueTipAt
//
//  Every mouse event in the window: a move over a value cut off at the
//  pane's edge puts the whole value over the row, and any other event
//  hides it.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::UpdateValueTipAt (const DxuiMouseEvent & ev)
{
    //  The row is tested at the list's left edge: the list takes a point past
    //  its columns' natural widths, as over the stretched Value column, for
    //  empty space.
    DxuiMouseEvent  local = ExplorerLayout::ToLocal (ev, m_list->GetBounds());
    bool            over  = m_isVariables && m_list->IsVisible() && ev.kind == DxuiMouseEventKind::Move && ExplorerLayout::Contains (m_list->GetBounds(), ev.positionDip);



    UpdateValueTip (over ? m_list->HitTestRow (0, local.positionDip.y) : -1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::UpdateValueTip
//
//  A value wider than its cell is drawn whole over the row, from the cell's
//  left to the list's right, wrapped onto as many lines as it takes.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::UpdateValueTip (int row)
{
    HRESULT              hr      = S_OK;
    IDxuiTextRenderer  * text    = m_context.window.GetTextRenderer();
    RECT                 bounds  = m_list->GetBounds();
    RECT                 cell    = {};
    RECT                 tip     = {};
    float                fontPx  = 0.0f;
    float                width   = 0.0f;
    float                height  = 0.0f;
    int                  padPx   = m_context.scaler.ToPx (s_kValueTipPadDip);
    bool                 shown   = false;
    std::wstring         value;



    BAIL_OUT_IF (row < 0 || row >= (int) m_varLines.size() || text == nullptr || m_context.theme == nullptr, S_OK);
    BAIL_OUT_IF (!m_list->GetCellTextRectPx (row, s_kValueColumn, cell), S_OK);

    value  = m_varLines[(size_t) row].value;
    fontPx = (float) m_context.scaler.ToPxf (m_list->GetFontDip());

    hr = text->MeasureString (value.c_str(), fontPx, DxuiTheme::kMonoFace, width, height);
    BAIL_OUT_IF (FAILED (hr) || width <= (float) (cell.right - cell.left), S_OK);

    tip.left  = bounds.left + cell.left - padPx;
    tip.right = bounds.right - m_list->GetScrollbarWidthPx();
    tip.top   = bounds.top + cell.top;

    hr = text->MeasureStringWrapped (value.c_str(), fontPx, DxuiTheme::kMonoFace, (float) (tip.right - tip.left - 2 * padPx), width, height);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    tip.bottom = tip.top + (std::max) ((int) (cell.bottom - cell.top), (int) std::ceil (height)) + padPx;

    m_valueTip.Show (tip, m_context.theme->TooltipBackground(), [this, value, fontPx, padPx, tip] (IDxuiPainter & painter, IDxuiTextRenderer & render)
    {
        float  w = (float) (tip.right - tip.left);
        float  h = (float) (tip.bottom - tip.top);

        HRESULT  drawn = S_OK;

        painter.OutlineRect (0.0f, 0.0f, w, h, 1.0f, m_context.theme->TooltipBorder());

        drawn = render.DrawString (value.c_str(), (float) padPx, 0.0f, w - 2.0f * (float) padPx, h,
                                   m_context.theme->TooltipForeground(), fontPx, DxuiTheme::kMonoFace);
        IGNORE_RETURN_VALUE (drawn, S_OK);
    });

    shown = true;

Error:
    if (!shown)
    {
        m_valueTip.Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::GetCharWidthPx
//
//  The width of one character in the list's fixed-width face.
//
////////////////////////////////////////////////////////////////////////////////

int PreviewListingView::GetCharWidthPx() const
{
    HRESULT              hr     = S_OK;
    IDxuiTextRenderer  * text   = m_context.window.GetTextRenderer();
    float                width  = 0.0f;
    float                height = 0.0f;
    int                  result = m_context.scaler.ToPx (s_kFallbackCharDip);



    BAIL_OUT_IF (text == nullptr, S_OK);

    hr = text->MeasureString (s_kpszWidthSample, (float) m_context.scaler.ToPxf (m_list->GetFontDip()), DxuiTheme::kMonoFace, width, height);
    BAIL_OUT_IF (FAILED (hr) || width <= 0.0f, S_OK);

    result = (int) std::ceil (width / (float) wcslen (s_kpszWidthSample));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::SortVariables
//
//  A click on the sorted column turns the order around; a click on another
//  sorts on it, ascending.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::SortVariables (int column)
{
    m_varDescending = (column == m_varColumn) ? !m_varDescending : false;
    m_varColumn     = column;

    FillVariables (m_context.browser.GetPreview(), false);
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::SetVariablesFilter
//
//  Clearing the search closes the arrays again, as for a new file.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::SetVariablesFilter (const std::wstring & filter)
{
    bool  cleared = !m_varFilter.empty() && filter.empty();



    m_varFilter = filter;

    FillVariables (m_context.browser.GetPreview(), cleared);
    m_context.window.Invalidate();
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
        m_valueTip.Hide();
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
    }

    if (m_isVariables && HandleVariablesMouse (ev))
    {
        m_context.window.Invalidate();
        return true;
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
    if (m_isVariables && HandleVariablesKey (ev))
    {
        m_context.window.Invalidate();
        return true;
    }

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
//  is the I-beam, as over text; over an archive's or a VAR file's rows, and
//  over the column titles, it is the arrow, as over the file list's.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR PreviewListingView::GetCursorForPoint (POINT clientPx) const
{
    RECT     list   = m_list->GetBounds();
    POINT    local  = { clientPx.x - list.left, clientPx.y - list.top };
    LPCWSTR  cursor = nullptr;
    bool     arrow  = m_isArchive || m_isVariables || local.y < m_list->GetHeaderHeightPx();



    if (!m_list->IsVisible() || !ExplorerLayout::Contains (list, clientPx))
    {
        return nullptr;
    }

    cursor = m_list->GetCursorForPoint (local);

    return (cursor != nullptr || m_list->IsOverScrollbar (local)) ? cursor : (arrow ? IDC_ARROW : IDC_IBEAM);
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

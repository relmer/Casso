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

    //  A VAR file's values wrap to the pane's width, so they wrap again when
    //  the pane is resized.
    if (m_isVariables && content.right - content.left != m_varLayoutWidthPx)
    {
        FillVariables (m_context.browser.GetPreview(), false);
    }
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
//  The simple variables are a group, and each array another, its label the
//  array's name and subscripts. Arrays start collapsed. While a search
//  filters the rows, every group is open, so each match is in view.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewListingView::FillVariables (const PreviewContent & preview, bool reset)
{
    using Column = ApplesoftVariables::Column;

    std::vector<std::vector<DxuiListView::Cell>>  rows;
    std::vector<DxuiListView::Group>              groups;
    std::vector<ApplesoftVariables::Section>      sections;
    std::wstring                                  lower = m_varFilter;

    auto  matchesIn = [&lower] (const std::wstring & text)
    {
        std::vector<std::pair<int, int>>  ranges;
        std::wstring                      hay = text;

        std::transform (hay.begin(), hay.end(), hay.begin(), [] (wchar_t c) { return (wchar_t) towlower (c); });

        for (size_t at = lower.empty() ? std::wstring::npos : hay.find (lower); at != std::wstring::npos; at = hay.find (lower, at + lower.size()))
        {
            ranges.emplace_back ((int) at, (int) (at + lower.size()));
        }

        return ranges;
    };



    std::transform (lower.begin(), lower.end(), lower.begin(), [] (wchar_t c) { return (wchar_t) towlower (c); });

    m_varRows     = preview.variables.data();
    m_varRowCount = preview.variables.size();

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
                                    DxuiListView::Column { L"Value",  0, false } });
    m_list->SetSortIndicator     (m_varColumn, m_varDescending);

    sections = ApplesoftVariables::Arrange (preview.variables, preview.arrays, m_varFilter, (Column) m_varColumn, m_varDescending);

    //  The rows with each value wrapped at `wrap` characters: a long value
    //  continues on further rows below its first.
    auto  build = [&] (size_t wrap)
    {
        rows.clear();
        groups.clear();

        for (const ApplesoftVariables::Section & section : sections)
        {
            DxuiListView::Group  group;

            group.firstRow  = (int) rows.size();
            group.label     = (section.array < 0) ? std::format (L"Variables ({})", section.rows.size())
                                                  : std::format (L"{}{} ({})", preview.arrays[(size_t) section.array].name,
                                                                 preview.arrays[(size_t) section.array].dimensions, section.rows.size());
            group.collapsed = section.array >= 0 && m_varFilter.empty();
            groups.push_back (group);

            for (size_t index : section.rows)
            {
                const VariableRow &  row   = preview.variables[index];
                std::wstring         first = row.value.substr (0, wrap);

                rows.push_back ({ { row.name,   false, matchesIn (row.name)  },
                                  { row.type,   true },
                                  { row.length, true },
                                  { first,      false, matchesIn (first) } });

                for (size_t at = wrap; at < row.value.size(); at += wrap)
                {
                    std::wstring  part = row.value.substr (at, wrap);

                    rows.push_back ({ { L"" }, { L"" }, { L"" }, { part, false, matchesIn (part) } });
                }
            }
        }
    };



    //  The other columns are measured with every value whole, and the values
    //  then wrapped to the remaining width.
    build (std::wstring::npos);
    m_list->SetRows (rows);
    build (GetValueWrapChars());

    m_list->SetRows   (std::move (rows));
    m_list->SetGroups (groups);

    //  A new file, or a search begun or cleared, sets every group's state;
    //  after a resort, each group is as the user left it.
    if (reset || !m_varFilter.empty())
    {
        for (size_t i = 0; i < groups.size(); i++)
        {
            m_list->SetGroupCollapsed ((int) i, groups[i].collapsed);
        }
    }

    m_list->ResetAutoFit();
    m_list->UpdateAutoFitFromRows();

    if (reset)
    {
        m_list->SetTopRow (0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView::GetValueWrapChars
//
//  How many characters of a value fit in the list's width, less the Name,
//  Type and Length columns and the scrollbar, in the list's fixed-width face.
//
////////////////////////////////////////////////////////////////////////////////

size_t PreviewListingView::GetValueWrapChars()
{
    HRESULT              hr      = S_OK;
    size_t               result  = s_kMinWrapChars;
    IDxuiTextRenderer *  text    = m_context.window.GetTextRenderer();
    RECT                 bounds  = m_list->GetBounds();
    float                width   = 0.0f;
    float                height  = 0.0f;
    float                charW   = 0.0f;
    int                  roomPx  = 0;



    m_varLayoutWidthPx = bounds.right - bounds.left;

    BAIL_OUT_IF (text == nullptr || m_varLayoutWidthPx <= 0, S_OK);

    hr = text->MeasureString (s_kpszWrapSample, (float) m_context.scaler.ToPxf (m_list->GetFontDip()), DxuiTheme::kMonoFace, width, height);
    BAIL_OUT_IF (FAILED (hr) || width <= 0.0f, S_OK);

    charW  = width / (float) wcslen (s_kpszWrapSample);
    roomPx = m_varLayoutWidthPx - m_list->GetScrollbarWidthPx() - m_list->GetCellPaddingPx (s_kValueColumn);

    for (size_t column = 0; column < s_kValueColumn; column++)
    {
        roomPx -= m_list->MeasureColumnContentPx (*text, column);
    }

    result = (std::max) (s_kMinWrapChars, (size_t) ((float) (std::max) (roomPx, 0) / charW));

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

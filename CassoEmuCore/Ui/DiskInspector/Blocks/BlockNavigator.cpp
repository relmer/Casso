#include "Pch.h"

#include "Core/NumberFormat.h"
#include "Ui/DiskInspector/Blocks/BlockNavigator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::Create
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::Create (DxuiPanel & host, BlockInspectorModel * model, const IDxuiTheme * theme, HWND hwnd, IDxuiTextRenderer * text)
{
    m_model  = model;
    m_crumbs = host.CreateChild<DxuiAddressBar>();
    m_search = host.CreateChild<DxuiSearchBox>();
    m_tree   = host.CreateChild<DxuiTreeView>();
    m_list   = host.CreateChild<DxuiListView>();

    m_browser.SetModel (model);

    m_crumbs->SetTextRenderer (text);
    m_crumbs->SetOnSegment    ([this] (int index)
    {
        if (index < 0 || index >= (int) m_targets.size())
        {
            return;
        }

        if (m_targets[index].kind == BlockPlace::Kind::Block)
        {
            if (m_onOpenBlock)
            {
                m_onOpenBlock (m_targets[index].block);
            }

            return;
        }

        GoTo (m_targets[index]);
    });

    m_search->SetTheme       (theme);
    m_search->SetHwnd        (hwnd);
    m_search->SetPlaceholder (L"Search");
    m_search->SetOnChange    ([this] (const std::wstring & value)
    {
        if (!m_isFilling)
        {
            m_query = value;
            FillList();
        }
    });

    m_tree->SetShowCheckboxes (false);
    m_tree->SetOnSelect       ([this] (const std::wstring & id)
    {
        BlockPlace  place;

        if (!m_isFilling && BlockBrowser::ParseId (id, place))
        {
            GoTo (place);
        }
    });

    m_list->SetTheme                 (theme);
    m_list->SetOwnerWindow           (hwnd);
    m_list->SetShowHeader            (true);
    m_list->SetActivateOnDoubleClick (true);
    m_list->SetAlwaysShowSelection   (true);
    m_list->SetOnSelectionChanged    ([this] (int row) { OnRowSelected (row); });
    m_list->SetOnActivateRow         ([this] (int row) { OnRowActivated (row); });
    m_list->SetOnSortColumn          ([this] (int column)
    {
        m_isDescending = column == m_sortColumn && !m_isDescending;
        m_sortColumn   = column;
        FillList();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::Sync
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::Sync()
{
    uint32_t  generation = m_model->GetMapGeneration();
    bool      isFirst    = m_generation == 0;



    if (generation == m_generation || !m_model->IsOpen())
    {
        return;
    }

    m_generation = generation;

    if (isFirst)
    {
        m_browser.Start();
    }

    m_isFilling = true;
    m_tree->SetNodes (m_browser.BuildTree (GetCardLabel()));
    m_isFilling = false;

    Show();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::GoTo
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::GoTo (const BlockPlace & place)
{
    m_browser.GoTo (place);

    m_isFilling = true;
    m_search->Clear();
    m_isFilling = false;
    m_query.clear();

    Show();

    if (m_onPlace)
    {
        m_onPlace();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::ShowBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::ShowBlock (uint32_t block)
{
    m_browser.GoTo (m_browser.MakeBlockPlace (block));
    Show();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::ReplaceBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::ReplaceBlock (uint32_t block)
{
    m_browser.Replace (m_browser.MakeBlockPlace (block));
    Show();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool BlockNavigator::GoBack()
{
    if (!m_browser.GoBack())
    {
        return false;
    }

    Show();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool BlockNavigator::GoForward()
{
    if (!m_browser.GoForward())
    {
        return false;
    }

    Show();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::Show
//
//  The address bar and the tree follow the place, and at a place other
//  than a block, the list is filled for it. A directory's blocks and every
//  file's under it are highlighted, and a file's blocks.
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::Show()
{
    const BlockPlace  & place = m_browser.GetPlace();



    m_crumbs->SetSegments (m_browser.BuildCrumbs (place, GetCardLabel(), m_targets));
    SelectTreeNode();

    if (place.kind == BlockPlace::Kind::Block)
    {
        return;
    }

    m_selected = -1;
    FillList();

    if (place.kind == BlockPlace::Kind::File)
    {
        m_model->HighlightFiles (place.volume, { place.entry });
    }
    else if (place.kind == BlockPlace::Kind::Directory)
    {
        m_model->HighlightFiles (place.volume, BlockBrowser::GetDescendants (m_model->GetVolumeMap (place.volume), place.entry));
    }
    else
    {
        m_model->ClearHighlight();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::SelectTreeNode
//
//  The node of the place's container, its ancestors opened so its row is
//  in the tree.
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::SelectTreeNode()
{
    BlockPlace           container = m_browser.GetContainer (m_browser.GetPlace());
    vector<std::wstring> chain;
    int                  at        = container.entry;
    int                  row       = 0;



    if (container.kind == BlockPlace::Kind::Directory && container.volume >= 0 && container.volume < m_model->GetVolumeCount())
    {
        const BlockMap  & map = m_model->GetVolumeMap (container.volume);

        while (at >= 0 && (size_t) at < map.entries.size() && chain.size() <= map.entries.size())
        {
            chain.push_back (BlockBrowser::MakeId ({ BlockPlace::Kind::Directory, container.volume, at }));
            at = map.entries[at].parent;
        }
    }

    if (container.kind != BlockPlace::Kind::Card)
    {
        chain.push_back (BlockBrowser::MakeId ({ BlockPlace::Kind::Volume, container.volume }));
    }

    chain.push_back (BlockBrowser::MakeId ({ BlockPlace::Kind::Card }));

    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        row = m_tree->FindRowById (*it);

        if (row >= 0 && *it != chain.front())
        {
            (void) m_tree->SetRowExpanded (row, true);
        }
    }

    row = m_tree->FindRowById (chain.front());

    if (row >= 0)
    {
        m_tree->HighlightRow (row);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::FillList
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::FillList()
{
    static constexpr int  kNameDip   = 180;
    static constexpr int  kColumnDip = 78;



    vector<DxuiListView::Column>        columns;
    vector<vector<DxuiListView::Cell>>  rows;
    vector<DxuiListView::Group>         groups;
    size_t                              i = 0;



    m_listing  = m_browser.BuildListing (m_browser.GetPlace(), m_query);
    m_selected = -1;

    SortRows();

    for (i = 0; i < m_listing.columns.size(); i++)
    {
        bool  isNumber = i < m_listing.isNumber.size() && m_listing.isNumber[i];

        columns.push_back ({ m_listing.columns[i], i == 0 ? kNameDip : kColumnDip, i + 1 == m_listing.columns.size(), isNumber ? DxuiTextHAlign::Right : DxuiTextHAlign::Left });
    }

    for (const BlockListGroup & group : m_listing.groups)
    {
        groups.push_back ({ group.title, (int) rows.size() });

        for (const BlockListRow & row : group.rows)
        {
            vector<DxuiListView::Cell>  cells;

            for (const std::wstring & text : row.cells)
            {
                cells.push_back ({ text });
            }

            rows.push_back (std::move (cells));
        }
    }

    m_list->SetColumns       (std::move (columns));
    m_list->SetSortIndicator (m_sortColumn, m_isDescending);
    m_list->SetRows          (std::move (rows));
    m_list->SetGroups        (std::move (groups));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::SortRows
//
//  Within each group, by the sort column: a column of numbers by value,
//  any other by its text without case, then by name.
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::SortRows()
{
    size_t  column   = (size_t) std::max (0, m_sortColumn);
    bool    isNumber = column < m_listing.isNumber.size() && m_listing.isNumber[column];
    bool    isDown   = m_isDescending;
    auto    valueOf  = [] (const std::wstring & text) { return wcstoull (NumberFormat::RemoveThousandsSeparators (text).c_str(), nullptr, 10); };



    for (BlockListGroup & group : m_listing.groups)
    {
        std::stable_sort (group.rows.begin(), group.rows.end(), [&] (const BlockListRow & a, const BlockListRow & b)
        {
            const std::wstring  & left  = column < a.cells.size() ? a.cells[column] : std::wstring();
            const std::wstring  & right = column < b.cells.size() ? b.cells[column] : std::wstring();
            int                   order = isNumber ? (valueOf (left) < valueOf (right) ? -1 : valueOf (left) > valueOf (right) ? 1 : 0) : _wcsicmp (left.c_str(), right.c_str());

            if (order == 0 && column != 0 && !a.cells.empty() && !b.cells.empty())
            {
                order = _wcsicmp (a.cells[0].c_str(), b.cells[0].c_str());
            }

            return isDown ? order > 0 : order < 0;
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::GetRow
//
////////////////////////////////////////////////////////////////////////////////

const BlockListRow * BlockNavigator::GetRow (int index) const
{
    for (const BlockListGroup & group : m_listing.groups)
    {
        if (index >= 0 && index < (int) group.rows.size())
        {
            return &group.rows[index];
        }

        index -= (int) group.rows.size();
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::OnRowSelected
//
//  A file's blocks, a directory's with every file's under it, or the
//  blocks a partition, a structure, a finding or a run covers, highlighted.
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::OnRowSelected (int index)
{
    const BlockListRow  * row = GetRow (index);



    m_selected = index;

    if (row == nullptr)
    {
        return;
    }

    if (row->kind == BlockListRow::Kind::File)
    {
        m_model->HighlightFiles (row->volume, { row->entry });
    }
    else if (row->kind == BlockListRow::Kind::Directory)
    {
        m_model->HighlightFiles (row->volume, BlockBrowser::GetDescendants (m_model->GetVolumeMap (row->volume), row->entry));
    }
    else
    {
        m_model->HighlightBlocks (BlockBrowser::GetRowBlocks (*row));
    }

    if (m_onHighlight)
    {
        m_onHighlight();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::OnRowActivated
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::OnRowActivated (int index)
{
    const BlockListRow  * row = GetRow (index);



    if (row == nullptr)
    {
        return;
    }

    switch (row->kind)
    {
        case BlockListRow::Kind::Partition: GoTo ({ BlockPlace::Kind::Volume, row->volume });                 break;
        case BlockListRow::Kind::Directory: GoTo ({ BlockPlace::Kind::Directory, row->volume, row->entry }); break;
        case BlockListRow::Kind::File:      GoTo ({ BlockPlace::Kind::File, row->volume, row->entry });      break;

        default:
            if (m_onOpenBlock)
            {
                m_onOpenBlock (row->first);
            }

            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::SelectForBlock
//
//  The block's row in the list, or after going to the directory of the
//  block's file, its row there.
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::SelectForBlock (uint32_t block)
{
    int         index     = m_browser.FindRow (m_listing, block);
    BlockPlace  container = m_browser.GetContainer (m_browser.MakeBlockPlace (block));



    if (index < 0 && container.kind != BlockPlace::Kind::Card && !(container == m_browser.GetPlace()))
    {
        GoTo (container);
        index = m_browser.FindRow (m_listing, block);
    }

    if (index >= 0)
    {
        m_list->SetSelectedRow (index);
        OnRowSelected (index);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::GetSelectionLine
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockNavigator::GetSelectionLine() const
{
    const BlockPlace    & place = m_browser.GetPlace();
    const BlockListRow  * row   = GetRow (m_selected);



    if (m_model == nullptr || !m_model->IsOpen())
    {
        return std::wstring();
    }

    if (place.kind == BlockPlace::Kind::Block)
    {
        return L"Selected: block " + NumberFormat::FormatInteger (m_model->GetBlock());
    }

    if (row != nullptr && row->entry >= 0 && row->volume >= 0)
    {
        return L"Selected: " + m_model->GetVolumeMap (row->volume).files[row->entry];
    }

    if (row != nullptr && !row->cells.empty())
    {
        return L"Selected: " + row->cells[0] + L", " + BlockBrowser::DescribeBlocks (BlockBrowser::GetRowBlocks (*row));
    }

    if (place.entry >= 0 && place.volume >= 0)
    {
        return L"Selected: " + m_model->GetVolumeMap (place.volume).files[place.entry];
    }

    return place.volume >= 0 ? L"Selected: " + m_browser.GetVolumeName (place.volume) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::GetCardLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockNavigator::GetCardLabel() const
{
    return m_model != nullptr && m_model->IsOpen() ? fs::path (m_model->GetFile().GetPath()).filename().wstring() : std::wstring();
}

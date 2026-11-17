#include "Pch.h"

#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
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
    m_hwnd   = hwnd;
    m_crumbs = host.CreateChild<DxuiAddressBar>();
    m_search = host.CreateChild<DxuiTextInput>();
    m_tree   = host.CreateChild<DxuiTreeView>();
    m_list   = host.CreateChild<DxuiListView>();

    m_browser.SetModel (model);

    //  The address bar and the search box as Casso Explorer has them: its
    //  text face and icons, and the search box's magnifier and clear button.
    m_crumbs->SetTextRenderer (text);
    m_crumbs->SetIconFace     (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace) ? DxuiToolbar::kFluentIconFace : DxuiToolbar::kMdl2IconFace);
    m_crumbs->SetFont         (DxuiTextRenderer::IsFontFamilyInstalled (DxuiAddressBar::kVariableTextFace) ? DxuiAddressBar::kVariableTextFace : DxuiTheme::GetUiFace(),
                               DxuiAddressBar::kFontDip);
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
    m_crumbs->SetOnSubmit    ([this] (const std::wstring & text) { SubmitPath (text); });
    m_crumbs->SetOnSeparator ([this] (int index, const RECT & anchor) { ShowChildMenu (index, anchor); });
    m_crumbs->SetOnOverflow  ([this] (const RECT & anchor) { ShowOverflowMenu (anchor); });

    m_search->SetTheme          (theme);
    m_search->SetHwnd           (hwnd);
    m_search->SetTextRenderer   (text);
    m_search->SetFont           (DxuiTextRenderer::IsFontFamilyInstalled (DxuiAddressBar::kVariableTextFace) ? DxuiAddressBar::kVariableTextFace : DxuiTheme::GetUiFace(),
                                 DxuiAddressBar::kFontDip);
    m_search->SetExplorerChrome (true);
    m_search->SetTrailingGlyph  (s_kpszMdl2Search, DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace) ? DxuiToolbar::kFluentIconFace : DxuiToolbar::kMdl2IconFace);
    m_search->SetClearGlyph     (s_kpszMdl2Cancel);
    m_search->SetPlaceholder    (L"Search");
    m_search->SetOnChange       ([this] (const std::wstring & value)
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

    m_list->SetTheme                   (theme);
    m_list->SetOwnerWindow             (hwnd);
    m_list->SetShowHeader              (true);
    m_list->SetHorizontalScrollEnabled (true);
    m_list->SetIconColumn              (0);
    m_list->SetExplorerDetails         (true);
    m_list->SetRowHeightPxFn           (&ExplorerLayout::GetListRowHeightPx);
    m_list->SetActivateOnDoubleClick   (true);
    m_list->SetAlwaysShowSelection     (true);
    m_list->SetOnSelectionChanged      ([this] (int row) { OnRowSelected (row); });
    m_list->SetOnActivateRow           ([this] (int row) { OnRowActivated (row); });
    m_list->SetOnSortColumn            ([this] (int column)
    {
        m_isDescending = column == m_sortColumn && !m_isDescending;
        m_sortColumn   = column;
        FillList();
    });

    ExplorerTextStyles::Apply (*m_list, ExplorerTextStyles::kProse);
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

    vector<DxuiTreeNode>  nodes = m_browser.BuildTree (GetCardLabel());



    SetTreeIcons (nodes);

    m_isFilling = true;
    m_tree->SetNodes (std::move (nodes));
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
    m_search->SetText (std::wstring());
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
    m_crumbs->SetPath     (m_browser.FormatPath (place));
    m_crumbs->SetLeadIcon (m_icons == nullptr ? nullptr : m_icons->GetForKind (m_browser.GetContainer (place).kind == BlockPlace::Kind::Directory ? IShellIcons::Kind::Folder : IShellIcons::Kind::DiskImage));
    SelectTreeNode (m_browser.GetContainer (m_browser.GetPlace()));

    m_selected = -1;
    FillList();

    //  At a block, the list is the block's directory's, with the row that
    //  covers the block selected.
    if (place.kind == BlockPlace::Kind::Block)
    {
        m_selected = m_browser.FindRow (m_listing, place.block);

        if (m_selected >= 0)
        {
            m_list->SetSelectedRow (m_selected);
        }

        return;
    }

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

    //  A place opened anywhere flies the map to its blocks, as a row selected
    //  in the list does.
    if (m_onHighlight)
    {
        m_onHighlight();
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

void BlockNavigator::SelectTreeNode (const BlockPlace & container)
{
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
//  A row's cells are built the first time the list draws the row, so a
//  folder of thousands of entries is listed as fast as one of ten.
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::FillList()
{
    static constexpr int  kNameDip   = 180;
    static constexpr int  kTypeDip   = 130;     // a boot loader, as "Apple Pascal loader", or a file system
    static constexpr int  kColumnDip = 78;



    vector<DxuiListView::Column>  columns;
    vector<DxuiListView::Group>   groups;
    size_t                        i     = 0;
    int                           width = 0;



    m_listing  = m_browser.BuildListing (GetListPlace(), m_query);
    m_selected = -1;

    SortRows();

    for (i = 0; i < m_listing.columns.size(); i++)
    {
        bool  isNumber = i < m_listing.isNumber.size() && m_listing.isNumber[i];

        width = (i == 0) ? kNameDip : (i == 1) ? kTypeDip : kColumnDip;

        columns.push_back ({ m_listing.columns[i], width, i + 1 == m_listing.columns.size(), isNumber ? DxuiTextHAlign::Right : DxuiTextHAlign::Left });
    }

    m_rowOrder.clear();

    for (const BlockListGroup & group : m_listing.groups)
    {
        groups.push_back ({ group.title, (int) m_rowOrder.size() });

        for (const BlockListRow & row : group.rows)
        {
            m_rowOrder.push_back (&row);
        }
    }

    m_rowCells.assign (m_rowOrder.size(), {});
    m_rowBuilt.assign (m_rowOrder.size(), false);

    m_list->SetColumns       (std::move (columns));
    m_list->SetSortIndicator (m_sortColumn, m_isDescending);
    m_list->SetRowSource     ((int) m_rowOrder.size(), [this] (int row) -> const vector<DxuiListView::Cell> & { return GetRowCells (row); });
    m_list->SetRowNameSource ([this] (int row) { return (row >= 0 && (size_t) row < m_rowOrder.size() && !m_rowOrder[(size_t) row]->cells.empty()) ? m_rowOrder[(size_t) row]->cells[0] : std::wstring(); });
    m_list->SetGroups        (std::move (groups));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::GetRowCells
//
////////////////////////////////////////////////////////////////////////////////

const vector<DxuiListView::Cell> & BlockNavigator::GetRowCells (int row)
{
    static const vector<DxuiListView::Cell>  s_kNone;



    if (row < 0 || (size_t) row >= m_rowOrder.size())
    {
        return s_kNone;
    }

    if (!m_rowBuilt[(size_t) row])
    {
        vector<DxuiListView::Cell> & cells = m_rowCells[(size_t) row];

        for (const std::wstring & text : m_rowOrder[(size_t) row]->cells)
        {
            cells.push_back ({ text });
        }

        if (!cells.empty())
        {
            cells[0].icon = GetRowIcon (*m_rowOrder[(size_t) row]);
        }

        m_rowBuilt[(size_t) row] = true;
    }

    return m_rowCells[(size_t) row];
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

    SelectTreeNode (GetRowContainer (*row));

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
//  A click on the map selects what the block is part of: the file or the
//  directory that uses it, in the list of the directory it is in, or a
//  structure of the volume, such as its bitmap, in the volume's list. At a
//  file's place, the file's run with the block is selected instead.
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::SelectForBlock (uint32_t block)
{
    BlockPlace  at     = m_browser.MakeBlockPlace (block);
    BlockPlace  target = { HasCardPlace() ? BlockPlace::Kind::Card : BlockPlace::Kind::Volume, at.volume };
    int         entry  = -1;
    int         index  = -1;



    if (at.volume >= 0 && at.volume < m_model->GetVolumeCount())
    {
        const BlockMap  & map   = m_model->GetVolumeMap (at.volume);
        uint32_t          inner = block - m_model->GetVolumeFirst (at.volume);

        entry  = inner < map.GetBlockCount() && !map.blocks[inner].owners.empty() ? map.blocks[inner].owners[0] : -1;
        entry  = entry >= 0 && (size_t) entry < map.entries.size() ? entry : -1;
        target = entry >= 0 && map.entries[entry].parent >= 0 ? BlockPlace { BlockPlace::Kind::Directory, at.volume, map.entries[entry].parent }
                                                              : BlockPlace { BlockPlace::Kind::Volume, at.volume };
    }

    //  A file's place lists its runs, which cover the block.
    if (GetPlace().kind == BlockPlace::Kind::File && GetPlace().volume == at.volume && GetPlace().entry == entry)
    {
        target = GetPlace();
    }

    if (!(target == GetPlace()) && target.kind != BlockPlace::Kind::Card)
    {
        GoTo (target);
    }

    for (int i = 0; entry >= 0 && GetPlace().kind != BlockPlace::Kind::File && GetRow (i) != nullptr && index < 0; i++)
    {
        const BlockListRow  * row = GetRow (i);

        if ((row->kind == BlockListRow::Kind::File || row->kind == BlockListRow::Kind::Directory) && row->volume == at.volume && row->entry == entry)
        {
            index = i;
        }
    }

    index = index >= 0 ? index : m_browser.FindRow (m_listing, block);

    if (index >= 0)
    {
        m_list->SetSelectedRow (index);
        m_list->EnsureVisible  (index);
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





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::SetTreeIcons
//
//  The card and each volume have the disk image's floppy, and each
//  directory the folder, as in Casso Explorer's tree.
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::SetTreeIcons (vector<DxuiTreeNode> & nodes)
{
    BlockPlace  place;



    if (m_icons == nullptr)
    {
        return;
    }

    for (DxuiTreeNode & node : nodes)
    {
        if (BlockBrowser::ParseId (node.id, place))
        {
            node.icon = m_icons->GetForKind (place.kind == BlockPlace::Kind::Directory ? IShellIcons::Kind::Folder : IShellIcons::Kind::DiskImage);
        }

        SetTreeIcons (node.children);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::GetRowIcon
//
//  A partition has the disk image's floppy, a directory the folder, and a
//  file its Apple type's icon, as in Casso Explorer's list. A structure, a
//  finding and a run of blocks have the generic file's.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> BlockNavigator::GetRowIcon (const BlockListRow & row)
{
    if (m_icons == nullptr)
    {
        return nullptr;
    }

    switch (row.kind)
    {
        case BlockListRow::Kind::Partition: return m_icons->GetForKind (IShellIcons::Kind::DiskImage);
        case BlockListRow::Kind::Directory: return m_icons->GetForKind (IShellIcons::Kind::Folder);
        case BlockListRow::Kind::File:      return m_icons->GetForKind (row.cells.size() > (size_t) BlockBrowser::kType ? BrowserCells::GetAppleTypeIconKind (row.cells[(size_t) BlockBrowser::kType]) : IShellIcons::Kind::File);
        default:                            return m_icons->GetForKind (IShellIcons::Kind::File);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::SubmitPath
//
//  A path typed in the address bar opens its place. The current place's
//  path, a block's included, ends the edit with no change. For a path
//  that matches nothing, the field stays open.
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::SubmitPath (const std::wstring & text)
{
    BlockPlace  place;



    if (text == m_browser.FormatPath (GetPlace()))
    {
        m_crumbs->EndEdit();
        return;
    }

    if (!m_browser.FindPath (text, GetCardLabel(), place))
    {
        MessageBeep (MB_ICONWARNING);
        return;
    }

    m_crumbs->EndEdit();
    GoTo (place);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::ShowChildMenu
//
//  The places inside a segment's place, hung from the separator after it,
//  as Casso Explorer's address bar lists a folder's folders.
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::ShowChildMenu (int index, const RECT & anchor)
{
    vector<DxuiPopupMenuItem>  items;



    if (m_popupHost == nullptr || index < 0 || index >= (int) m_targets.size())
    {
        return;
    }

    m_menuCommands.clear();

    for (const auto & [label, place] : m_browser.GetChildPlaces (m_targets[index]))
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        BlockPlace                    target  = place;
        std::wstring                  escaped;

        for (wchar_t ch : label)
        {
            escaped += ch == L'&' ? L"&&" : std::wstring (1, ch);
        }

        command->label    = escaped;
        command->dispatch = [this, target] () { GoTo (target); };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_menuCommands.push_back (std::move (command));
    }

    if (items.empty())
    {
        return;
    }

    m_crumbs->SetOpenSeparator (index);
    DxuiContextMenu::ShowUnder (*m_popupHost, anchor, std::move (items), [this] (bool)
    {
        m_crumbs->SetOpenSeparator (-1);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::GetListPlace
//
//  The place the list is of: the place, or at a block, the block's
//  directory.
//
////////////////////////////////////////////////////////////////////////////////

BlockPlace BlockNavigator::GetListPlace() const
{
    return IsBlockPlace() ? m_browser.GetContainer (GetPlace()) : GetPlace();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::GetRowContainer
//
//  The tree's node for a row: a file's or a directory's directory, or for
//  any other row, its volume.
//
////////////////////////////////////////////////////////////////////////////////

BlockPlace BlockNavigator::GetRowContainer (const BlockListRow & row) const
{
    if (row.volume < 0 || row.volume >= m_model->GetVolumeCount())
    {
        return m_browser.GetContainer (GetPlace());
    }

    const BlockMap  & map = m_model->GetVolumeMap (row.volume);

    if ((row.kind == BlockListRow::Kind::File || row.kind == BlockListRow::Kind::Directory) && row.entry >= 0 && (size_t) row.entry < map.entries.size() && map.entries[row.entry].parent >= 0)
    {
        return { BlockPlace::Kind::Directory, row.volume, map.entries[row.entry].parent };
    }

    return { BlockPlace::Kind::Volume, row.volume };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::HasCardPlace
//
////////////////////////////////////////////////////////////////////////////////

bool BlockNavigator::HasCardPlace() const
{
    return m_browser.HasCard();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::AddMenuCommand
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::AddMenuCommand (vector<DxuiPopupMenuItem> & items, const std::wstring & label, std::function<void()> dispatch)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



    command->label    = label;
    command->dispatch = std::move (dispatch);

    items.push_back (DxuiPopupMenuItem::ForCommand (command));
    m_menuCommands.push_back (std::move (command));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::ShowOverflowMenu
//
//  The segments collapsed behind the overflow button, nearest first, as
//  Casso Explorer's address bar lists them.
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::ShowOverflowMenu (const RECT & anchor)
{
    vector<DxuiPopupMenuItem>  items;
    vector<BlockPlace>         targets;
    vector<std::wstring>       labels = m_browser.BuildCrumbs (GetPlace(), GetCardLabel(), targets);



    if (m_popupHost == nullptr)
    {
        return;
    }

    m_menuCommands.clear();

    for (int i = std::min (m_crumbs->GetFirstShown(), (int) labels.size()) - 1; i >= 0; i--)
    {
        BlockPlace  target = targets[(size_t) i];

        AddMenuCommand (items, labels[(size_t) i], [this, target] () { GoTo (target); });
    }

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnder (*m_popupHost, anchor, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockNavigator::ShowAddressMenu
//
////////////////////////////////////////////////////////////////////////////////

void BlockNavigator::ShowAddressMenu (int x, int y, std::function<void()> focus)
{
    vector<DxuiPopupMenuItem>  items;
    std::wstring               address = m_browser.FormatPath (GetPlace());



    if (m_popupHost == nullptr)
    {
        return;
    }

    m_menuCommands.clear();

    AddMenuCommand (items, L"&Copy Address", [this, address] () { DxuiClipboard::SetText (m_hwnd, address); });
    AddMenuCommand (items, L"&Edit Address", [this, focus] ()
    {
        if (focus)
        {
            focus();
        }

        m_crumbs->BeginEdit();
    });

    DxuiContextMenu::Show (*m_popupHost, x, y, std::move (items));
}

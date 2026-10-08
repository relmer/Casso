#pragma once

#include "Pch.h"

#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FolderViewEntry
//
//  One folder's view as chosen: the folder's key, the view, and how its items
//  are sorted and grouped. A folder never given a sort or a grouping opens
//  with Explorer's own: by name, up, and not grouped.
//
////////////////////////////////////////////////////////////////////////////////

struct FolderViewEntry
{
    std::wstring        key;
    DxuiListView::View  view            = DxuiListView::View::Details;
    int                 sortColumn      = 0;
    bool                sortDescending  = false;
    int                 groupBy         = 0;          // a RowGrouping::Field
    bool                groupDescending = false;

    //  The list's column widths in dips, set by the user in this folder;
    //  empty follows the default widths, and a zero a column that fits itself.
    std::vector<int>    columnWidthsDip;

    //  The order the user dragged this folder's columns into, and the
    //  columns hidden from its header's menu, as Explorer keeps both for
    //  each folder; empty, and not chosen, follow the latest choices.
    std::vector<int>    columnOrder;
    std::vector<int>    hiddenColumns;
    bool                columnsChosen   = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FolderViews
//
//  Which view each folder opens in, by File Explorer's rules: the view last
//  chosen for that folder, else the default for the folder's type. This PC
//  is a list of drives and opens in Tiles; a picture or video folder opens
//  in Large icons; every other folder opens in Details.
//
//  THE CHOICES ARE KEPT MOST RECENT FIRST, AND CAPPED. Explorer keeps each
//  folder's view the same way and drops the oldest past a limit, so a folder
//  visited once long ago does not hold its place forever.
//
////////////////////////////////////////////////////////////////////////////////

class FolderViews
{
public:
    enum class FolderType { Generic, Documents, Pictures, Music, Videos, Drives };

    static DxuiListView::View  GetDefaultView (FolderType type);

    DxuiListView::View  GetView  (const std::wstring & key, FolderType type) const;
    void                Remember (const std::wstring & key, DxuiListView::View view);

    //  The folder's entry, or one with the defaults for its type when none was
    //  kept; and keeping one, first, whole.
    FolderViewEntry     GetEntry (const std::wstring & key, FolderType type) const;
    void                Remember (const FolderViewEntry & entry);

    const std::vector<FolderViewEntry> &  GetEntries () const { return m_entries; }
    void                                  SetEntries (std::vector<FolderViewEntry> entries);

    //  A host folder's type by Explorer's rules: a user folder by its location,
    //  and any other by the FolderType entry in its desktop.ini.
    static FolderType  ReadFolderType (const std::wstring & path);

    //  Explorer's own limit: its BagMRU Size defaults to 5,000 folders.
    static constexpr size_t  kMaxEntries = 5000;

private:
    static bool  IsSameFolder (const std::wstring & a, const std::wstring & b);

    std::vector<FolderViewEntry>  m_entries;
};

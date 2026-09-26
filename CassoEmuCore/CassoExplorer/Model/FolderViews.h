#pragma once

#include "Pch.h"

#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FolderViewEntry
//
//  One folder's view as chosen: the folder's key and the view.
//
////////////////////////////////////////////////////////////////////////////////

struct FolderViewEntry
{
    std::wstring        key;
    DxuiListView::View  view = DxuiListView::View::Details;
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

    const std::vector<FolderViewEntry> &  GetEntries () const { return m_entries; }
    void                                  SetEntries (std::vector<FolderViewEntry> entries);

    //  A host folder's type by Explorer's rules: a user folder by its location,
    //  and any other by the FolderType entry in its desktop.ini.
    static FolderType  ReadFolderType (const std::wstring & path);

    static constexpr size_t  kMaxEntries = 1000;

private:
    static bool  IsSameFolder (const std::wstring & a, const std::wstring & b);

    std::vector<FolderViewEntry>  m_entries;
};

#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailTabs
//
//  A window's tabs, for thumbnails of whole folders: the host folder of its
//  active tab, empty when the tab is at something else, the host folders of
//  its other tabs, and which of its tabs is at the Casso node.
//
////////////////////////////////////////////////////////////////////////////////

struct ThumbnailTabs
{
    enum class CassoNode { None, Active, Other };

    std::wstring               active;
    std::vector<std::wstring>  others;
    CassoNode                  casso = CassoNode::None;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IFolderThumbnails
//
//  Thumbnails for every disk image in the folders open in a window's tabs
//  and under the Casso node's folders, with the whole-folder setting on in
//  Options. Images in an active tab are booted first, then those in the
//  other tabs, then the rest. The Casso node's images count as in the tab
//  at the node.
//
////////////////////////////////////////////////////////////////////////////////

class IFolderThumbnails
{
public:
    virtual ~IFolderThumbnails () = default;

    //  A folder that wasn't open in the window's tabs before is swept.
    virtual void  SetWindowTabs (HWND window, const ThumbnailTabs & tabs) = 0;
    virtual void  RemoveWindow  (HWND window) = 0;

    //  Sweeps a folder just listed, for an image added since its last sweep.
    virtual void  SweepFolder   (const std::wstring & folder) = 0;

    //  The Casso node's folders, swept with their subfolders, and the images
    //  its list has, stock and demo disks included. Both wait until Casso
    //  Explorer has been running a while, or until StartKnownFolders.
    virtual void  SetKnownFolders   (const std::vector<std::wstring> & folders) = 0;
    virtual void  SetCassoImages    (const std::vector<std::wstring> & paths)   = 0;
    virtual void  StartKnownFolders () = 0;

    //  After Options changed: every folder is swept again, or with the
    //  setting off, the images waiting for a sweep are removed.
    virtual void  ApplySetting () = 0;
};

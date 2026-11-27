#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IFolderThumbnails
//
//  Thumbnails for every disk image in the folders open in a window's tabs
//  and in the known folders, with the whole-folder setting on in Options.
//  Images in an active tab's folder are booted first, then those in the
//  other tabs' folders, then the rest.
//
////////////////////////////////////////////////////////////////////////////////

class IFolderThumbnails
{
public:
    virtual ~IFolderThumbnails () = default;

    //  A window's tabs: the host folder of its active tab, empty when the tab
    //  is at something else, and those of its other tabs. A folder that wasn't
    //  open in the window's tabs before is swept.
    virtual void  SetWindowFolders (HWND window, const std::wstring & active, const std::vector<std::wstring> & others) = 0;
    virtual void  RemoveWindow     (HWND window) = 0;

    //  Sweeps a folder just listed, for an image added since its last sweep.
    virtual void  SweepFolder      (const std::wstring & folder) = 0;

    //  The known folders, swept once Casso Explorer has been running a while,
    //  or at once after StartKnownFolders.
    virtual void  SetKnownFolders   (const std::vector<std::wstring> & folders) = 0;
    virtual void  StartKnownFolders () = 0;

    //  After Options changed: every folder is swept again, or with the
    //  setting off, the images waiting for a sweep are removed.
    virtual void  ApplySetting     () = 0;
};

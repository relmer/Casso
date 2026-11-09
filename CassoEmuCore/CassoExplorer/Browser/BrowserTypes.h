#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/VolumeTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserStatus
//
//  What the status bar draws for the location: the item count, the
//  selection, the preview's detail and the free space.
//
////////////////////////////////////////////////////////////////////////////////

struct BrowserStatus
{
    std::wstring  selection;   // how many items: "22 items"
    std::wstring  selected;    // what is selected, empty when nothing is
    std::wstring  detail;
    std::wstring  freeSpace;      // "112 KB free of 140 KB" for a disk image
    std::wstring  freeSpaceTip;   // which disk: the image's name and its volume
};





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSearchMatch
//
//  A search's match inside a disk image: the image, its file system, the
//  directory inside it, and the entry.
//
////////////////////////////////////////////////////////////////////////////////

struct BrowserSearchMatch
{
    std::wstring  imagePath;
    VolumeKind    kind = VolumeKind::Unknown;
    std::string   directory;   // inside the image, or empty for the volume's
    FileEntry     entry;
};
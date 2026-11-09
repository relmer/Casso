#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/Location.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndoStep
//
//  What Ctrl+Z puts back, as Explorer's undo does: a deletion to the Recycle
//  Bin, a rename, a new folder or item, a copy or a move on the PC, and
//  inside a disk image a deletion (from a copy saved aside), a rename, a new
//  folder or a put. Each kind uses only some of the fields.
//
////////////////////////////////////////////////////////////////////////////////

struct ExplorerUndoStep
{
    enum class Kind { Recycle, HostRename, HostNewFolder, HostNewItem, ImageRename, ImageNewFolder, ImageDelete, HostCopy, HostMove, ImagePut };

    Kind                       kind = Kind::Recycle;
    Location                   location;    // where an image step was made
    std::vector<std::wstring>  paths;       // host paths, as they are now
    std::wstring               oldName;
    std::wstring               newName;
    std::wstring               savedDir;    // an image deletion's copy
    std::vector<std::wstring>  sources;     // where moved host items were
    std::vector<std::string>   entries;     // what a put or copy made in an image
};
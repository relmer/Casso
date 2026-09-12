#pragma once

#include "Pch.h"

#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IShellIcons
//
//  The icons Windows itself shows for files, folders and drives, as pixels.
//
//  A SEAM BECAUSE A TEST MAY NOT ASK THE SHELL. Which icon a tree node or a
//  list row is given -- the real path's, or a generic one when there is no
//  path Windows could look at -- is decided above this call and asserted
//  against a fake that records what it was asked.
//
////////////////////////////////////////////////////////////////////////////////

class IShellIcons
{
public:
    //  Icons for things with no path of their own: an entry inside a disk
    //  image, the This PC root, and the emulator the browser belongs to.
    enum class Kind { ThisPc, Folder, File, Casso };

    virtual ~IShellIcons () = default;

    //  The icon Explorer shows for a real file, folder or drive.
    virtual std::shared_ptr<const DxuiIconImage>  GetForPath (const std::wstring & path) = 0;
    virtual std::shared_ptr<const DxuiIconImage>  GetForKind (Kind kind) = 0;
};

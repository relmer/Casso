#pragma once

#include "Pch.h"

#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IShellIcons
//
//  The icons Windows displays for files, folders and drives, as pixels.
//
//  A SEAM BECAUSE A TEST MAY NOT CALL THE SHELL. Which icon a tree node or a
//  list row gets, the icon for its path or a generic one when there is no host
//  path, is decided by the caller and tested against a fake that records the
//  requests.
//
////////////////////////////////////////////////////////////////////////////////

class IShellIcons
{
public:
    //  Icons for things with no path of their own: an entry inside a disk
    //  image, the This PC root, and the emulator the browser belongs to.
    enum class Kind { ThisPc, Folder, File, Casso };

    virtual ~IShellIcons () = default;

    //  The icon Explorer displays for a real file, folder or drive.
    virtual std::shared_ptr<const DxuiIconImage>  GetForPath (const std::wstring & path) = 0;
    virtual std::shared_ptr<const DxuiIconImage>  GetForKind (Kind kind) = 0;
};

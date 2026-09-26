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
    //
    //  `isDirectory` is told rather than looked up: an implementation caching
    //  ordinary files by extension needs to know only whether this one is a
    //  directory, and every caller already holds that. Asking the file system
    //  instead costs a call for each row of a listing.
    virtual std::shared_ptr<const DxuiIconImage>  GetForPath (const std::wstring & path, bool isDirectory) = 0;
    virtual std::shared_ptr<const DxuiIconImage>  GetForKind (Kind kind) = 0;

    //  The type Explorer's Type column shows for a real file or folder, such
    //  as "Text Document" or "File folder". Empty, the default, when the
    //  implementation has no type name for it, and the caller keeps its own.
    virtual std::wstring  GetTypeName (const std::wstring & path, bool isDirectory) { (void) path; (void) isDirectory; return std::wstring(); }
};

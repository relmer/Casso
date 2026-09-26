#pragma once

#include "Pch.h"

#include "Config/IFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FolderOptions
//
//  The File Explorer settings that decide which host items a folder lists and
//  how: its "Show hidden files" and "Hide protected operating system files"
//  options, and whether compressed and encrypted items are shown in color.
//
//  Explorer's rule, which IsShown follows: a hidden item is listed only when
//  hidden items are shown, and a hidden item that is also a system item only
//  when protected files are shown as well. A system item that is not hidden
//  is always listed.
//
////////////////////////////////////////////////////////////////////////////////

struct FolderOptions
{
    bool  showHidden      = false;
    bool  showProtected   = false;
    bool  colorCompressed = false;

    bool  IsShown (const FileSystemEntry & entry) const;

    bool  operator== (const FolderOptions &) const = default;

    //  The user's own settings, as Explorer reads them.
    static FolderOptions  ReadFromShell ();
};

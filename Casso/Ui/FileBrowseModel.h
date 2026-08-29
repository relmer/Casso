#pragma once

#include "Pch.h"

#include "../Config/IFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileBrowseModel
//
//  The pure navigation / validation engine behind the create-disk dialog
//  Owns the current folder, its listing
//  (folders first, then files matching the extension filter), the unique
//  default name, and target validation — all through the injected
//  IFileSystem so every behavior is unit-testable without a window.
//
//  The model never writes anything; creation is the shell's edge.
//
////////////////////////////////////////////////////////////////////////////////

//  Listing rows reuse the filesystem's entry type verbatim (name, isFolder,
//  sizeBytes, modifiedUnix).
using FileBrowseEntry = FileSystemEntry;



//
//  ValidateTarget's verdict, in precedence order: the first failing check
//  wins, so a mounted image is refused outright and never reaches the
//  overwrite-confirm path.
//
enum class TargetVerdict
{
    Ok,
    InvalidName,
    MountedInDrive,
    Exists,
    FolderNotWritable,
};



class FileBrowseModel
{
public:
    void  Bind (IFileSystem * fs);

    HRESULT  SetFolder    (const std::wstring & absolute);
    HRESULT  NavigateInto (size_t entryIndex);
    HRESULT  NavigateUp   ();
    HRESULT  Refresh      ();

    const std::vector<FileBrowseEntry> &  GetEntries       () const { return m_entries; }
    const std::wstring &                  GetCurrentFolder () const { return m_folder; }

    void  SetExtensionFilter (const std::wstring & ext);

    std::wstring  GetUniqueDefaultName (const std::wstring & baseName) const;

    void  SetMountedPaths (std::vector<std::wstring> paths, std::vector<int> drives);

    TargetVerdict  ValidateTarget    (const std::wstring & fileName, int & outDrive) const;
    std::wstring   ComposeTargetPath (const std::wstring & fileName) const;

    //  Windows filename validity (illegal chars, trailing dot/space, reserved
    //  device names). Static so the dialog can pre-check keystrokes too.
    static bool  IsValidFileName (const std::wstring & fileName);

private:
    void  RebuildFilteredView ();

    //  ASCII-range lowering: feeds case-insensitive path comparisons and
    //  name sorts, matching NTFS-default semantics.
    static std::wstring  ToLower (const std::wstring & s);

    //  Case-lowered with separators unified, so the same file reached via
    //  either separator style compares equal.
    static std::wstring  NormalizeForCompare (const std::wstring & path);

    IFileSystem                  * m_fs = nullptr;   // non-owning
    std::wstring                   m_folder;
    std::wstring                   m_extension;      // ".woz" / ".dsk" / ".po"
    std::vector<FileBrowseEntry>   m_allEntries;     // unfiltered cache
    std::vector<FileBrowseEntry>   m_entries;        // filtered view
    std::vector<std::wstring>      m_mountedPaths;
    std::vector<int>               m_mountedDrives;
};

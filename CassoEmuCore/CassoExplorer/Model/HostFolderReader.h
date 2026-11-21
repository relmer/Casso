#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/FolderOptions.h"
#include "Config/IFileSystem.h"
#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderItem
//
////////////////////////////////////////////////////////////////////////////////

struct HostFolderItem
{
    enum class Kind { Folder, Archive, Image, File };

    FileSystemEntry  entry;
    std::wstring     path;
    std::wstring     label;
    Kind             kind = Kind::File;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderReader
//
//  What a folder on this computer contains, for the tree and the list: the
//  entries the folder options let through, which are folders, disk images
//  and archives, and the label of each.
//
////////////////////////////////////////////////////////////////////////////////

class HostFolderReader
{
public:
    using LocalizedNameLookup = std::function<std::wstring (const std::wstring & path)>;

    void                   SetFolderOptions       (const FolderOptions & options) { m_folderOptions = options; }
    const FolderOptions  & GetFolderOptions       () const                         { return m_folderOptions; }
    void                   SetLocalizedNameLookup (LocalizedNameLookup lookup)     { m_localizedName = std::move (lookup); }
    void                   SetShellVerbs          (IShellItemVerbs * verbs)        { m_shellVerbs = verbs; }

    bool  IsShown   (const FileSystemEntry & entry) const { return m_folderOptions.IsShown (entry); }
    bool  IsArchive (const std::wstring & path) const;
    void  Classify  (const std::wstring & folder, const std::vector<FileSystemEntry> & entries, std::vector<HostFolderItem> & outItems) const;

    static bool          IsSupportedImage (const std::wstring & fileName);
    static std::wstring  JoinPath         (const std::wstring & folder, const std::wstring & name);

private:
    FolderOptions                         m_folderOptions;
    LocalizedNameLookup                   m_localizedName;
    IShellItemVerbs                     * m_shellVerbs = nullptr;
    mutable std::map<std::wstring, bool>  m_archiveExtensions;
};

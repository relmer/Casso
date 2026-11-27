#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/HostFolderReader.h"
#include "Config/IFileSystem.h"

struct TreeNode;





////////////////////////////////////////////////////////////////////////////////
//
//  TreeHostNodes
//
//  The tree's nodes in a folder on this computer: its subfolders, then its
//  archives, then its disk images.
//
////////////////////////////////////////////////////////////////////////////////

class TreeHostNodes
{
public:
    //  How a host folder on a network path is read: false while it is being
    //  read, so an unreachable share never stops the window. Without one,
    //  every host folder is read at once.
    using HostFetch = std::function<bool (const std::wstring & path, std::vector<FileSystemEntry> & outEntries, HRESULT & outResult)>;

    TreeHostNodes (IFileSystem & fs, HostFolderReader & hostFolders);

    void  SetHostFetch (HostFetch fetch) { m_hostFetch = std::move (fetch); }

    HRESULT  List      (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes);
    bool     IsCurrent (bool underCasso, const std::wstring & path, const std::vector<TreeNode> & cached);

private:
    bool  HasTreeChildren (const std::wstring & path);

    //  The node id for an item List lists, or empty for a plain file.
    static std::wstring  GetItemId (bool underCasso, const HostFolderItem & item);

    IFileSystem       & m_fs;
    HostFolderReader  & m_hostFolders;
    HostFetch           m_hostFetch;
};

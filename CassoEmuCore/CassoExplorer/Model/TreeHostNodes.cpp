#include "Pch.h"

#include "CassoExplorer/Model/TreeHostNodes.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TreeHostNodes::TreeHostNodes
//
////////////////////////////////////////////////////////////////////////////////

TreeHostNodes::TreeHostNodes (IFileSystem & fs, HostFolderReader & hostFolders)
    : m_fs          (fs),
      m_hostFolders (hostFolders)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeHostNodes::HasTreeChildren
//
//  Whether a host folder holds anything the tree shows, a subfolder or a disk
//  image, so it gets an expander only then, as Explorer's tree does. A folder
//  that cannot be read has none. A network path is not looked into, so an
//  unreachable share never holds up the listing; it keeps its expander.
//
////////////////////////////////////////////////////////////////////////////////

bool TreeHostNodes::HasTreeChildren (const std::wstring & path)
{
    HRESULT                       hr      = S_OK;
    std::vector<FileSystemEntry>  entries;
    bool                          found   = PathResolver::IsNetworkPath (path);



    BAIL_OUT_IF (found, S_OK);

    hr = m_fs.EnumerateAllEntries (path, entries);
    CHR (hr);

    for (const FileSystemEntry & entry : entries)
    {
        found = found || (m_hostFolders.IsShown (entry) && (entry.isFolder || HostFolderReader::IsSupportedImage (entry.name)));
    }

Error:
    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeHostNodes::List
//
//  Subfolders first, then the disk images, each group in name order. Plain
//  files are not nodes; they appear in the file list instead.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeHostNodes::List (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes)
{
    HRESULT                       hr      = S_OK;
    std::vector<FileSystemEntry>  entries;
    std::vector<TreeNode>         folders;
    std::vector<TreeNode>         archives;
    std::vector<TreeNode>         images;
    std::vector<HostFolderItem>   items;
    bool                          network = PathResolver::IsNetworkPath (path);
    bool                          fetched = false;



    outNodes.clear();

    //  Being read: no nodes yet, and none cached, so the next call takes the
    //  listing.
    if (network && m_hostFetch)
    {
        fetched = m_hostFetch (path, entries, hr);
        CBREx (fetched, S_FALSE);   // EHM-ALLOW-SFALSE: still being read; GetChildren keeps nothing for it
    }
    else
    {
        hr = m_fs.EnumerateAllEntries (path, entries);
    }

    CHR (hr);

    m_hostFolders.Classify (path, entries, items);

    for (const HostFolderItem & item : items)
    {
        TreeNode  node;

        node.hidden = item.entry.isHidden;
        node.label  = item.label;

        if (item.kind == HostFolderItem::Kind::Folder)
        {
            node.id        = GetItemId (underCasso, item);
            node.kind      = TreeNode::Kind::HostFolder;
            node.location  = Location::MakeHostFolder (item.path);
            node.canExpand = HasTreeChildren (item.path);

            folders.push_back (node);
        }
        else if (item.kind == HostFolderItem::Kind::Archive)
        {
            node.id        = GetItemId (underCasso, item);
            node.kind      = TreeNode::Kind::ShellRoot;
            node.location  = Location::MakeShellFolder (item.path, item.entry.name);
            node.canExpand = true;
            node.archive   = true;

            archives.push_back (node);
        }
        else if (item.kind == HostFolderItem::Kind::Image)
        {
            node.id       = GetItemId (underCasso, item);
            node.kind     = TreeNode::Kind::DiskImage;
            node.location = Location::MakeDiskImage (item.path);

            //  The image is read when it is expanded, not when its folder
            //  is listed, so a folder of large images opens at once.
            node.canExpand = true;

            images.push_back (node);
        }
    }

    std::sort (folders.begin(), folders.end(), [] (const TreeNode & a, const TreeNode & b)
               { return StrCmpLogicalW (a.label.c_str(), b.label.c_str()) < 0; });
    std::sort (archives.begin(), archives.end(), [] (const TreeNode & a, const TreeNode & b)
               { return StrCmpLogicalW (a.label.c_str(), b.label.c_str()) < 0; });
    std::sort (images.begin(), images.end(), [] (const TreeNode & a, const TreeNode & b)
               { return StrCmpLogicalW (a.label.c_str(), b.label.c_str()) < 0; });

    //  Archives follow the folders, as in Explorer's pane.
    outNodes.insert (outNodes.end(), folders.begin(), folders.end());
    outNodes.insert (outNodes.end(), archives.begin(), archives.end());
    outNodes.insert (outNodes.end(), images.begin(), images.end());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeHostNodes::GetItemId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TreeHostNodes::GetItemId (bool underCasso, const HostFolderItem & item)
{
    switch (item.kind)
    {
        case HostFolderItem::Kind::Folder:  return TreeModel::MakeFolderId (underCasso, item.path);
        case HostFolderItem::Kind::Archive: return std::wstring (TreeModel::kShellRootTag) + item.path;
        case HostFolderItem::Kind::Image:   return TreeModel::MakeImageId (underCasso, item.path);
        default:                            return std::wstring();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeHostNodes::IsCurrent
//
//  One read of the folder, without looking into each subfolder for an
//  expander: a subfolder that changes is watched and read on its own.
//
////////////////////////////////////////////////////////////////////////////////

bool TreeHostNodes::IsCurrent (bool underCasso, const std::wstring & path, const std::vector<TreeNode> & cached)
{
    //  A node's id, label and whether it is hidden.
    using NodeKey = std::tuple<std::wstring, std::wstring, bool>;



    HRESULT                       hr      = S_OK;
    std::vector<FileSystemEntry>  entries;
    std::vector<HostFolderItem>   items;
    std::vector<NodeKey>          before;
    std::vector<NodeKey>          after;
    bool                          current = false;



    BAIL_OUT_IF (path.empty() || PathResolver::IsNetworkPath (path), S_OK);

    hr = m_fs.EnumerateAllEntries (path, entries);
    CHR (hr);

    m_hostFolders.Classify (path, entries, items);

    for (const HostFolderItem & item : items)
    {
        std::wstring  itemId = GetItemId (underCasso, item);

        if (!itemId.empty())
        {
            after.emplace_back (itemId, item.label, item.entry.isHidden);
        }
    }

    for (const TreeNode & node : cached)
    {
        before.emplace_back (node.id, node.label, node.hidden);
    }

    std::sort (before.begin(), before.end());
    std::sort (after.begin(), after.end());

    current = before == after;

Error:
    return current;
}

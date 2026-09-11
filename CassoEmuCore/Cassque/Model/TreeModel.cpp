#include "Pch.h"

#include "Cassque/Model/TreeModel.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::TreeModel
//
////////////////////////////////////////////////////////////////////////////////

TreeModel::TreeModel (IFileSystem & fs)
    : m_fs (fs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::SetKnownFolders
//
////////////////////////////////////////////////////////////////////////////////

void TreeModel::SetKnownFolders (std::vector<std::wstring> folders)
{
    m_knownFolders = std::move (folders);

    m_children.erase (kCassoRootId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::SetDrives
//
////////////////////////////////////////////////////////////////////////////////

void TreeModel::SetDrives (std::vector<std::wstring> driveRoots)
{
    m_drives = std::move (driveRoots);

    m_children.erase (kThisPcRootId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::IsSupportedImage
//
//  The container words the disk tool advertises, as extensions.
//
////////////////////////////////////////////////////////////////////////////////

bool TreeModel::IsSupportedImage (const std::wstring & fileName)
{
    size_t                                    count      = 0;
    const DiskCommandRunner::ContainerName  * containers = DiskCommandRunner::GetAdvertisedContainers (count);
    size_t                                    dot        = fileName.rfind (L'.');
    size_t                                    i          = 0;
    std::string                               extension;



    if (dot == std::wstring::npos)
    {
        return false;
    }

    for (wchar_t c : fileName.substr (dot + 1))
    {
        extension += (char) ((c >= L'A' && c <= L'Z') ? (c - L'A' + L'a') : (c & 0x7F));
    }

    for (i = 0; i < count; i++)
    {
        if (extension == containers[i].name)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::MakeFolderId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TreeModel::MakeFolderId (bool underCasso, const std::wstring & path)
{
    return std::wstring (underCasso ? kCassoRootId : kThisPcRootId) + path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::MakeImageId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TreeModel::MakeImageId (bool underCasso, const std::wstring & path)
{
    return std::wstring (kImagePrefix) + (underCasso ? kCassoTag : kThisPcTag) + kSeparator + path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::MakeDirectoryId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TreeModel::MakeDirectoryId (bool underCasso, const std::wstring & path, const std::string & inner)
{
    return std::wstring (kDirectoryPrefix) + (underCasso ? kCassoTag : kThisPcTag) + kSeparator + path
         + kSeparator + std::wstring (inner.begin(), inner.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::TryParseImageOrDirectoryId
//
////////////////////////////////////////////////////////////////////////////////

bool TreeModel::TryParseImageOrDirectoryId (
    const std::wstring  & id,
    bool                & outUnderCasso,
    std::wstring        & outPath,
    std::string         & outInner)
{
    size_t        prefixLength = 0;
    size_t        firstBar     = 0;
    size_t        secondBar    = 0;
    std::wstring  tag;



    outUnderCasso = false;
    outPath.clear();
    outInner.clear();

    if (id.rfind (kImagePrefix, 0) == 0)
    {
        prefixLength = wcslen (kImagePrefix);
    }
    else if (id.rfind (kDirectoryPrefix, 0) == 0)
    {
        prefixLength = wcslen (kDirectoryPrefix);
    }
    else
    {
        return false;
    }

    firstBar = id.find (kSeparator, prefixLength);

    if (firstBar == std::wstring::npos)
    {
        return false;
    }

    tag           = id.substr (prefixLength, firstBar - prefixLength);
    outUnderCasso = tag == kCassoTag;
    secondBar     = id.find (kSeparator, firstBar + 1);

    if (secondBar == std::wstring::npos)
    {
        outPath = id.substr (firstBar + 1);
    }
    else
    {
        std::wstring  inner = id.substr (secondBar + 1);

        outPath = id.substr (firstBar + 1, secondBar - firstBar - 1);
        outInner.clear();

        //  The inner path was widened one byte to one character when the id
        //  was made, so narrowing each character back is exact.
        for (wchar_t ch : inner)
        {
            outInner.push_back (static_cast<char> (ch));
        }
    }

    return !outPath.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::JoinPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TreeModel::JoinPath (const std::wstring & folder, const std::wstring & name)
{
    if (folder.empty() || folder.back() == L'\\' || folder.back() == L'/')
    {
        return folder + name;
    }

    return folder + L"\\" + name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::GetLeaf
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TreeModel::GetLeaf (const std::wstring & path)
{
    size_t  cut = path.find_last_of (L"\\/");



    if (cut == std::wstring::npos)
    {
        return path;
    }

    return path.substr (cut + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::GetRoots
//
////////////////////////////////////////////////////////////////////////////////

void TreeModel::GetRoots (std::vector<TreeNode> & outNodes) const
{
    TreeNode  casso;
    TreeNode  pc;



    casso.id        = kCassoRootId;
    casso.kind      = TreeNode::Kind::CassoRoot;
    casso.label     = L"Casso";
    casso.canExpand = true;

    pc.id        = kThisPcRootId;
    pc.kind      = TreeNode::Kind::ThisPcRoot;
    pc.label     = L"This PC";
    pc.canExpand = true;

    outNodes.clear();
    outNodes.push_back (casso);
    outNodes.push_back (pc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::Invalidate
//
////////////////////////////////////////////////////////////////////////////////

void TreeModel::Invalidate (const std::wstring & id)
{
    m_children.erase (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::InvalidateAll
//
////////////////////////////////////////////////////////////////////////////////

void TreeModel::InvalidateAll()
{
    m_children.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::ListRoot
//
//  Known folders under Casso, each probed for existence; drives under This
//  PC, taken as given.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeModel::ListRoot (bool underCasso, std::vector<TreeNode> & outNodes)
{
    const std::vector<std::wstring> & paths = underCasso ? m_knownFolders : m_drives;



    outNodes.clear();

    for (const std::wstring & path : paths)
    {
        TreeNode  node;
        bool      exists = !m_directoryProbe || m_directoryProbe (path);

        node.id        = MakeFolderId (underCasso, path);
        node.kind      = underCasso ? TreeNode::Kind::KnownFolder : TreeNode::Kind::Drive;
        node.label     = path;
        node.location  = Location::MakeHostFolder (path);
        node.missing   = !exists;
        node.canExpand = exists;

        outNodes.push_back (node);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::DescribeImage
//
//  Reads the image once. A ProDOS volume's subdirectories become the node's
//  children and give it an expand affordance; a DOS 3.3 volume has none; a
//  file that will not parse gets the error as its tooltip and no affordance.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeModel::DescribeImage (bool underCasso, const std::wstring & path, TreeNode & inOutNode)
{
    HRESULT                hr         = S_OK;
    std::string            content;
    std::vector<Byte>      fileBytes;
    std::vector<Byte>      sectors;
    SectorDecodeReport     report;
    VolumeKind             kind       = VolumeKind::Unknown;
    VolumeListing          listing;
    std::vector<TreeNode>  children;
    std::string            narrowPath = TextEncoding::WideToNarrow (path);



    inOutNode.canExpand = false;
    inOutNode.loadError.clear();

    hr = m_fs.ReadAllText (path, content);

    if (SUCCEEDED (hr))
    {
        fileBytes.assign (content.begin(), content.end());

        hr = VolumeImage::Load (fileBytes, narrowPath, sectors, report);
    }

    if (SUCCEEDED (hr))
    {
        kind = VolumeImage::DetectFilesystem (sectors);

        if (kind == VolumeKind::Unknown)
        {
            hr = HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME);
        }
    }

    if (FAILED (hr))
    {
        inOutNode.loadError = GetLeaf (path) + L" " + std::wstring (DiskImageSession::kNoFilesystemText,
                                                                    DiskImageSession::kNoFilesystemText + strlen (DiskImageSession::kNoFilesystemText));
        m_children[inOutNode.id] = children;

        return S_OK;
    }

    if (kind == VolumeKind::ProDos)
    {
        ProDosVolume  volume (sectors);
        HRESULT       hrList = volume.Enumerate (listing);

        IGNORE_RETURN_VALUE (hrList, S_OK);

        for (const FileEntry & entry : listing.entries)
        {
            TreeNode  child;

            if (!entry.isDirectory)
            {
                continue;
            }

            child.id        = MakeDirectoryId (underCasso, path, entry.name);
            child.kind      = TreeNode::Kind::DiskDirectory;
            child.label     = std::wstring (entry.name.begin(), entry.name.end());
            child.location  = Location::MakeDiskDirectory (path, entry.name);
            child.canExpand = false;

            children.push_back (child);
        }
    }

    inOutNode.canExpand      = !children.empty();
    m_children[inOutNode.id] = children;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::ListHostFolder
//
//  Subfolders first, then the disk images, each group in name order. Plain
//  files are not nodes; they appear in the file list instead.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeModel::ListHostFolder (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes)
{
    HRESULT                       hr = S_OK;
    std::vector<FileSystemEntry>  entries;
    std::vector<TreeNode>         folders;
    std::vector<TreeNode>         images;



    outNodes.clear();

    hr = m_fs.EnumerateEntries (path, entries);
    CHR (hr);

    for (const FileSystemEntry & entry : entries)
    {
        TreeNode  node;

        if (entry.isFolder)
        {
            node.id        = MakeFolderId (underCasso, JoinPath (path, entry.name));
            node.kind      = TreeNode::Kind::HostFolder;
            node.label     = entry.name;
            node.location  = Location::MakeHostFolder (JoinPath (path, entry.name));
            node.canExpand = true;

            folders.push_back (node);
        }
        else if (IsSupportedImage (entry.name))
        {
            node.id       = MakeImageId (underCasso, JoinPath (path, entry.name));
            node.kind     = TreeNode::Kind::DiskImage;
            node.label    = entry.name;
            node.location = Location::MakeDiskImage (JoinPath (path, entry.name));

            hr = DescribeImage (underCasso, JoinPath (path, entry.name), node);
            CHR (hr);

            images.push_back (node);
        }
    }

    std::sort (folders.begin(), folders.end(), [] (const TreeNode & a, const TreeNode & b)
               { return _wcsicmp (a.label.c_str(), b.label.c_str()) < 0; });
    std::sort (images.begin(), images.end(), [] (const TreeNode & a, const TreeNode & b)
               { return _wcsicmp (a.label.c_str(), b.label.c_str()) < 0; });

    outNodes.insert (outNodes.end(), folders.begin(), folders.end());
    outNodes.insert (outNodes.end(), images.begin(), images.end());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::ListImage
//
//  An image expanded before its folder was listed -- a restored tab, say --
//  is described on demand; otherwise its children are already cached.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeModel::ListImage (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes)
{
    HRESULT   hr = S_OK;
    TreeNode  node;



    node.id       = MakeImageId (underCasso, path);
    node.kind     = TreeNode::Kind::DiskImage;
    node.label    = GetLeaf (path);
    node.location = Location::MakeDiskImage (path);

    hr = DescribeImage (underCasso, path, node);
    CHR (hr);

    outNodes = m_children[node.id];

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::GetChildren
//
//  The cache answers when it can; otherwise the id says which listing to
//  produce, and the result is kept for next time.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeModel::GetChildren (const std::wstring & id, std::vector<TreeNode> & outNodes)
{
    HRESULT       hr         = S_OK;
    auto          cached     = m_children.find (id);
    bool          underCasso = false;
    std::wstring  path;
    std::string   inner;



    outNodes.clear();

    if (cached != m_children.end())
    {
        outNodes = cached->second;

        return S_OK;
    }

    m_fetchCount++;

    if (id == kCassoRootId)
    {
        hr = ListRoot (true, outNodes);
    }
    else if (id == kThisPcRootId)
    {
        hr = ListRoot (false, outNodes);
    }
    else if (id.rfind (kCassoRootId, 0) == 0)
    {
        hr = ListHostFolder (true, id.substr (wcslen (kCassoRootId)), outNodes);
    }
    else if (id.rfind (kThisPcRootId, 0) == 0)
    {
        hr = ListHostFolder (false, id.substr (wcslen (kThisPcRootId)), outNodes);
    }
    else if (TryParseImageOrDirectoryId (id, underCasso, path, inner))
    {
        //  A directory inside an image has no children this layer can walk;
        //  an image's children are its directories.
        if (inner.empty())
        {
            hr = ListImage (underCasso, path, outNodes);
        }
    }
    else
    {
        hr = E_INVALIDARG;
    }

    CHR (hr);

    m_children[id] = outNodes;

Error:
    return hr;
}

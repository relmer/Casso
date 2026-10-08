#include "Pch.h"

#include "CassoExplorer/Model/TreeModel.h"
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
//  TreeModel::IsKnownFolder
//
//  Case and a trailing separator do not make a different folder.
//
////////////////////////////////////////////////////////////////////////////////

bool TreeModel::IsKnownFolder (const std::wstring & path) const
{
    std::wstring  wanted = path;



    while (wanted.size() > 3 && (wanted.back() == L'\\' || wanted.back() == L'/'))
    {
        wanted.pop_back();
    }

    for (std::wstring known : m_knownFolders)
    {
        while (known.size() > 3 && (known.back() == L'\\' || known.back() == L'/'))
        {
            known.pop_back();
        }

        if (_wcsicmp (known.c_str(), wanted.c_str()) == 0)
        {
            return true;
        }
    }

    return false;
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
//  TreeModel::BreakAfterFirstSentence
//
//  A message about an image leads with what is wrong and follows with the
//  detail on a line of its own.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TreeModel::BreakAfterFirstSentence (std::wstring text)
{
    size_t  stop = text.find (L". ");



    if (stop != std::wstring::npos)
    {
        text.replace (stop, 2, L".\n");
    }

    return text;
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

std::wstring TreeModel::MakeDirectoryId (bool underCasso, const std::wstring & path, const std::string & inner, size_t occurrence)
{
    std::wstring  id = std::wstring (kDirectoryPrefix) + (underCasso ? kCassoTag : kThisPcTag) + kSeparator + path
                     + kSeparator + std::wstring (inner.begin(), inner.end());



    //  A damaged directory can hold two entries of the same name, which would
    //  otherwise share an id. The first keeps the plain one.
    if (occurrence > 0)
    {
        id += kSeparator + std::to_wstring (occurrence);
    }

    return id;
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
        size_t        thirdBar = id.find (kSeparator, secondBar + 1);
        std::wstring  inner    = id.substr (secondBar + 1, (thirdBar == std::wstring::npos) ? std::wstring::npos : thirdBar - secondBar - 1);

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
//  TreeModel::IsNavRootShown
//
//  Casso Explorer's own value where the user set one; otherwise what File
//  Explorer's pane does. Without the shell's roots, This PC shows alone.
//
////////////////////////////////////////////////////////////////////////////////

bool TreeModel::IsNavRootShown (NavRoot root) const
{
    const std::optional<bool>  & chosen   = (root == NavRoot::ThisPc)  ? m_navOptions.showThisPc
                                          : (root == NavRoot::Network) ? m_navOptions.showNetwork
                                                                       : m_navOptions.showLibraries;
    bool                         explorer = root == NavRoot::ThisPc;



    for (const IShellItemVerbs::ShellFolderItem & item : m_navRoots)
    {
        bool  matches = (root == NavRoot::ThisPc  && item.isThisPc)
                     || (root == NavRoot::Network && item.isNetwork)
                     || (root == NavRoot::Libraries && item.isLibraries);

        if (matches)
        {
            explorer = item.shownByShell;
        }
    }

    return chosen.value_or (explorer);
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
    TreeNode  bin;
    size_t    rest = 0;



    casso.id        = kCassoRootId;
    casso.kind      = TreeNode::Kind::CassoRoot;
    casso.label     = GetRootLabel (kCassoRootId);
    casso.location  = Location::MakeRoot (kCassoRootId);
    casso.canExpand = true;

    pc.id        = kThisPcRootId;
    pc.kind      = TreeNode::Kind::ThisPcRoot;
    pc.label     = GetRootLabel (kThisPcRootId);
    pc.location  = Location::MakeRoot (kThisPcRootId);
    pc.canExpand = true;

    //  Explorer's navigation pane shows the Recycle Bin beside This PC once
    //  it shows all folders; it holds no folders to expand.
    bin.id       = Location::kRecycleBinId;
    bin.kind     = TreeNode::Kind::RecycleBinRoot;
    bin.label    = GetRootLabel (Location::kRecycleBinId);
    bin.location = Location::MakeRecycleBin();

    outNodes.clear();
    outNodes.push_back (casso);

    //  Without the shell's roots: This PC under its line, then the bin.
    if (m_navRoots.empty())
    {
        if (IsNavRootShown (NavRoot::ThisPc))
        {
            outNodes.push_back (pc);
        }

        if (m_navOptions.showAllFolders)
        {
            outNodes.push_back (bin);
        }

        if (outNodes.size() > 1)
        {
            outNodes[1].dividerAbove = true;
        }

        return;
    }

    //  Explorer's order: Home, Gallery and OneDrive; a line; the pinned
    //  folders; a line; the rest, with Casso's own This PC where the shell's
    //  is; the Recycle Bin last, as with all folders shown.
    for (const IShellItemVerbs::ShellFolderItem & root : m_navRoots)
    {
        if (root.leading)
        {
            outNodes.push_back (MakeShellNode (root));
        }
    }

    for (size_t i = 0; i < m_pinned.size(); i++)
    {
        outNodes.push_back (MakeShellNode (m_pinned[i]));
        outNodes.back().dividerAbove = i == 0;
    }

    rest = outNodes.size();

    for (const IShellItemVerbs::ShellFolderItem & root : m_navRoots)
    {
        bool  hidden = (root.isThisPc    && !IsNavRootShown (NavRoot::ThisPc))
                    || (root.isNetwork   && !IsNavRootShown (NavRoot::Network))
                    || (root.isLibraries && !IsNavRootShown (NavRoot::Libraries));

        if (root.leading || root.isRecycleBin || hidden)
        {
            continue;
        }

        outNodes.push_back (root.isThisPc ? pc : MakeShellNode (root));
    }

    if (m_navOptions.showAllFolders)
    {
        outNodes.push_back (bin);
    }

    if (rest < outNodes.size())
    {
        outNodes[rest].dividerAbove = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::RefreshShellRoots
//
////////////////////////////////////////////////////////////////////////////////

void TreeModel::RefreshShellRoots()
{
    HRESULT  hr = S_OK;



    m_navRoots.clear();
    m_pinned.clear();

    if (m_shellVerbs == nullptr)
    {
        return;
    }

    hr = m_shellVerbs->ListNavigationRoots (m_navRoots);

    if (FAILED (hr))
    {
        m_navRoots.clear();
        return;
    }

    hr = m_shellVerbs->ListPinnedFolders (m_pinned);

    if (FAILED (hr))
    {
        m_pinned.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::IsPinnedFolder
//
////////////////////////////////////////////////////////////////////////////////

bool TreeModel::IsPinnedFolder (const std::wstring & path) const
{
    return std::any_of (m_pinned.begin(), m_pinned.end(), [&path] (const IShellItemVerbs::ShellFolderItem & folder)
    {
        return _wcsicmp (folder.path.c_str(), path.c_str()) == 0;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::MakeShellNode
//
////////////////////////////////////////////////////////////////////////////////

TreeNode TreeModel::MakeShellNode (const IShellItemVerbs::ShellFolderItem & item)
{
    TreeNode  node;



    node.label  = item.name;
    node.iconId = item.iconId;

    if (!item.path.empty() && !item.isFile)
    {
        node.id        = MakeFolderId (false, item.path);
        node.kind      = TreeNode::Kind::HostFolder;
        node.location  = Location::MakeHostFolder (item.path);
        node.canExpand = true;
    }
    else
    {
        node.id        = std::wstring (kShellRootTag) + item.id;
        node.kind      = TreeNode::Kind::ShellRoot;
        node.location  = Location::MakeShellFolder (item.id, item.name);
        node.canExpand = item.hasSubfolders;
    }

    return node;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::ListShellFolders
//
//  The folders under a shell root, as Explorer's tree lists them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeModel::ListShellFolders (const std::wstring & id, std::vector<TreeNode> & outNodes)
{
    HRESULT                                        hr      = S_OK;
    std::vector<IShellItemVerbs::ShellFolderItem>  items;
    bool                                           fetched = false;



    CBREx (m_shellVerbs != nullptr || m_shellFetch, E_NOTIMPL);

    //  Still being read: nothing yet, and nothing kept, so the next ask
    //  takes it.
    if (m_shellFetch)
    {
        fetched = m_shellFetch (id, items);
        CBREx (fetched, S_FALSE);   // EHM-ALLOW-SFALSE: still being read; GetChildren keeps nothing for it
    }
    else
    {
        hr = m_shellVerbs->ListShellFolder (id, items);
        CHR (hr);
    }

    for (const IShellItemVerbs::ShellFolderItem & item : items)
    {
        if (item.isFolder && !(item.isFile && !item.path.empty() && IsSupportedImage (item.path)))
        {
            outNodes.push_back (MakeShellNode (item));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::GetRootLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TreeModel::GetRootLabel (const std::wstring & id)
{
    if (id == kCassoRootId)
    {
        return L"Casso";
    }

    if (id == kThisPcRootId)
    {
        return L"This PC";
    }

    if (id == Location::kRecycleBinId)
    {
        return L"Recycle Bin";
    }

    return std::wstring();
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
//  TreeModel::GetKnownFolderLabels
//
//  A folder shows its own name, or its full path where another known folder
//  has the same name.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> TreeModel::GetKnownFolderLabels (const std::vector<std::wstring> & paths)
{
    std::vector<std::wstring>  leaves (paths.size());
    std::vector<std::wstring>  labels (paths.size());



    for (size_t i = 0; i < paths.size(); i++)
    {
        std::wstring  rest  = paths[i];
        size_t        slash = 0;

        while (rest.size() > 3 && rest.back() == L'\\')
        {
            rest.pop_back();
        }

        slash     = rest.rfind (L'\\');
        leaves[i] = (slash != std::wstring::npos && slash + 1 < rest.size()) ? rest.substr (slash + 1) : rest;
        labels[i] = leaves[i];
    }

    for (size_t i = 0; i < paths.size(); i++)
    {
        for (size_t j = 0; j < paths.size(); j++)
        {
            if (i != j && _wcsicmp (leaves[i].c_str(), leaves[j].c_str()) == 0 && _wcsicmp (paths[i].c_str(), paths[j].c_str()) != 0)
            {
                labels[i] = paths[i];
                break;
            }
        }
    }

    return labels;
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
    const std::vector<std::wstring> & paths  = underCasso ? m_knownFolders : m_drives;
    std::vector<std::wstring>         labels = underCasso ? GetKnownFolderLabels (paths) : paths;
    size_t                            i      = 0;



    outNodes.clear();

    for (i = 0; i < paths.size(); i++)
    {
        const std::wstring &  path   = paths[i];
        TreeNode              node;
        bool                  exists = !m_directoryProbe || m_directoryProbe (path);

        node.id        = MakeFolderId (underCasso, path);
        node.kind      = underCasso ? TreeNode::Kind::KnownFolder : TreeNode::Kind::Drive;
        node.label     = labels[i];
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
    MountDiagnosis         diagnosis;
    bool                   loaded     = false;
    VolumeKind             kind       = VolumeKind::Unknown;
    std::vector<TreeNode>  children;
    std::string            narrowPath = TextEncoding::WideToNarrow (path);



    inOutNode.canExpand = false;
    inOutNode.broken    = false;
    inOutNode.loadError.clear();

    hr = m_fs.ReadAllText (path, content);

    if (SUCCEEDED (hr))
    {
        fileBytes.assign (content.begin(), content.end());

        hr     = VolumeImage::Load (fileBytes, narrowPath, sectors, report, diagnosis);
        loaded = SUCCEEDED (hr);
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
        //  A file the loader refused says why, as the list does; one that
        //  loaded holds no file system this browser reads.
        inOutNode.loadError = BreakAfterFirstSentence (GetLeaf (path) + L" "
                            + TextEncoding::NarrowToWide (loaded ? std::string (DiskImageSession::kNoFilesystemText) : diagnosis.Describe())
                            + L".");
        inOutNode.broken         = !loaded;
        m_children[inOutNode.id] = children;

        return S_OK;
    }

    if (kind == VolumeKind::ProDos)
    {
        ListDirectories (underCasso, path, sectors, std::string(), children);
    }

    inOutNode.canExpand      = !children.empty();
    m_children[inOutNode.id] = children;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::ListDirectories
//
//  The subdirectories of one directory inside a ProDOS image. Each is listed
//  one level deeper, so only a directory that contains subdirectories can
//  expand. A directory whose listing fails has no children.
//
////////////////////////////////////////////////////////////////////////////////

void TreeModel::ListDirectories (
    bool                      underCasso,
    const std::wstring      & path,
    const std::vector<Byte> & sectors,
    const std::string       & inner,
    std::vector<TreeNode>   & outNodes)
{
    ProDosVolume                          volume (sectors);
    VolumeListing                            listing;
    VolumeListing                            below;
    std::unordered_map<std::string, size_t>  seen;
    HRESULT                                  hr      = volume.EnumerateDirectory (FilePath::Parse (inner), listing);



    outNodes.clear();

    if (FAILED (hr))
    {
        return;
    }

    for (const FileEntry & entry : listing.entries)
    {
        TreeNode     child;
        std::string  childInner = inner.empty() ? entry.name : inner + "/" + entry.name;
        bool         hasSubdir  = false;

        if (!entry.isDirectory)
        {
            continue;
        }

        below = VolumeListing();
        hr    = volume.EnumerateDirectory (FilePath::Parse (childInner), below);

        for (const FileEntry & grandchild : below.entries)
        {
            hasSubdir = hasSubdir || (SUCCEEDED (hr) && grandchild.isDirectory);
        }

        child.id        = MakeDirectoryId (underCasso, path, childInner, seen[entry.name]++);
        child.kind      = TreeNode::Kind::DiskDirectory;
        child.label     = std::wstring (entry.name.begin(), entry.name.end());
        child.location  = Location::MakeDiskDirectory (path, childInner);
        child.canExpand = hasSubdir;

        outNodes.push_back (child);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::ListImageDirectory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeModel::ListImageDirectory (bool underCasso, const std::wstring & path, const std::string & inner, std::vector<TreeNode> & outNodes)
{
    HRESULT             hr        = S_OK;
    std::string         content;
    std::vector<Byte>   fileBytes;
    std::vector<Byte>   sectors;
    SectorDecodeReport  report;
    bool                isProDos  = false;



    outNodes.clear();

    hr = m_fs.ReadAllText (path, content);
    CHR (hr);

    fileBytes.assign (content.begin(), content.end());

    hr = VolumeImage::Load (fileBytes, TextEncoding::WideToNarrow (path), sectors, report);
    CHR (hr);

    isProDos = VolumeImage::DetectFilesystem (sectors) == VolumeKind::ProDos;
    CBREx (isProDos, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME));

    ListDirectories (underCasso, path, sectors, inner, outNodes);

Error:
    return hr;
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

    hr = m_fs.EnumerateAllEntries (path, entries);
    CHR (hr);

    for (const FileSystemEntry & entry : entries)
    {
        TreeNode  node;

        if (!m_folderOptions.IsShown (entry))
        {
            continue;
        }

        node.hidden = entry.isHidden;

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
               { return StrCmpLogicalW (a.label.c_str(), b.label.c_str()) < 0; });
    std::sort (images.begin(), images.end(), [] (const TreeNode & a, const TreeNode & b)
               { return StrCmpLogicalW (a.label.c_str(), b.label.c_str()) < 0; });

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

    if (id == Location::kRecycleBinId)
    {
        //  Nothing under it in the tree: its items show in the list.
    }
    else if (id.rfind (kShellRootTag, 0) == 0)
    {
        hr = ListShellFolders (id.substr (wcslen (kShellRootTag)), outNodes);
    }
    else if (id == kCassoRootId)
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
        //  An image's children are its directories, and a directory's are the
        //  directories inside it.
        if (inner.empty())
        {
            hr = ListImage (underCasso, path, outNodes);
        }
        else
        {
            hr = ListImageDirectory (underCasso, path, inner, outNodes);
        }
    }
    else
    {
        hr = E_INVALIDARG;
    }

    CHR (hr);

    if (hr != S_FALSE)
    {
        m_children[id] = outNodes;
    }

Error:
    return hr;
}

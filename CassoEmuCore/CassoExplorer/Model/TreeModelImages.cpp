#include "Pch.h"

#include "CassoExplorer/Model/TreeModel.h"
#include "CassoExplorer/Model/FilesystemChoices.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/AppleFileName.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::DescribeImage
//
//  Reads the image once. A ProDOS volume's subdirectories become the node's
//  children and give it an expand affordance; a DOS 3.3 volume has none; a
//  file that will not parse gets the error as its tooltip and no affordance.
//  The partitions of a partitioned image follow the directories, and a file
//  that has partitions is not an error.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeModel::DescribeImage (bool underCasso, const std::wstring & path, TreeNode & inOutNode)
{
    HRESULT                hr         = S_OK;
    std::vector<Byte>      fileBytes;
    std::string            name;
    std::vector<Byte>      sectors;
    SectorDecodeReport     report;
    MountDiagnosis         diagnosis;
    bool                   loaded     = false;
    VolumeKind             kind       = VolumeKind::Unknown;
    std::vector<TreeNode>  children;
    std::vector<TreeNode>  partitions;



    inOutNode.canExpand = false;
    inOutNode.broken    = false;
    inOutNode.loadError.clear();

    hr = ReadImage (path, fileBytes, name);

    if (SUCCEEDED (hr))
    {
        ListPartitions (underCasso, path, fileBytes, name, partitions);

        hr     = VolumeImage::Load (fileBytes, name, sectors, report, diagnosis);
        loaded = SUCCEEDED (hr);
    }

    if (SUCCEEDED (hr))
    {
        kind = VolumeImage::DetectFilesystem (sectors);

        if (kind == VolumeKind::Unknown && m_filesystemChoices != nullptr)
        {
            kind = m_filesystemChoices->GetChoice (path);
        }

        if (kind == VolumeKind::Unknown)
        {
            hr = HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME);
        }
    }

    if (FAILED (hr) && partitions.empty())
    {
        //  A file the loader refused says why, as the list does; one that
        //  loaded holds no file system this browser reads.
        inOutNode.loadError = loaded ? GetLeaf (path) + L" " + kNoFilesystemText
                                     : BreakAfterFirstSentence (GetLeaf (path) + L" " + TextEncoding::NarrowToWide (diagnosis.Describe()) + L".");
        inOutNode.broken         = !loaded;
        m_children[inOutNode.id] = children;

        return S_OK;
    }

    if (kind == VolumeKind::ProDos)
    {
        ListDirectories (underCasso, path, sectors, std::string(), children);
    }

    children.insert (children.end(), partitions.begin(), partitions.end());

    inOutNode.canExpand      = !children.empty();
    m_children[inOutNode.id] = children;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::ReadImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeModel::ReadImage (const std::wstring & path, std::vector<Byte> & outBytes, std::string & outName)
{
    HRESULT          hr       = S_OK;
    std::string      narrow   = TextEncoding::WideToNarrow (path);
    bool             isNested = ContainerPath::IsNested (narrow);
    std::string      content;
    std::string      error;
    OpenedContainer  opened;



    outBytes.clear();
    outName = narrow;

    if (isNested)
    {
        hr = ContainerPath::Open (ContainerPath::Parse (narrow), m_fs, opened, error);
        CHR (hr);

        outBytes = std::move (opened.bytes);
        outName  = opened.name;
    }
    else
    {
        hr = m_fs.ReadAllText (path, content);
        CHR (hr);

        outBytes.assign (content.begin(), content.end());
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel::ListPartitions
//
//  A ProDOS partition has an expander before its directories are read: they
//  are read when the node is expanded. A partition that ends past the end of
//  the image is listed as damaged.
//
////////////////////////////////////////////////////////////////////////////////

void TreeModel::ListPartitions (
    bool                      underCasso,
    const std::wstring      & path,
    const std::vector<Byte> & fileBytes,
    const std::string       & name,
    std::vector<TreeNode>   & outNodes)
{
    PartitionLayoutKind          layout  = PartitionLayoutKind::None;
    std::vector<PartitionEntry>  entries;
    HRESULT                      hr      = PartitionAccess::List (fileBytes, name, layout, entries);



    outNodes.clear();

    if (FAILED (hr))
    {
        return;
    }

    for (const PartitionEntry & entry : entries)
    {
        TreeNode      node;
        std::wstring  partitionPath = path + kSeparator + TextEncoding::NarrowToWide (entry.step);

        node.id        = MakeImageId (underCasso, partitionPath);
        node.kind      = TreeNode::Kind::DiskImage;
        node.label     = TextEncoding::NarrowToWide (PartitionAccess::GetLabel (entry, true));
        node.location  = Location::MakeDiskImage (partitionPath);
        node.canExpand = !entry.partition.damaged && entry.partition.volumeKind == VolumeKind::ProDos;

        if (entry.partition.damaged)
        {
            node.broken    = true;
            node.loadError = TextEncoding::NarrowToWide (PartitionAccess::GetLabel (entry, false)) + L" " + kDamagedPartitionText;
        }

        outNodes.push_back (node);
    }
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
        child.label     = AppleFileName::ToDisplay (entry.name);
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
    std::string         name;
    std::vector<Byte>   fileBytes;
    std::vector<Byte>   sectors;
    SectorDecodeReport  report;
    bool                isProDos  = false;



    outNodes.clear();

    hr = ReadImage (path, fileBytes, name);
    CHR (hr);

    hr = VolumeImage::Load (fileBytes, name, sectors, report);
    CHR (hr);

    isProDos = VolumeImage::DetectFilesystem (sectors) == VolumeKind::ProDos;
    CBREx (isProDos, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME));

    ListDirectories (underCasso, path, sectors, inner, outNodes);

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

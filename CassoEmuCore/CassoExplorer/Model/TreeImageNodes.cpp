#include "Pch.h"

#include "CassoExplorer/Model/TreeImageNodes.h"
#include "CassoExplorer/Model/DiskImageCatalog.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/AppleFileName.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TreeImageNodes::Describe
//
//  A volume's directories are the node's children, and a node with children
//  has an expander. A file that will not open gets the error as its tooltip
//  and no expander. The partitions of a partitioned image follow the
//  directories, and a file that has partitions is not an error. Volumes
//  inside a volume, such as DOS Master's, are volumes; any other image's are
//  partitions. Without a catalog, the node has no children.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeImageNodes::Describe (bool underCasso, const std::wstring & path, TreeNode & inOutNode, std::vector<TreeNode> & outChildren)
{
    HRESULT                hr = S_OK;
    ImageContents          contents;
    std::vector<TreeNode>  partitions;



    inOutNode.canExpand = false;
    inOutNode.broken    = false;
    inOutNode.loadError.clear();
    outChildren.clear();

    BAIL_OUT_IF (m_catalog == nullptr, S_OK);

    hr = m_catalog->Describe (path, std::string(), contents);

    AddPartitionNodes (underCasso, path, contents, partitions);

    if (FAILED (hr) && partitions.empty())
    {
        inOutNode.loadError = contents.error;
        inOutNode.broken    = contents.broken;
        BAIL_OUT_IF (true, S_OK);
    }

    hr = S_OK;

    AddDirectoryNodes (underCasso, path, contents.subdirectories, outChildren);
    outChildren.insert (outChildren.end(), partitions.begin(), partitions.end());

    inOutNode.canExpand = !outChildren.empty();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeImageNodes::ListImage
//
//  The children of an image expanded before its folder was listed, such as
//  a restored tab's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeImageNodes::ListImage (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes)
{
    TreeNode  image;



    image.id       = TreeModel::MakeImageId (underCasso, path);
    image.kind     = TreeNode::Kind::DiskImage;
    image.label    = TreeModel::GetLeaf (path);
    image.location = Location::MakeDiskImage (path);

    return Describe (underCasso, path, image, outNodes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeImageNodes::ListDirectory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeImageNodes::ListDirectory (bool underCasso, const std::wstring & path, const std::string & inner, std::vector<TreeNode> & outNodes)
{
    HRESULT        hr = S_OK;
    ImageContents  contents;



    outNodes.clear();

    CBREx (m_catalog != nullptr, E_UNEXPECTED);

    hr = m_catalog->Describe (path, inner, contents);
    CHR (hr);

    AddDirectoryNodes (underCasso, path, contents.subdirectories, outNodes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeImageNodes::ListPartitionVolumes
//
//  The volume in a partition, or for a partition with no file system Casso
//  reads, the partitions inside it, such as an 800K UniDOS disk's two
//  volumes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TreeImageNodes::ListPartitionVolumes (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes)
{
    HRESULT                hr     = S_OK;
    auto                   found  = m_partitions.find (path);
    PartitionInfo          info   = (found != m_partitions.end()) ? found->second : PartitionInfo { TreeModel::GetLeaf (path), L"Untitled", true, false };
    TreeNode               volume = MakeVolumeNode (underCasso, path, info.volumeLabel, info.isProDos);
    std::vector<TreeNode>  inside;



    outNodes = { volume };

    if (!info.hasFileSystem)
    {
        hr = Describe (underCasso, path, volume, inside);
        CHR (hr);

        if (!inside.empty())
        {
            outNodes = inside;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeImageNodes::GetPartitionLabel
//
//  A partition's label and its volume's, once the image it is in is listed,
//  or empty.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TreeImageNodes::GetPartitionLabel (const std::wstring & path) const
{
    auto  found = m_partitions.find (path);



    return found == m_partitions.end() ? std::wstring() : found->second.label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeImageNodes::GetVolumeLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TreeImageNodes::GetVolumeLabel (const std::wstring & path) const
{
    auto  found = m_partitions.find (path);



    return found == m_partitions.end() ? std::wstring() : found->second.volumeLabel;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeImageNodes::AddPartitionNodes
//
//  A partition is a node with the volumes in it below it, and a volume
//  inside a volume is a volume. A ProDOS volume has an expander before its
//  directories are read: they are read when the node is expanded. A
//  partition that ends past the end of the image is listed as damaged.
//
////////////////////////////////////////////////////////////////////////////////

void TreeImageNodes::AddPartitionNodes (bool underCasso, const std::wstring & path, const ImageContents & contents, std::vector<TreeNode> & outNodes)
{
    bool  isInsideVolume = contents.kind != VolumeKind::Unknown;



    outNodes.clear();

    for (const PartitionEntry & entry : contents.partitions)
    {
        TreeNode       node;
        std::wstring   partitionPath = path + L"|" + TextEncoding::NarrowToWide (entry.step);
        std::wstring   volumeLabel   = TextEncoding::NarrowToWide (PartitionAccess::GetVolumeLabel (entry));
        bool           isProDos      = !entry.partition.damaged && entry.partition.volumeKind == VolumeKind::ProDos;
        PartitionInfo  info;

        if (isInsideVolume)
        {
            node = MakeVolumeNode (underCasso, partitionPath, volumeLabel, isProDos);
        }
        else
        {
            info.label         = DiskImageCatalog::GetPartitionLabel (path, entry, false);
            info.volumeLabel   = volumeLabel;
            info.hasFileSystem = entry.partition.volumeKind != VolumeKind::Unknown || entry.partition.isHfs;
            info.isProDos      = isProDos;

            m_partitions[partitionPath] = info;

            node.id        = TreeModel::MakePartitionId (underCasso, partitionPath);
            node.kind      = TreeNode::Kind::Partition;
            node.label     = info.label;
            node.location  = Location::MakePartition (partitionPath);
            node.canExpand = !entry.partition.damaged;
        }

        if (entry.partition.damaged)
        {
            node.broken    = true;
            node.loadError = DiskImageCatalog::DescribeDamagedPartition (path, entry);
        }

        outNodes.push_back (node);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeImageNodes::AddDirectoryNodes
//
//  Each directory can expand only when it contains subdirectories. Two
//  entries with the same name, as a damaged directory can have, get
//  different ids.
//
////////////////////////////////////////////////////////////////////////////////

void TreeImageNodes::AddDirectoryNodes (bool underCasso, const std::wstring & path, const std::vector<ImageSubdirectory> & subdirectories, std::vector<TreeNode> & outNodes)
{
    std::unordered_map<std::string, size_t>  seen;



    for (const ImageSubdirectory & subdirectory : subdirectories)
    {
        TreeNode  child;

        child.id        = TreeModel::MakeDirectoryId (underCasso, path, subdirectory.path, seen[subdirectory.name]++);
        child.kind      = TreeNode::Kind::DiskDirectory;
        child.label     = AppleFileName::ToDisplay (subdirectory.name);
        child.location  = Location::MakeDiskDirectory (path, subdirectory.path);
        child.canExpand = subdirectory.hasSubdirectories;

        outNodes.push_back (child);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TreeImageNodes::MakeVolumeNode
//
////////////////////////////////////////////////////////////////////////////////

TreeNode TreeImageNodes::MakeVolumeNode (bool underCasso, const std::wstring & path, const std::wstring & label, bool canExpand)
{
    TreeNode  node;



    node.id        = TreeModel::MakeImageId (underCasso, path);
    node.kind      = TreeNode::Kind::DiskImage;
    node.label     = label;
    node.location  = Location::MakeDiskImage (path);
    node.canExpand = canExpand;

    return node;
}

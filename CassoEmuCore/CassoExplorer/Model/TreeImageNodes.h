#pragma once

#include "Pch.h"

class  DiskImageCatalog;
struct ImageContents;
struct ImageSubdirectory;
struct TreeNode;





////////////////////////////////////////////////////////////////////////////////
//
//  TreeImageNodes
//
//  The tree's nodes inside a disk image: its directories, its partitions,
//  and the volumes in a partition, read through the catalog.
//
////////////////////////////////////////////////////////////////////////////////

class TreeImageNodes
{
public:
    void  SetCatalog (DiskImageCatalog * catalog) { m_catalog = catalog; }

    HRESULT  Describe             (bool underCasso, const std::wstring & path, TreeNode & inOutNode, std::vector<TreeNode> & outChildren);
    HRESULT  ListImage            (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes);
    HRESULT  ListDirectory        (bool underCasso, const std::wstring & path, const std::string & inner, std::vector<TreeNode> & outNodes);
    HRESULT  ListPartitionVolumes (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes);

    std::wstring  GetPartitionLabel (const std::wstring & path) const;
    std::wstring  GetVolumeLabel    (const std::wstring & path) const;

private:
    struct PartitionInfo
    {
        std::wstring  label;
        std::wstring  volumeLabel;
        bool          hasFileSystem = false;
        bool          isProDos      = false;
    };

    void  AddPartitionNodes (bool underCasso, const std::wstring & path, const ImageContents & contents, std::vector<TreeNode> & outNodes);

    static void      AddDirectoryNodes (bool underCasso, const std::wstring & path, const std::vector<ImageSubdirectory> & subdirectories, std::vector<TreeNode> & outNodes);
    static TreeNode  MakeVolumeNode    (bool underCasso, const std::wstring & path, const std::wstring & label, bool canExpand);

    DiskImageCatalog                       * m_catalog = nullptr;
    std::map<std::wstring, PartitionInfo>    m_partitions;
};

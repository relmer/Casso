#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"

class CassoExplorerActions;
struct PartitionEntry;





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsPartition
//
//  One partition of a card image: its blocks written beside the card image
//  as an image file, and an image file written over it.
//
//  A partition's path is the card image's path, a bar and the partition's
//  step of a container path.
//
////////////////////////////////////////////////////////////////////////////////

class ActionsPartition
{
public:
    explicit ActionsPartition (CassoExplorerActions & actions) : m_actions (actions) {}

    //  The partition selected in a list of partitions, or the partition whose
    //  volume directory is open. False for anything else.
    bool  TryGetTarget (std::wstring & outPartitionPath) const;

    ActionsTypes::Outcome  CopyToImage (const std::wstring & partitionPath, std::wstring & outPath);
    ActionsTypes::Outcome  Replace     (const std::wstring & partitionPath, const std::wstring & imagePath);

    //  The question a replace is confirmed with, which lists the partition
    //  and the image. An empty text and a failed outcome for a partition or
    //  an image that cannot be read.
    ActionsTypes::Outcome  DescribeReplace (const std::wstring & partitionPath, const std::wstring & imagePath, std::wstring & outQuestion);

private:
    HRESULT  FindEntry (const std::wstring & partitionPath, PartitionEntry & outEntry);

    static bool  TrySplit (const std::wstring & partitionPath, std::wstring & outParent, int & outIndex);

    CassoExplorerActions & m_actions;
};

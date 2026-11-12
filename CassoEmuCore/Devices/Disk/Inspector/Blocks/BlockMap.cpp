#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockMap.h"
#include "Machines/Apple2/Common/HfsVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMap::GetBlocksOf
//
////////////////////////////////////////////////////////////////////////////////

vector<uint32_t> BlockMap::GetBlocksOf (int file) const
{
    vector<uint32_t>  found;
    uint32_t          block = 0;



    for (block = 0; block < GetBlockCount(); block++)
    {
        const vector<int> &  owners = blocks[block].owners;

        if (std::find (owners.begin(), owners.end(), file) != owners.end())
        {
            found.push_back (block);
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMap::DescribeOwners
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockMap::DescribeOwners (uint32_t block) const
{
    std::wstring  text;



    if (block >= GetBlockCount())
    {
        return text;
    }

    for (int owner : blocks[block].owners)
    {
        if (owner < 0 || owner >= (int) files.size())
        {
            continue;
        }

        if (!text.empty())
        {
            text += L"\n";
        }

        text += files[owner];
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMap::GetNodeBlock
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockMap::GetNodeBlock (uint32_t tree, uint32_t node) const
{
    const vector<uint32_t> *  nodes = nullptr;



    if (tree == HfsVolume::kCatalogFileId)
    {
        nodes = &catalogNodes;
    }
    else if (tree == HfsVolume::kExtentsFileId)
    {
        nodes = &extentsNodes;
    }

    if (nodes == nullptr || node >= nodes->size())
    {
        return kNoBlock;
    }

    return (*nodes)[node];
}

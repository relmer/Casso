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
//  BlockMap::GetBlocksOfEach
//
////////////////////////////////////////////////////////////////////////////////

vector<vector<uint32_t>> BlockMap::GetBlocksOfEach() const
{
    vector<vector<uint32_t>>  found (files.size());
    uint32_t                  block = 0;



    for (block = 0; block < GetBlockCount(); block++)
    {
        for (int owner : blocks[block].owners)
        {
            if (owner >= 0 && (size_t) owner < found.size())
            {
                found[owner].push_back (block);
            }
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMap::CompleteEntries
//
//  A file the walk read no entry for gets an entry whose name is the
//  path's last part.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMap::CompleteEntries()
{
    size_t  i = entries.size();



    entries.resize (files.size());

    for (; i < files.size(); i++)
    {
        entries[i].name = files[i].substr (files[i].find_last_of (L'/') + 1);
    }
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





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMap::GetPriority
//
////////////////////////////////////////////////////////////////////////////////

int BlockMap::GetPriority (SectorRole role)
{
    static constexpr SectorRole  kOrder[] =
    {
        SectorRole::Free,
        SectorRole::FileData,
        SectorRole::UnusedCatalogSector,
        SectorRole::IndexBlock,
        SectorRole::Subdirectory,
        SectorRole::CatalogOrDirectory,
        SectorRole::VolumeBitmap,
        SectorRole::BootArea,
        SectorRole::VtocOrKeyBlock,
        SectorRole::BadBlocksFile,
        SectorRole::AllocatedUnowned,
        SectorRole::OwnedMarkedFree,
        SectorRole::CrossLinked,
    };



    const SectorRole  * found = std::find (std::begin (kOrder), std::end (kOrder), role);



    return found == std::end (kOrder) ? 0 : (int) (found - std::begin (kOrder));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMap::PickRole
//
////////////////////////////////////////////////////////////////////////////////

SectorRole BlockMap::PickRole (SectorRole a, SectorRole b)
{
    return GetPriority (b) > GetPriority (a) ? b : a;
}

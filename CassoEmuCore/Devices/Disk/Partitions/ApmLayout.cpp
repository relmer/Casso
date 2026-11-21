#include "Pch.h"

#include "Devices/Disk/Partitions/ApmLayout.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/Blocks/ApplePartitionMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ApmLayout::TryDetect
//
//  The entries are in the map's order. An empty entry, the
//  Apple_partition_map entry and free space aren't partitions. Each is added
//  with the count in the map, so one that ends past the end of the store is
//  marked damaged.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ApmLayout::TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions)
{
    HRESULT                           hr      = S_OK;
    vector<ApplePartitionMap::Entry>  entries;
    bool                              isFound = false;



    outPartitions.clear();

    isFound = ApplePartitionMap::TryRead (store, entries);
    BAIL_OUT_IF (!isFound, S_OK);

    for (const ApplePartitionMap::Entry & entry : entries)
    {
        if (entry.mapBlockCount == 0 || entry.type == L"Apple_partition_map" || entry.type == L"Apple_Free")
        {
            continue;
        }

        hr = PartitionLayout::AppendBlockPartition (store, PartitionLayoutKind::Apm, entry.firstBlock, entry.mapBlockCount,
                                                    TextEncoding::WideToUtf8 (entry.name), outPartitions);
        CHR (hr);

        outPartitions.back().type = TextEncoding::WideToUtf8 (entry.type);
    }

Error:
    return hr;
}

#include "Pch.h"

#include "Devices/Disk/Partitions/FocusDriveLayout.h"





static constexpr char      s_kSignature[]   = "Parsons Engin.";
static constexpr size_t    s_kSignatureLen  = sizeof (s_kSignature) - 1;
static constexpr size_t    s_kOffCount      = 0x0F;
static constexpr size_t    s_kOffEntries    = 0x20;
static constexpr size_t    s_kEntryBytes    = 16;
static constexpr size_t    s_kEntOffSize    = 4;
static constexpr size_t    s_kOffNames      = IBlockStore::kBlockBytes + 0x20;
static constexpr size_t    s_kNameBytes     = 32;
static constexpr int       s_kMaxPartitions = 30;
static constexpr uint32_t  s_kMapBlocks     = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  FocusDriveLayout::TryDetect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FocusDriveLayout::TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions)
{
    HRESULT            hr    = S_OK;
    std::vector<Byte>  map;
    bool               found = false;
    int                count = 0;
    int                i     = 0;



    outPartitions.clear();

    BAIL_OUT_IF (store.GetBlockCount() < s_kMapBlocks, S_OK);

    hr = PartitionLayout::ReadBlocks (store, 0, s_kMapBlocks, map);
    CHR (hr);

    found = std::equal (s_kSignature, s_kSignature + s_kSignatureLen, map.begin());
    count = map[s_kOffCount];

    BAIL_OUT_IF (!found || count == 0 || count > s_kMaxPartitions, S_OK);

    for (i = 0; i < count; i++)
    {
        size_t       entry = s_kOffEntries + (size_t) i * s_kEntryBytes;
        size_t       label = s_kOffNames + (size_t) i * s_kNameBytes;
        std::string  name;
        size_t       n     = 0;

        for (n = 0; n < s_kNameBytes && map[label + n] >= 0x20 && map[label + n] < 0x7F; n++)
        {
            name += (char) map[label + n];
        }

        hr = PartitionLayout::AppendBlockPartition (store,
                                                    PartitionLayoutKind::FocusDrive,
                                                    PartitionLayout::ReadLong (map, entry),
                                                    PartitionLayout::ReadLong (map, entry + s_kEntOffSize),
                                                    name,
                                                    outPartitions);
        CHR (hr);
    }

Error:
    return hr;
}
#include "Pch.h"

#include "Devices/Disk/Partitions/CffaLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CffaLayout::Read
//
//  Up to `slotCount` slots from `first`, each clipped to the end of the
//  store. A slot is recognized when its volume's block count fits in it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CffaLayout::Read (IBlockStore & store, uint32_t first, uint32_t slotBlocks, int slotCount, Reading & outReading)
{
    HRESULT     hr           = S_OK;
    uint32_t    total        = store.GetBlockCount();
    uint32_t    blocks       = 0;
    uint32_t    volumeBlocks = 0;
    VolumeKind  kind         = VolumeKind::Unknown;
    bool        isHfs        = false;
    bool        known        = false;



    outReading.next = first;

    for (int slot = 0; slot < slotCount && outReading.next < total; slot++)
    {
        blocks = (std::min) (slotBlocks, total - outReading.next);

        hr = PartitionLayout::ProbeBlockVolume (store, outReading.next, blocks, kind, isHfs, volumeBlocks);
        CHR (hr);

        hr = PartitionLayout::AppendBlockPartition (store, PartitionLayoutKind::Cffa, outReading.next, blocks, std::string(), outReading.slots);
        CHR (hr);

        known = (kind != VolumeKind::Unknown || isHfs) && volumeBlocks <= blocks;

        outReading.recognized.push_back (known);
        outReading.found += known ? 1 : 0;
        outReading.next  += blocks;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CffaLayout::TryDetect
//
//  The first four slots, then the rest read as 1 GB volumes and as 32 MB
//  slots. The reading with more recognized volumes is the layout, and the
//  1 GB one on a tie. Slots after the last recognized one are dropped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CffaLayout::TryDetect (IBlockStore & store, std::vector<Partition> & outPartitions)
{
    HRESULT   hr    = S_OK;
    uint32_t  total = store.GetBlockCount();
    Reading   card;
    Reading   wide;
    Reading   slices;
    Reading * rest  = nullptr;
    size_t    keep  = 0;



    outPartitions.clear();

    BAIL_OUT_IF (total <= kSlotBlocks, S_OK);

    hr = Read (store, 0, kSlotBlocks, 4, card);
    CHR (hr);

    BAIL_OUT_IF (card.found == 0, S_OK);

    if (card.next < total)
    {
        hr = Read (store, card.next, kLargeBlocks, 2, wide);
        CHR (hr);

        hr = Read (store, card.next, kSlotBlocks, 9, slices);
        CHR (hr);

        rest = (slices.found > wide.found) ? &slices : &wide;

        card.slots.insert      (card.slots.end(),      rest->slots.begin(),      rest->slots.end());
        card.recognized.insert (card.recognized.end(), rest->recognized.begin(), rest->recognized.end());
    }

    for (size_t i = 0; i < card.slots.size(); i++)
    {
        keep = card.recognized[i] ? i + 1 : keep;
    }

    card.slots.resize (keep);

    for (size_t i = 0; i < card.slots.size(); i++)
    {
        card.slots[i].index = (int) i + 1;
    }

    outPartitions = std::move (card.slots);

Error:
    return hr;
}

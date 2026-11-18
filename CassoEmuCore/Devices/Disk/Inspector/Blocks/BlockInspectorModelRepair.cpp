#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Blocks/BlockRepair.h"
#include "Devices/Disk/Inspector/Blocks/Dos33SectorLayout.h"
#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::PlanRepair
//
//  With `chosen` empty, every change the volume can have.
//
////////////////////////////////////////////////////////////////////////////////

RepairPlan BlockInspectorModel::PlanRepair (int volume, const std::set<std::wstring> & chosen) const
{
    RepairPlan    plan;
    vector<Byte>  sectors;
    HRESULT       hr      = S_OK;



    if (volume < 0 || volume >= (int) m_volumes.size())
    {
        plan.reason = L"The volume can't be repaired in block mode.";
    }
    else if (m_volumes[volume]->isSectors)
    {
        const Volume  & target = *m_volumes[volume];

        hr = ReadSectors (target, sectors);

        if (FAILED (hr))
        {
            plan.reason = L"The volume can't be read.";
        }
        else
        {
            plan = chosen.empty() ? BlockRepair::PlanSectors (sectors, target.map, target.sectorsPerTrack, {})
                                  : BlockRepair::PlanSectorsChosen (sectors, target.map, target.sectorsPerTrack, {}, chosen);
        }
    }
    else
    {
        const Volume  & target = *m_volumes[volume];

        plan = chosen.empty() ? BlockRepair::Plan (*target.image, target.map, target.first, {})
                              : BlockRepair::PlanChosen (*target.image, target.map, target.first, {}, chosen);
    }

    return plan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::ReadSectors
//
//  A DOS volume's sectors in track and sector order, from its blocks: one in
//  a half of each block, or two to a block.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::ReadSectors (const Volume & volume, vector<Byte> & outSectors) const
{
    HRESULT       hr     = S_OK;
    vector<Byte>  blocks ((size_t) volume.count * kBlockBytes, 0);
    uint32_t      units  = volume.map.GetBlockCount();
    uint32_t      unit   = 0;
    size_t        from   = 0;



    outSectors.assign ((size_t) units * Dos33SectorLayout::kSectorBytes, 0);

    hr = m_file->GetStore().ReadBlocks (volume.first, volume.count, blocks);
    CHR (hr);

    for (unit = 0; unit < units; unit++)
    {
        from = (size_t) (GetUnitBlock (volume, unit) - volume.first) * kBlockBytes + GetSectorOffset (volume, unit);

        if (from + Dos33SectorLayout::kSectorBytes <= blocks.size())
        {
            std::copy (blocks.begin() + (ptrdiff_t) from, blocks.begin() + (ptrdiff_t) (from + Dos33SectorLayout::kSectorBytes), outSectors.begin() + (ptrdiff_t) unit * Dos33SectorLayout::kSectorBytes);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::GetSectorOffset
//
//  Where a sector volume's unit is in its block.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockInspectorModel::GetSectorOffset (const Volume & volume, uint32_t unit) const
{
    return (volume.half >= 0 ? (uint32_t) volume.half : unit % 2) * Dos33SectorLayout::kSectorBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::WriteRepairTo
//
//  A copy of the image has the plan's blocks written at the volume's place
//  in it. A volume alone is read from the file a run of blocks at a time,
//  with the plan's blocks over it, and written whole.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::WriteRepairTo (
    IDiskFileIo        & fileIo,
    const std::string  & path,
    int                  volume,
    const RepairPlan   & plan,
    bool                 isVolumeOnly,
    std::wstring       & outError,
    BlockJobProgress   * progress)
{
    HRESULT                          hr     = S_OK;
    std::unique_ptr<BlockImageFile>  copy;
    vector<Byte>                     bytes;
    uint32_t                         first  = 0;
    uint32_t                         count  = 0;
    uint32_t                         at     = 0;
    uint32_t                         run    = 0;
    int                              last   = (int) m_volumes.size() - 1;
    std::wstring                     error;



    CBRAEx (plan.canRepair && volume >= 0 && volume <= last, E_INVALIDARG);

    first = m_volumes[volume]->first;
    count = m_volumes[volume]->count;

    if (m_volumes[volume]->isSectors)
    {
        hr = WriteSectorRepair (fileIo, path, *m_volumes[volume], plan, isVolumeOnly, outError);
        CHR (hr);
    }
    else if (isVolumeOnly)
    {
        bytes.assign ((size_t) count * kBlockBytes, 0);

        if (progress != nullptr)
        {
            progress->total = count;
        }

        for (at = 0; at < count; at += run)
        {
            run = (std::min) (kRunBlocks, count - at);

            hr = m_file->GetStore().ReadBlocks (first + at, run, std::span<Byte> (bytes.data() + (size_t) at * kBlockBytes, (size_t) run * kBlockBytes));
            CHRF (hr, outError = L"The volume can't be read.");

            if (progress != nullptr)
            {
                progress->done = at + run;
            }
        }

        for (const RepairBlock & block : plan.blocks)
        {
            std::copy (block.bytes.begin(), block.bytes.end(), bytes.begin() + (ptrdiff_t) block.block * kBlockBytes);
        }

        hr = fileIo.WriteAllBytes (path, bytes);
        CHRF (hr, outError = L"The repaired copy can't be written.");
    }
    else
    {
        hr = BlockImageFile::Open (fileIo, path, copy, error);
        CHRF (hr, outError = error.empty() ? L"The copy can't be opened." : error);

        for (const RepairBlock & block : plan.blocks)
        {
            hr = copy->GetStore().WriteBlocks (first + block.block, block.bytes);
            CHRF (hr, outError = L"The repaired copy can't be written.");
        }

        hr = copy->Commit();
        CHRF (hr, outError = L"The repaired copy can't be written.");
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModel::WriteSectorRepair
//
//  A DOS volume's repair: each block with a changed sector read from the
//  copy, its sectors written into it, and the block written back; or the
//  volume alone as a DOS-order image of its sectors.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorModel::WriteSectorRepair (
    IDiskFileIo        & fileIo,
    const std::string  & path,
    const Volume       & volume,
    const RepairPlan   & plan,
    bool                 isVolumeOnly,
    std::wstring       & outError)
{
    HRESULT                          hr     = S_OK;
    std::unique_ptr<BlockImageFile>  copy;
    vector<Byte>                     sectors;
    std::map<uint32_t, Block>        blocks;
    uint32_t                         unit   = 0;
    uint32_t                         block  = 0;
    std::wstring                     error;



    if (isVolumeOnly)
    {
        hr = ReadSectors (volume, sectors);
        CHRF (hr, outError = L"The volume can't be read.");

        for (const RepairSector & sector : plan.sectors)
        {
            unit = (uint32_t) (sector.track * volume.sectorsPerTrack + sector.sector);
            std::copy (sector.bytes.begin(), sector.bytes.end(), sectors.begin() + (ptrdiff_t) unit * Dos33SectorLayout::kSectorBytes);
        }

        hr = fileIo.WriteAllBytes (path, sectors);
        CHRF (hr, outError = L"The repaired copy can't be written.");
        BAIL_OUT_IF (true, S_OK);
    }

    hr = BlockImageFile::Open (fileIo, path, copy, error);
    CHRF (hr, outError = error.empty() ? L"The copy can't be opened." : error);

    for (const RepairSector & sector : plan.sectors)
    {
        unit  = (uint32_t) (sector.track * volume.sectorsPerTrack + sector.sector);
        block = GetUnitBlock (volume, unit);

        if (!blocks.contains (block))
        {
            hr = copy->GetStore().ReadBlocks (block, 1, blocks[block]);
            CHRF (hr, outError = L"The copy can't be read.");
        }

        std::copy (sector.bytes.begin(), sector.bytes.end(), blocks[block].begin() + GetSectorOffset (volume, unit));
    }

    for (const auto & [at, bytes] : blocks)
    {
        hr = copy->GetStore().WriteBlocks (at, bytes);
        CHRF (hr, outError = L"The repaired copy can't be written.");
    }

    hr = copy->Commit();
    CHRF (hr, outError = L"The repaired copy can't be written.");

Error:
    return hr;
}

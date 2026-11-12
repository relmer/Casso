#include "Pch.h"

#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/BlockMapBuilder.h"
#include "Devices/Disk/Inspector/Blocks/HfsBlockLayout.h"
#include "Devices/Disk/Inspector/Blocks/PascalBlockLayout.h"
#include "Devices/Disk/Inspector/Blocks/ProDosBlockLayout.h"
#include "Machines/Apple2/Common/HfsVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapBuilder::Detect
//
////////////////////////////////////////////////////////////////////////////////

BlockFileSystem BlockMapBuilder::Detect (const BlockImage & image)
{
    uint32_t  blocks = 0;



    if (ProDosBlockLayout::IsFound (image, blocks))
    {
        return BlockFileSystem::ProDos;
    }

    if (HfsVolume::IsFound (image))
    {
        return BlockFileSystem::Hfs;
    }

    if (PascalBlockLayout::IsFound (image, blocks))
    {
        return BlockFileSystem::Pascal;
    }

    return BlockFileSystem::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapBuilder::Build
//
//  A volume of an unknown file system has every block free and no files.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapBuilder::Build (const BlockImage & image, BlockMap & outMap)
{
    BlockFileSystem        fileSystem = Detect (image);
    VolumeIntegrityReport  report;
    HRESULT                hrReport   = E_FAIL;



    switch (fileSystem)
    {
        case BlockFileSystem::ProDos:
        case BlockFileSystem::Sos:
            ProDosBlockLayout::Walk (image, fileSystem, outMap);
            hrReport = BuildProDosReport (image, report);
            break;

        case BlockFileSystem::Hfs:
        {
            HfsVolume  volume (image);

            HfsBlockLayout::Walk (image, outMap);
            hrReport = volume.BuildIntegrityReport (report);
            break;
        }

        case BlockFileSystem::Pascal:
            PascalBlockLayout::Walk (image, outMap);
            break;

        default:
            outMap = BlockMap();
            outMap.blocks.assign ((size_t) (image.size() / kBlockBytes), BlockMapEntry());
            break;
    }

    if (SUCCEEDED (hrReport))
    {
        ApplyReport (report, outMap);
    }
    else
    {
        SettleWithoutReport (outMap);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapBuilder::BuildProDosReport
//
//  The volume layer reads a 140K volume in DOS logical order and a larger
//  one in block order.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockMapBuilder::BuildProDosReport (const BlockImage & image, VolumeIntegrityReport & outReport)
{
    HRESULT       hr      = S_OK;
    vector<Byte>  sectors;



    if (image.size() == kFloppyBytes)
    {
        VolumeImage::ProDosFileToDosLogical (image.ToVector(), sectors);

        hr = ProDosVolume (sectors).BuildIntegrityReport (outReport);
    }
    else
    {
        hr = ProDosVolume (image).BuildIntegrityReport (outReport);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapBuilder::GetUnit
//
//  The report's unit a block is in: the block itself, or for HFS the
//  allocation block, and UINT32_MAX for a block before the first allocation
//  block.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockMapBuilder::GetUnit (const BlockMap & map, uint32_t block)
{
    if (map.fileSystem != BlockFileSystem::Hfs)
    {
        return block;
    }

    if (block < map.allocationStart)
    {
        return UINT32_MAX;
    }

    return (block - map.allocationStart) / map.blocksPerAllocation;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapBuilder::ApplyReport
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapBuilder::ApplyReport (const VolumeIntegrityReport & report, BlockMap & ioMap)
{
    uint32_t  block     = 0;
    uint32_t  unit      = 0;
    bool      isUnit    = false;
    size_t    claims    = 0;
    bool      allocated = false;



    for (block = 0; block < ioMap.GetBlockCount(); block++)
    {
        BlockMapEntry  & entry = ioMap.blocks[block];

        unit      = GetUnit (ioMap, block);
        isUnit    = unit < report.GetUnitCount();
        claims    = isUnit ? report.GetClaimantsOf (unit).size() : 0;
        allocated = isUnit && report.IsAllocatedInFreeMap (unit);

        if (entry.owners.size() > 1 || claims > 1)
        {
            entry.role = SectorRole::CrossLinked;
        }
        else if (entry.structure == BlockStructure::None)
        {
            entry.role = (allocated && claims == 0) ? SectorRole::AllocatedUnowned : SectorRole::Free;
        }
        else if (isUnit && !allocated && !entry.owners.empty())
        {
            entry.role = SectorRole::OwnedMarkedFree;
        }
    }

    ioMap.isCatalogComplete = ioMap.isCatalogComplete && report.IsCatalogFullyParsed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapBuilder::SettleWithoutReport
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapBuilder::SettleWithoutReport (BlockMap & ioMap)
{
    for (BlockMapEntry & entry : ioMap.blocks)
    {
        if (entry.owners.size() > 1)
        {
            entry.role = SectorRole::CrossLinked;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapBuilder::Annotate
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapBuilder::Annotate (const BlockMap & map, uint32_t block, BlockBytes bytes, vector<BlockField> & outFields)
{
    outFields.clear();

    if (block >= map.GetBlockCount())
    {
        return;
    }

    if (map.blocks[block].structure == BlockStructure::FileData)
    {
        AnnotateData (map, block, bytes, outFields);
        return;
    }

    switch (map.fileSystem)
    {
        case BlockFileSystem::ProDos:
        case BlockFileSystem::Sos:
            ProDosBlockLayout::Annotate (map, block, bytes, outFields);
            break;

        case BlockFileSystem::Hfs:
            HfsBlockLayout::Annotate (map, block, bytes, outFields);
            break;

        case BlockFileSystem::Pascal:
            PascalBlockLayout::Annotate (map, block, bytes, outFields);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapBuilder::AnnotateData
//
//  A file's data block, as its number in the file and the file's path.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapBuilder::AnnotateData (const BlockMap & map, uint32_t block, BlockBytes bytes, vector<BlockField> & outFields)
{
    BlockFieldList         list (bytes, outFields);
    const BlockMapEntry  & entry = map.blocks[block];
    std::wstring           owner = entry.owners.empty() ? std::wstring() : map.files[entry.owners[0]];
    std::wstring           fork  = (map.fileSystem == BlockFileSystem::Hfs && entry.tree != 0) ? L", resource fork" : L"";



    list.AddRun (0, kBlockBytes, L"Data, block " + std::to_wstring (entry.node) + L" of the file" + fork, owner);
}

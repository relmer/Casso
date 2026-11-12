#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"
#include "Devices/Disk/Inspector/Blocks/BlockMap.h"

class BlockImage;
class VolumeIntegrityReport;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapBuilder
//
//  The block map of a volume of blocks: its file system found, its
//  structures walked, and each block's role settled from the volume's
//  integrity report. A block two files use is cross-linked; one the free map
//  marks used that no file uses is allocated but unowned; one a file uses
//  that the free map marks free is owned but marked free. A volume with no
//  free map has only the first.
//
//  Annotate returns a block's fields by the volume's file system.
//
////////////////////////////////////////////////////////////////////////////////

class BlockMapBuilder
{
public:
    static constexpr uint32_t  kBlockBytes  = 512;
    static constexpr size_t    kFloppyBytes = 143360;

    static BlockFileSystem  Detect (const BlockImage & image);

    static void  Build    (const BlockImage & image, BlockMap & outMap);
    static void  Annotate (const BlockMap & map, uint32_t block, BlockBytes bytes, vector<BlockField> & outFields);

    //  The roles from a report whose units are the map's blocks, or for HFS
    //  its allocation blocks. Public for the tests.
    static void  ApplyReport (const VolumeIntegrityReport & report, BlockMap & ioMap);

    //  The roles of a volume with no free map: only cross-links.
    static void  SettleWithoutReport (BlockMap & ioMap);

private:
    static HRESULT   BuildProDosReport (const BlockImage & image, VolumeIntegrityReport & outReport);
    static uint32_t  GetUnit           (const BlockMap & map, uint32_t block);
    static void      AnnotateData      (const BlockMap & map, uint32_t block, BlockBytes bytes, vector<BlockField> & outFields);
};

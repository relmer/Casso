#pragma once

#include "Pch.h"

#include "Seams/RawDevice/IRawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Fat32Format
//
//  An empty FAT32 volume in one MBR partition from block 2048, the media a
//  CFFA3000 reads disk image files from. Its manual requires FAT16 or FAT32
//  with an MBR, and Windows formats a card over 32 GB as exFAT only.
//
//  The boot sector, FSInfo, FATs and cluster sizes follow Microsoft's FAT
//  specification. The volume's label is CFFA3000.
//
////////////////////////////////////////////////////////////////////////////////

class Fat32Format
{
public:
    static constexpr uint32_t  kPartitionStart  = 2048;
    static constexpr uint32_t  kReservedSectors = 32;
    static constexpr uint32_t  kFatCount        = 2;
    static constexpr uint32_t  kMinClusters     = 65525;
    static constexpr uint64_t  kMaxSectors      = 0xFFFFFFFFull;

    //  True when the drive is large enough for FAT32's minimum of clusters.
    static bool      CanFormat           (uint64_t blocks);

    //  Writes the MBR, the boot sector and its copy, FSInfo, both FATs and
    //  the root directory. The rest of the drive is left as it is.
    static HRESULT   Write               (IRawDevice & device, uint64_t blocks);

    //  An empty FAT32 volume alone, written at blocks numbered from the
    //  volume's start, as through a volume's handle, which Windows opens for
    //  the signed-in user on a removable drive.
    using BlockWriter = std::function<HRESULT (uint64_t first, std::span<const Byte> bytes)>;

    static HRESULT   WriteVolume         (const BlockWriter & write, uint64_t sectors, uint64_t hiddenSectors);
    static bool      CanFormatVolume     (uint64_t sectors);

    //  The sectors per cluster for a volume of `sectors`, from the
    //  specification's table.
    static uint32_t  GetSectorsPerCluster (uint64_t sectors);

    //  The sectors of one FAT for a volume of `sectors`.
    static uint32_t  GetFatSectors        (uint64_t sectors, uint32_t sectorsPerCluster);

private:
    static void  BuildMbr      (uint64_t sectors, std::vector<Byte> & outSector);
    static void  BuildBoot     (uint64_t sectors, uint32_t sectorsPerCluster, uint32_t fatSectors, uint32_t hiddenSectors, std::vector<Byte> & outSector);
    static void  BuildFsInfo   (std::vector<Byte> & outSector);
    static void  PutWord       (std::vector<Byte> & bytes, size_t at, uint16_t value);
    static void  PutLong       (std::vector<Byte> & bytes, size_t at, uint32_t value);
};

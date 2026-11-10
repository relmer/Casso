#include "Pch.h"

#include "Seams/RawDevice/Fat32Format.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Fat32Format::CanFormat
//
////////////////////////////////////////////////////////////////////////////////

bool Fat32Format::CanFormat (uint64_t blocks)
{
    uint64_t  sectors    = blocks > kPartitionStart ? std::min (blocks - kPartitionStart, kMaxSectors) : 0;
    uint32_t  perCluster = 0;
    uint32_t  fatSectors = 0;
    uint64_t  data       = 0;



    if (sectors <= kReservedSectors)
    {
        return false;
    }

    perCluster = GetSectorsPerCluster (sectors);
    fatSectors = GetFatSectors (sectors, perCluster);
    data       = sectors - kReservedSectors - (uint64_t) kFatCount * fatSectors;

    return sectors > kReservedSectors + (uint64_t) kFatCount * fatSectors && data / perCluster >= kMinClusters;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Fat32Format::GetSectorsPerCluster
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Fat32Format::GetSectorsPerCluster (uint64_t sectors)
{
    if (sectors <= 532480)
    {
        return 1;
    }

    if (sectors <= 16777216)
    {
        return 8;
    }

    if (sectors <= 33554432)
    {
        return 16;
    }

    if (sectors <= 67108864)
    {
        return 32;
    }

    return 64;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Fat32Format::GetFatSectors
//
//  The specification's computation, which can be a sector or so more than
//  the clusters require.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Fat32Format::GetFatSectors (uint64_t sectors, uint32_t sectorsPerCluster)
{
    uint64_t  rest    = sectors - kReservedSectors;
    uint64_t  divisor = (256ull * sectorsPerCluster + kFatCount) / 2;



    return (uint32_t) ((rest + divisor - 1) / divisor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Fat32Format::Write
//
//  The sectors before the partition are cleared, so no table or volume of
//  the drive's old layout is found there.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Fat32Format::Write (IRawDevice & device, uint64_t blocks)
{
    HRESULT            hr         = S_OK;
    uint64_t           sectors    = 0;
    uint32_t           perCluster = 0;
    uint32_t           fatSectors = 0;
    uint64_t           boot       = kPartitionStart;
    uint64_t           fat        = 0;
    uint64_t           root       = 0;
    bool               fits       = CanFormat (blocks);
    std::vector<Byte>  sector;
    std::vector<Byte>  info;
    std::vector<Byte>  reserved ((size_t) kReservedSectors * IRawDevice::kBlockBytes, 0);
    std::vector<Byte>  lead ((size_t) kPartitionStart * IRawDevice::kBlockBytes, 0);
    std::vector<Byte>  table;
    std::vector<Byte>  cluster;



    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    sectors    = std::min (blocks - kPartitionStart, kMaxSectors);
    perCluster = GetSectorsPerCluster (sectors);
    fatSectors = GetFatSectors (sectors, perCluster);
    fat        = boot + kReservedSectors;
    root       = fat + (uint64_t) kFatCount * fatSectors;

    BuildMbr (sectors, sector);
    std::copy (sector.begin(), sector.end(), lead.begin());

    hr = device.WriteBlocks (0, lead);
    CHR (hr);

    //  The boot sector at 0 and its copy at 6, FSInfo after each, and the
    //  third sector of each set ending in the signature as the boot code's.
    BuildBoot (sectors, perCluster, fatSectors, sector);
    BuildFsInfo (info);

    for (size_t copy : { (size_t) 0, (size_t) 6 })
    {
        std::copy (sector.begin(), sector.end(), reserved.begin() + (ptrdiff_t) (copy * IRawDevice::kBlockBytes));
        std::copy (info.begin(), info.end(), reserved.begin() + (ptrdiff_t) ((copy + 1) * IRawDevice::kBlockBytes));
        reserved[(copy + 2) * IRawDevice::kBlockBytes + 510] = 0x55;
        reserved[(copy + 2) * IRawDevice::kBlockBytes + 511] = 0xAA;
    }

    hr = device.WriteBlocks (boot, reserved);
    CHR (hr);

    //  Entries 0 and 1 are reserved, and 2, the root directory, is its only
    //  cluster.
    table.assign ((size_t) fatSectors * IRawDevice::kBlockBytes, 0);
    PutLong (table, 0, 0x0FFFFFF8);
    PutLong (table, 4, 0x0FFFFFFF);
    PutLong (table, 8, 0x0FFFFFFF);

    for (uint32_t copy = 0; copy < kFatCount; copy++)
    {
        hr = device.WriteBlocks (fat + (uint64_t) copy * fatSectors, table);
        CHR (hr);
    }

    cluster.assign ((size_t) perCluster * IRawDevice::kBlockBytes, 0);
    std::copy_n ("CFFA3000   ", 11, cluster.begin());
    cluster[11] = 0x08;

    hr = device.WriteBlocks (root, cluster);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Fat32Format::BuildMbr
//
//  One active-flag-clear partition of type 0x0C, FAT32 addressed by LBA. The
//  CHS fields have the values for a partition past the CHS limit.
//
////////////////////////////////////////////////////////////////////////////////

void Fat32Format::BuildMbr (uint64_t sectors, std::vector<Byte> & outSector)
{
    size_t  entry = 446;



    outSector.assign (IRawDevice::kBlockBytes, 0);

    PutLong (outSector, 440, (uint32_t) GetTickCount64() ^ 0x43415353);

    outSector[entry + 1] = 0xFE;
    outSector[entry + 2] = 0xFF;
    outSector[entry + 3] = 0xFF;
    outSector[entry + 4] = 0x0C;
    outSector[entry + 5] = 0xFE;
    outSector[entry + 6] = 0xFF;
    outSector[entry + 7] = 0xFF;

    PutLong (outSector, entry + 8,  kPartitionStart);
    PutLong (outSector, entry + 12, (uint32_t) sectors);

    outSector[510] = 0x55;
    outSector[511] = 0xAA;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Fat32Format::BuildBoot
//
////////////////////////////////////////////////////////////////////////////////

void Fat32Format::BuildBoot (uint64_t sectors, uint32_t sectorsPerCluster, uint32_t fatSectors, std::vector<Byte> & outSector)
{
    outSector.assign (IRawDevice::kBlockBytes, 0);

    outSector[0] = 0xEB;
    outSector[1] = 0x58;
    outSector[2] = 0x90;
    std::copy_n ("MSWIN4.1", 8, outSector.begin() + 3);

    PutWord (outSector, 11, (uint16_t) IRawDevice::kBlockBytes);
    outSector[13] = (Byte) sectorsPerCluster;
    PutWord (outSector, 14, (uint16_t) kReservedSectors);
    outSector[16] = (Byte) kFatCount;
    outSector[21] = 0xF8;
    PutWord (outSector, 24, 63);
    PutWord (outSector, 26, 255);
    PutLong (outSector, 28, kPartitionStart);
    PutLong (outSector, 32, (uint32_t) sectors);
    PutLong (outSector, 36, fatSectors);
    PutLong (outSector, 44, 2);
    PutWord (outSector, 48, 1);
    PutWord (outSector, 50, 6);
    outSector[64] = 0x80;
    outSector[66] = 0x29;
    PutLong (outSector, 67, (uint32_t) GetTickCount64());
    std::copy_n ("CFFA3000   ", 11, outSector.begin() + 71);
    std::copy_n ("FAT32   ", 8, outSector.begin() + 82);

    outSector[510] = 0x55;
    outSector[511] = 0xAA;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Fat32Format::BuildFsInfo
//
//  The free count and next free cluster are unknown, which every FAT32
//  reader accepts and recomputes.
//
////////////////////////////////////////////////////////////////////////////////

void Fat32Format::BuildFsInfo (std::vector<Byte> & outSector)
{
    outSector.assign (IRawDevice::kBlockBytes, 0);

    PutLong (outSector, 0,   0x41615252);
    PutLong (outSector, 484, 0x61417272);
    PutLong (outSector, 488, 0xFFFFFFFF);
    PutLong (outSector, 492, 0xFFFFFFFF);
    PutLong (outSector, 508, 0xAA550000);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Fat32Format::PutWord
//
////////////////////////////////////////////////////////////////////////////////

void Fat32Format::PutWord (std::vector<Byte> & bytes, size_t at, uint16_t value)
{
    bytes[at]     = (Byte) (value & 0xFF);
    bytes[at + 1] = (Byte) (value >> 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Fat32Format::PutLong
//
////////////////////////////////////////////////////////////////////////////////

void Fat32Format::PutLong (std::vector<Byte> & bytes, size_t at, uint32_t value)
{
    bytes[at]     = (Byte) (value & 0xFF);
    bytes[at + 1] = (Byte) ((value >> 8) & 0xFF);
    bytes[at + 2] = (Byte) ((value >> 16) & 0xFF);
    bytes[at + 3] = (Byte) (value >> 24);
}

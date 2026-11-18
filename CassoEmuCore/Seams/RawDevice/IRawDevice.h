#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalDrive
//
//  A drive of the PC, as Windows reports it.
//
////////////////////////////////////////////////////////////////////////////////

struct PhysicalDrive
{
    //  The N of `\\.\PhysicalDriveN`.
    int          number    = -1;
    std::string  model;
    uint64_t     bytes     = 0;

    //  "USB", "SD", "MMC", or empty for any other bus.
    std::string  bus;

    //  True for a storage card: removable media on a USB, SD or MMC bus, with
    //  no volume of Windows, its boot files, a page file or a hibernation
    //  file on it. A hard disk, an SSD or an NVMe drive is never one.
    bool         removable = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WriteProgress
//
//  How far a long write has come, in bytes of the bytes it writes.
//
////////////////////////////////////////////////////////////////////////////////

using WriteProgress = std::function<void (uint64_t doneBytes, uint64_t totalBytes)>;





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsVolume
//
//  A volume Windows has on a drive, for the confirmation of an erase.
//
////////////////////////////////////////////////////////////////////////////////

struct WindowsVolume
{
    //  "E:", or empty for a volume with no drive letter.
    std::wstring  letter;
    std::wstring  label;

    //  "FAT32", "exFAT", "NTFS", or empty for a volume with no file system.
    std::wstring  fileSystem;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockExtent
//
//  A run of blocks of a drive.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockExtent
{
    uint64_t  first = 0;
    uint64_t  count = 0;

    bool  Contains (uint64_t start, uint64_t length) const { return start >= first && length <= count && start - first <= count - length; }
    bool  Overlaps (uint64_t start, uint64_t length) const { return length > 0 && count > 0 && start < first + count && first < start + length; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  IRawDevice
//
//  The blocks of one drive of the PC, read and written in place.
//
//  A SEAM BECAUSE A TEST MAY NOT OPEN A DRIVE. The server and the client of
//  the elevated process are tested on a fake with its blocks in memory.
//
////////////////////////////////////////////////////////////////////////////////

class IRawDevice
{
public:
    static constexpr size_t  kBlockBytes = 512;

    virtual ~IRawDevice() = default;

    //  Every drive, removable or not. Listing does not open a drive for reading.
    virtual HRESULT  ListDrives  (std::vector<PhysicalDrive> & outDrives) = 0;

    virtual HRESULT  Open        (int number, PhysicalDrive & outDrive) = 0;
    virtual HRESULT  ReadBlocks  (uint64_t first, uint32_t count, std::span<Byte> outBytes) = 0;
    virtual HRESULT  WriteBlocks (uint64_t first, std::span<const Byte> bytes) = 0;
    virtual void     Close       () = 0;

    //  The blocks of each volume on the open drive that Windows has mounted
    //  with a file system, read at each call, as a volume can mount at any
    //  time.
    virtual HRESULT  GetMountedExtents (std::vector<BlockExtent> & outExtents) = 0;

    //  Every volume on the open drive locked and dismounted, for an erase.
    //  `UnlockVolumes` has Windows read the drive's partitions again and
    //  releases them. A volume with an open file fails to lock.
    virtual HRESULT  LockVolumes   () = 0;
    virtual void     UnlockVolumes () = 0;
};

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

    //  True for removable media and for a drive on a USB, SD or MMC bus,
    //  unless the drive has the Windows volume on it.
    bool         removable = false;
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
};

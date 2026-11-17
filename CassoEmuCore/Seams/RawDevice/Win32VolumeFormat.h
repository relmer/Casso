#pragma once

#include "Pch.h"

#include "Seams/RawDevice/IRawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32VolumeFormat
//
//  FAT32 written in place over the first partition of a drive's MBR, through
//  the partition's volume. Windows opens a removable drive's volume for
//  reading and writing to the signed-in user, so this runs without
//  administrator access, where the drive itself requires it.
//
//  The volume is locked, or dismounted when a program has a file open on
//  it, and its partition type set to FAT32's, 0x0C, when Windows allows.
//  Windows mounts the new volume once the handle closes.
//
////////////////////////////////////////////////////////////////////////////////

class Win32VolumeFormat
{
public:
    //  `outDone` is false, and nothing written, for a drive with no MBR, or
    //  whose first partition has no volume, as a CFFA card; the caller then
    //  writes the whole drive another way.
    static HRESULT  FormatFirstPartition (int number, const WriteProgress & progress, bool & outDone);

private:
    static void  Log (const std::wstring & step, DWORD error);

    static constexpr int     kLockAttempts  = 10;
    static constexpr DWORD   kLockRetryMs   = 100;
    static constexpr size_t  kSectorBytes   = 512;
    static constexpr size_t  kTransferBytes = 128 * kSectorBytes;
    static constexpr BYTE    kFat32Lba      = 0x0C;

    static HRESULT  FindFirstPartition (int number, std::wstring & outVolume, bool & outFound);
    static HRESULT  WriteAt            (HANDLE volume, uint64_t first, std::span<const Byte> bytes);
};

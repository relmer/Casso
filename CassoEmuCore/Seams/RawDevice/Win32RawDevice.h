#pragma once

#include "Pch.h"

#include "Seams/RawDevice/IRawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice
//
//  A drive of the PC through `\\.\PhysicalDriveN`.
//
//  Listing and describing a drive open it with no access, which every user
//  may do. Reading and writing its blocks takes administrator access, so
//  `Open` succeeds only in the elevated process.
//
//  A removable drive is one with removable media or on a USB, SD or MMC bus.
//  The drive with Windows on it is never one, whatever its bus. A drive whose
//  sectors are not 512 bytes fails to open with ERROR_NOT_SUPPORTED.
//
//  Before the first write, each Windows volume on the drive is locked and
//  dismounted, and stays locked until `Close`; Windows fails a write to
//  sectors inside a mounted volume. A volume with an open file fails to
//  lock, and the write fails with that error.
//
////////////////////////////////////////////////////////////////////////////////

class Win32RawDevice : public IRawDevice
{
public:
    Win32RawDevice () = default;
    ~Win32RawDevice () override { Close(); }

    Win32RawDevice (const Win32RawDevice &) = delete;
    Win32RawDevice & operator= (const Win32RawDevice &) = delete;

    //  Drives with no media, such as an empty card reader, are left out.
    HRESULT  ListDrives  (std::vector<PhysicalDrive> & outDrives) override;

    //  Fails with E_ACCESSDENIED for a drive that is not removable.
    HRESULT  Open        (int number, PhysicalDrive & outDrive) override;
    HRESULT  ReadBlocks  (uint64_t first, uint32_t count, std::span<Byte> outBytes) override;
    HRESULT  WriteBlocks (uint64_t first, std::span<const Byte> bytes) override;
    void     Close       () override;

private:
    static constexpr int     kMaxDrives      = 64;
    static constexpr size_t  kTransferBlocks = 128;
    static constexpr size_t  kTransferBytes  = kTransferBlocks * kBlockBytes;

    static std::wstring  MakeDrivePath      (int number);
    static HRESULT       QueryDrive         (HANDLE drive, int number, const std::set<DWORD> & systemDrives, PhysicalDrive & outDrive, DWORD & outSectorBytes);
    static std::string   ReadDescriptorText (const std::vector<Byte> & descriptor, DWORD offset);
    static void          FindSystemDrives   (std::set<DWORD> & outDrives);
    static bool          TryGetVolumeDrives (HANDLE volume, std::set<DWORD> & outDrives);

    HRESULT  LockVolumes ();
    HRESULT  Seek        (uint64_t block);

    HANDLE               m_drive    = INVALID_HANDLE_VALUE;
    int                  m_number   = -1;
    uint64_t             m_blocks   = 0;
    bool                 m_locked   = false;
    std::vector<HANDLE>  m_volumes;

    //  Page-aligned, as a transfer to a drive is from sector-aligned memory.
    Byte               * m_transfer = nullptr;
};

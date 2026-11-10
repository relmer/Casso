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
//  A storage card is removable media on a USB, SD or MMC bus. A drive with
//  a volume of Windows, its boot files, a page file, a swap file or a
//  hibernation file is never one, nor is a drive with a BitLocker volume,
//  found when it opens. A drive whose sectors are not 512 bytes fails to
//  open with ERROR_NOT_SUPPORTED.
//
//  No volume is locked or dismounted: RawDeviceServer fails every write
//  inside a mounted volume.
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

    HRESULT  GetMountedExtents (std::vector<BlockExtent> & outExtents) override;

private:
    static constexpr int     kMaxDrives      = 64;
    static constexpr size_t  kTransferBlocks = 128;
    static constexpr size_t  kTransferBytes  = kTransferBlocks * kBlockBytes;

    //  A volume with any of these at its root is protected.
    static constexpr const wchar_t *  s_kProtectedFiles[] = { L"Windows\\System32", L"bootmgr", L"EFI\\Microsoft\\Boot", L"Boot\\BCD", L"pagefile.sys", L"swapfile.sys", L"hiberfil.sys" };

    static constexpr char  s_kBitLockerSignature[] = "-FVE-FS-";

    static std::wstring  MakeDrivePath       (int number);
    static HRESULT       QueryDrive          (HANDLE drive, int number, const std::set<DWORD> & systemDrives, PhysicalDrive & outDrive, DWORD & outSectorBytes);
    static std::string   ReadDescriptorText  (const std::vector<Byte> & descriptor, DWORD offset);
    static void          FindProtectedDrives (std::set<DWORD> & outDrives);
    static bool          IsProtectedVolume   (const std::wstring & root);
    static bool          TryGetVolumeDrives  (HANDLE volume, std::set<DWORD> & outDrives);
    static std::wstring  TrimVolumeName      (const std::wstring & name);

    HRESULT  CheckBitLocker ();
    HRESULT  Seek           (uint64_t block);

    HANDLE               m_drive    = INVALID_HANDLE_VALUE;
    int                  m_number   = -1;
    uint64_t             m_blocks   = 0;

    //  Page-aligned, as a transfer to a drive is from sector-aligned memory.
    Byte               * m_transfer = nullptr;
};

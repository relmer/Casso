#include "Pch.h"

#include "Seams/RawDevice/Win32RawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::ListDrives
//
//  Drive numbers are not always consecutive, so every number is probed. The
//  volumes are searched for protected ones only when a drive could be a
//  storage card, as the list is read every few seconds.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::ListDrives (std::vector<PhysicalDrive> & outDrives)
{
    std::set<DWORD>  noDrives;
    std::set<DWORD>  systemDrives;
    bool             anyCard      = false;



    outDrives.clear();

    for (int number = 0; number < kMaxDrives; number++)
    {
        PhysicalDrive  drive;
        DWORD          sectorBytes = 0;
        HRESULT        hrQuery     = S_OK;
        HANDLE         handle      = CreateFileW (MakeDrivePath (number).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

        if (handle == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        hrQuery = QueryDrive (handle, number, noDrives, drive, sectorBytes);

        if (SUCCEEDED (hrQuery) && drive.bytes > 0 && sectorBytes == kBlockBytes)
        {
            anyCard = anyCard || drive.removable;
            outDrives.push_back (drive);
        }

        CloseHandle (handle);
    }

    if (anyCard)
    {
        FindProtectedDrives (systemDrives);
    }

    for (PhysicalDrive & drive : outDrives)
    {
        drive.removable = drive.removable && !systemDrives.contains ((DWORD) drive.number);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::HasUnmountedPartition
//
//  Read without administrator access. True for a drive with no partition
//  table, and for one with a partition no volume with a file system covers.
//  An extended MBR entry is passed over: each of its logical drives is a
//  separate entry.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::HasUnmountedPartition (int number, bool & outHasUnmounted)
{
    HRESULT                          hr       = S_OK;
    HANDLE                           drive    = INVALID_HANDLE_VALUE;
    std::vector<Byte>                buffer (sizeof (DRIVE_LAYOUT_INFORMATION_EX) + 127 * sizeof (PARTITION_INFORMATION_EX), 0);
    DRIVE_LAYOUT_INFORMATION_EX    * layout   = (DRIVE_LAYOUT_INFORMATION_EX *) buffer.data();
    DWORD                            returned = 0;
    BOOL                             ok       = FALSE;
    bool                             anyEntry = false;
    std::vector<BlockExtent>         mounted;



    outHasUnmounted = false;

    drive = CreateFileW (MakeDrivePath (number).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    CWR (drive != INVALID_HANDLE_VALUE);

    ok = DeviceIoControl (drive, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0, buffer.data(), (DWORD) buffer.size(), &returned, nullptr);
    CWR (ok);

    hr = ReadMountedExtents (number, mounted);
    CHR (hr);

    for (DWORD at = 0; at < layout->PartitionCount && at < 128; at++)
    {
        const PARTITION_INFORMATION_EX  & partition = layout->PartitionEntry[at];
        uint64_t                          start     = (uint64_t) partition.StartingOffset.QuadPart / kBlockBytes;
        uint64_t                          length    = (uint64_t) partition.PartitionLength.QuadPart / kBlockBytes;
        bool                              extended  = partition.PartitionStyle == PARTITION_STYLE_MBR && IsContainerPartition (partition.Mbr.PartitionType);
        bool                              covered   = false;

        if (length == 0 || extended)
        {
            continue;
        }

        anyEntry = true;

        for (const BlockExtent & extent : mounted)
        {
            covered = covered || extent.Overlaps (start, length);
        }

        outHasUnmounted = outHasUnmounted || !covered;
    }

    outHasUnmounted = outHasUnmounted || !anyEntry;

Error:
    if (drive != INVALID_HANDLE_VALUE)
    {
        CloseHandle (drive);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::Open
//
//  The drive is queried through a handle with no access first, so a drive
//  that is not removable is never opened for reading or writing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::Open (int number, PhysicalDrive & outDrive)
{
    HRESULT          hr          = S_OK;
    HANDLE           probe       = INVALID_HANDLE_VALUE;
    DWORD            sectorBytes = 0;
    bool             inRange     = number >= 0 && number < kMaxDrives;
    std::set<DWORD>  systemDrives;



    Close();

    outDrive = PhysicalDrive();

    CBREx (inRange, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    FindProtectedDrives (systemDrives);

    probe = CreateFileW (MakeDrivePath (number).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    CWR (probe != INVALID_HANDLE_VALUE);

    hr = QueryDrive (probe, number, systemDrives, outDrive, sectorBytes);
    CHR (hr);

    CBREx (outDrive.removable, E_ACCESSDENIED);
    CBREx (outDrive.bytes > 0, HRESULT_FROM_WIN32 (ERROR_NOT_READY));
    CBREx (sectorBytes == kBlockBytes, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    m_transfer = (Byte *) VirtualAlloc (nullptr, kTransferBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CWR (m_transfer != nullptr);

    m_drive = CreateFileW (MakeDrivePath (number).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    CWR (m_drive != INVALID_HANDLE_VALUE);

    m_number = number;
    m_blocks = outDrive.bytes / kBlockBytes;

    hr = CheckBitLocker();
    CHR (hr);

Error:
    if (probe != INVALID_HANDLE_VALUE)
    {
        CloseHandle (probe);
    }

    if (FAILED (hr))
    {
        Close();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::ReadBlocks (uint64_t first, uint32_t count, std::span<Byte> outBytes)
{
    HRESULT  hr     = S_OK;
    bool     open   = m_drive != INVALID_HANDLE_VALUE;
    bool     inside = first <= m_blocks && count <= m_blocks - first;
    bool     sized  = outBytes.size() >= (size_t) count * kBlockBytes;
    size_t   done   = 0;



    CBRAEx (open,  E_UNEXPECTED);
    CBRAEx (sized, E_INVALIDARG);
    CBREx (inside, HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND));

    hr = Seek (first);
    CHR (hr);

    while (done < count)
    {
        size_t  run   = std::min (count - done, kTransferBlocks);
        DWORD   bytes = (DWORD) (run * kBlockBytes);
        DWORD   got   = 0;
        BOOL    ok    = ReadFile (m_drive, m_transfer, bytes, &got, nullptr);

        CWR (ok);
        CBREx (got == bytes, HRESULT_FROM_WIN32 (ERROR_READ_FAULT));

        std::copy (m_transfer, m_transfer + bytes, outBytes.begin() + (ptrdiff_t) (done * kBlockBytes));

        done += run;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::WriteBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::WriteBlocks (uint64_t first, std::span<const Byte> bytes)
{
    HRESULT   hr     = S_OK;
    uint64_t  count  = bytes.size() / kBlockBytes;
    bool      open   = m_drive != INVALID_HANDLE_VALUE;
    bool      whole  = bytes.size() % kBlockBytes == 0;
    bool      inside = first <= m_blocks && count <= m_blocks - first;
    size_t    done   = 0;



    CBRAEx (open,  E_UNEXPECTED);
    CBRAEx (whole, E_INVALIDARG);
    CBREx (inside, HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND));

    hr = Seek (first);
    CHR (hr);

    while (done < count)
    {
        size_t  run     = (size_t) std::min<uint64_t> (count - done, kTransferBlocks);
        DWORD   length  = (DWORD) (run * kBlockBytes);
        DWORD   written = 0;
        BOOL    ok      = FALSE;

        std::copy (bytes.begin() + (ptrdiff_t) (done * kBlockBytes), bytes.begin() + (ptrdiff_t) ((done + run) * kBlockBytes), m_transfer);

        ok = WriteFile (m_drive, m_transfer, length, &written, nullptr);
        CWR (ok);
        CBREx (written == length, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));

        done += run;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::Close
//

////////////////////////////////////////////////////////////////////////////////

void Win32RawDevice::Close()
{
    if (m_drive != INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers (m_drive);
        CloseHandle (m_drive);
    }

    UnlockVolumes();

    if (m_transfer != nullptr)
    {
        VirtualFree (m_transfer, 0, MEM_RELEASE);
    }

    m_drive    = INVALID_HANDLE_VALUE;
    m_transfer = nullptr;
    m_number   = -1;
    m_blocks   = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::MakeDrivePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32RawDevice::MakeDrivePath (int number)
{
    return std::format (L"\\\\.\\PhysicalDrive{}", number);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::QueryDrive
//
//  The model is the vendor and the product Windows reports, trimmed. The
//  size is 0 for a drive with no media in it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::QueryDrive (HANDLE drive, int number, const std::set<DWORD> & systemDrives, PhysicalDrive & outDrive, DWORD & outSectorBytes)
{
    HRESULT                       hr           = S_OK;
    STORAGE_PROPERTY_QUERY        query        = {};
    std::vector<Byte>             buffer (1024, 0);
    STORAGE_DEVICE_DESCRIPTOR   * descriptor   = (STORAGE_DEVICE_DESCRIPTOR *) buffer.data();
    DISK_GEOMETRY_EX              geometry     = {};
    DWORD                         returned     = 0;
    BOOL                          ok           = FALSE;
    bool                          complete     = false;
    bool                          onCardBus    = false;
    std::string                   vendor;
    std::string                   product;



    outDrive        = PhysicalDrive();
    outDrive.number = number;
    outSectorBytes  = 0;

    query.PropertyId = StorageDeviceProperty;
    query.QueryType  = PropertyStandardQuery;

    ok = DeviceIoControl (drive, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof (query), buffer.data(), (DWORD) buffer.size(), &returned, nullptr);
    CWR (ok);

    complete = returned >= sizeof (STORAGE_DEVICE_DESCRIPTOR);
    CBREx (complete, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    buffer.resize (returned);
    descriptor = (STORAGE_DEVICE_DESCRIPTOR *) buffer.data();

    switch (descriptor->BusType)
    {
        case BusTypeUsb:
            outDrive.bus = "USB";
            break;

        case BusTypeSd:
            outDrive.bus = "SD";
            break;

        case BusTypeMmc:
            outDrive.bus = "MMC";
            break;

        default:
            break;
    }

    vendor  = ReadDescriptorText (buffer, descriptor->VendorIdOffset);
    product = ReadDescriptorText (buffer, descriptor->ProductIdOffset);

    outDrive.model = vendor.empty() || product.empty() ? vendor + product : vendor + " " + product;
    outDrive.model = std::regex_replace (outDrive.model, std::regex ("  +"), " ");

    onCardBus          = !outDrive.bus.empty();
    outDrive.removable = descriptor->RemovableMedia != FALSE && onCardBus && !systemDrives.contains ((DWORD) number);

    //  Fails for a drive with no media, which is reported as a size of 0.
    if (DeviceIoControl (drive, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &geometry, sizeof (geometry), &returned, nullptr))
    {
        outDrive.bytes = (uint64_t) geometry.DiskSize.QuadPart;
        outSectorBytes = geometry.Geometry.BytesPerSector;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::ReadDescriptorText
//
//  The text at an offset of a device descriptor, up to its terminator,
//  without the spaces at either end. An offset of 0 is a drive with no such
//  text.
//
////////////////////////////////////////////////////////////////////////////////

std::string Win32RawDevice::ReadDescriptorText (const std::vector<Byte> & descriptor, DWORD offset)
{
    std::string  text;
    size_t       first = 0;
    size_t       last  = 0;



    for (size_t at = offset; offset != 0 && at < descriptor.size() && descriptor[at] != 0; at++)
    {
        Byte  ch = descriptor[at];

        //  Runs of spaces are one, as some readers pad the vendor and product.
        if (ch >= 0x20 && ch < 0x7F && !(ch == ' ' && !text.empty() && text.back() == ' '))
        {
            text.push_back ((char) ch);
        }
    }

    first = text.find_first_not_of (' ');
    last  = text.find_last_not_of (' ');

    return first == std::string::npos ? std::string() : text.substr (first, last - first + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::FindProtectedDrives
//
//  The drives of every volume that is protected. A volume that cannot be
//  opened or has no extents, as a CD drive with no disc, is passed over.
//
////////////////////////////////////////////////////////////////////////////////

void Win32RawDevice::FindProtectedDrives (std::set<DWORD> & outDrives)
{
    wchar_t  name[MAX_PATH] = {};
    HANDLE   search         = FindFirstVolumeW (name, ARRAYSIZE (name));
    BOOL     more           = search != INVALID_HANDLE_VALUE;



    outDrives.clear();

    while (more)
    {
        std::wstring  root   = name;
        HANDLE        volume = INVALID_HANDLE_VALUE;

        if (IsProtectedVolume (root))
        {
            volume = CreateFileW (TrimVolumeName (root).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        }

        if (volume != INVALID_HANDLE_VALUE)
        {
            TryGetVolumeDrives (volume, outDrives);
            CloseHandle (volume);
        }

        more = FindNextVolumeW (search, name, ARRAYSIZE (name));
    }

    if (search != INVALID_HANDLE_VALUE)
    {
        FindVolumeClose (search);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::IsProtectedVolume
//
//  A volume with Windows, Windows' boot files, a page file, a swap file or
//  a hibernation file at its root. A volume whose root cannot be read for a
//  reason other than an empty drive is treated as protected.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32RawDevice::IsProtectedVolume (const std::wstring & root)
{
    DWORD  attributes = GetFileAttributesW (root.c_str());
    DWORD  error      = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_SUCCESS;
    bool   found      = false;



    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return error != ERROR_NOT_READY && error != ERROR_UNRECOGNIZED_VOLUME && error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND;
    }

    for (const wchar_t * marker : s_kProtectedFiles)
    {
        found = found || GetFileAttributesW ((root + marker).c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::TrimVolumeName
//
//  A volume is opened by its name without the final backslash.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32RawDevice::TrimVolumeName (const std::wstring & name)
{
    return !name.empty() && name.back() == L'\\' ? name.substr (0, name.size() - 1) : name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::TryGetVolumeDrives
//
////////////////////////////////////////////////////////////////////////////////

bool Win32RawDevice::TryGetVolumeDrives (HANDLE volume, std::set<DWORD> & outDrives)
{
    std::vector<Byte>       buffer (sizeof (VOLUME_DISK_EXTENTS) + 31 * sizeof (DISK_EXTENT), 0);
    VOLUME_DISK_EXTENTS   * extents  = (VOLUME_DISK_EXTENTS *) buffer.data();
    DWORD                   returned = 0;
    bool                    read     = DeviceIoControl (volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, buffer.data(), (DWORD) buffer.size(), &returned, nullptr) != FALSE;



    for (DWORD at = 0; read && at < extents->NumberOfDiskExtents && at < 32; at++)
    {
        outDrives.insert (extents->Extents[at].DiskNumber);
    }

    return read;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::GetMountedExtents
//
//  A volume Windows reports as RAW, or with no file system, is not mounted.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::GetMountedExtents (std::vector<BlockExtent> & outExtents)
{
    HRESULT  hr   = S_OK;
    bool     open = m_drive != INVALID_HANDLE_VALUE;



    outExtents.clear();

    CBRAEx (open, E_UNEXPECTED);

    hr = ReadMountedExtents (m_number, outExtents);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::ReadMountedExtents
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::ReadMountedExtents (int number, std::vector<BlockExtent> & outExtents)
{
    HRESULT   hr             = S_OK;
    wchar_t   name[MAX_PATH] = {};
    HANDLE    search         = INVALID_HANDLE_VALUE;
    BOOL      more           = FALSE;



    outExtents.clear();

    search = FindFirstVolumeW (name, ARRAYSIZE (name));
    CWR (search != INVALID_HANDLE_VALUE);

    more = TRUE;

    while (more)
    {
        std::wstring          root                       = name;
        wchar_t               fileSystem[MAX_PATH + 1]   = {};
        std::vector<Byte>     buffer (sizeof (VOLUME_DISK_EXTENTS) + 31 * sizeof (DISK_EXTENT), 0);
        VOLUME_DISK_EXTENTS * extents                    = (VOLUME_DISK_EXTENTS *) buffer.data();
        DWORD                 returned                   = 0;
        HANDLE                volume                     = INVALID_HANDLE_VALUE;
        bool                  mounted                    = GetVolumeInformationW (root.c_str(), nullptr, 0, nullptr, nullptr, nullptr, fileSystem, ARRAYSIZE (fileSystem)) != FALSE &&
                                                           _wcsicmp (fileSystem, L"RAW") != 0;
        bool                  read                       = false;

        if (mounted)
        {
            volume = CreateFileW (TrimVolumeName (root).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        }

        if (volume != INVALID_HANDLE_VALUE)
        {
            read = DeviceIoControl (volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, buffer.data(), (DWORD) buffer.size(), &returned, nullptr) != FALSE;
            CloseHandle (volume);
        }

        for (DWORD at = 0; read && at < extents->NumberOfDiskExtents && at < 32; at++)
        {
            const DISK_EXTENT  & extent = extents->Extents[at];
            uint64_t             start  = (uint64_t) extent.StartingOffset.QuadPart;
            uint64_t             length = (uint64_t) extent.ExtentLength.QuadPart;

            if (extent.DiskNumber == (DWORD) number && length > 0)
            {
                outExtents.push_back (BlockExtent { start / kBlockBytes, (start + length + kBlockBytes - 1) / kBlockBytes - start / kBlockBytes });
            }
        }

        more = FindNextVolumeW (search, name, ARRAYSIZE (name));
    }

Error:
    if (search != INVALID_HANDLE_VALUE)
    {
        FindVolumeClose (search);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::CheckBitLocker
//
//  A partition that begins with BitLocker's signature fails the open with
//  E_ACCESSDENIED, locked or not.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::CheckBitLocker()
{
    HRESULT                          hr       = S_OK;
    std::vector<Byte>                buffer (sizeof (DRIVE_LAYOUT_INFORMATION_EX) + 127 * sizeof (PARTITION_INFORMATION_EX), 0);
    DRIVE_LAYOUT_INFORMATION_EX    * layout   = (DRIVE_LAYOUT_INFORMATION_EX *) buffer.data();
    DWORD                            returned = 0;
    BOOL                             ok       = FALSE;
    std::vector<Byte>                sector (kBlockBytes, 0);



    ok = DeviceIoControl (m_drive, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0, buffer.data(), (DWORD) buffer.size(), &returned, nullptr);
    CWR (ok);

    for (DWORD at = 0; at < layout->PartitionCount && at < 128; at++)
    {
        const PARTITION_INFORMATION_EX  & partition = layout->PartitionEntry[at];
        uint64_t                          start     = (uint64_t) partition.StartingOffset.QuadPart;
        bool                              isLocked  = false;

        if (partition.PartitionLength.QuadPart <= 0 || start % kBlockBytes != 0)
        {
            continue;
        }

        hr = ReadBlocks (start / kBlockBytes, 1, sector);
        CHR (hr);

        isLocked = memcmp (sector.data() + 3, s_kBitLockerSignature, sizeof (s_kBitLockerSignature) - 1) == 0;
        CBREx (!isLocked, E_ACCESSDENIED);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::Seek
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::Seek (uint64_t block)
{
    HRESULT        hr       = S_OK;
    LARGE_INTEGER  position = {};
    BOOL           ok       = FALSE;



    position.QuadPart = (LONGLONG) (block * kBlockBytes);

    ok = SetFilePointerEx (m_drive, position, nullptr, FILE_BEGIN);
    CWR (ok);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::LockVolumes
//
//  Every volume with an extent on this drive, mounted or not. A volume that
//  cannot be opened or has no extents, as a CD drive with no disc, is passed
//  over.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::LockVolumes()
{
    HRESULT   hr             = S_OK;
    wchar_t   name[MAX_PATH] = {};
    HANDLE    search         = INVALID_HANDLE_VALUE;
    BOOL      more           = FALSE;
    bool      open           = m_drive != INVALID_HANDLE_VALUE;



    CBRAEx (open, E_UNEXPECTED);

    search = FindFirstVolumeW (name, ARRAYSIZE (name));
    more   = search != INVALID_HANDLE_VALUE;

    while (more)
    {
        std::set<DWORD>  drives;
        HANDLE           volume   = CreateFileW (TrimVolumeName (name).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        DWORD            returned = 0;
        BOOL             ok       = FALSE;

        if (volume != INVALID_HANDLE_VALUE && TryGetVolumeDrives (volume, drives) && drives.contains ((DWORD) m_number))
        {
            m_volumes.push_back (volume);

            //  A volume with a file open, as by File Explorer or the search
            //  indexer, fails to lock. Dismounting it anyway closes those
            //  files, after the confirmation of the erase, and the lock then
            //  succeeds.
            ok = DeviceIoControl (volume, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);

            if (!ok)
            {
                ok = DeviceIoControl (volume, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);
                CWR (ok);

                ok = DeviceIoControl (volume, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);
                CWR (ok);
            }

            ok = DeviceIoControl (volume, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);
            CWR (ok);
        }
        else if (volume != INVALID_HANDLE_VALUE)
        {
            CloseHandle (volume);
        }

        more = FindNextVolumeW (search, name, ARRAYSIZE (name));
    }

Error:
    if (search != INVALID_HANDLE_VALUE)
    {
        FindVolumeClose (search);
    }

    if (FAILED (hr))
    {
        UnlockVolumes();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::UnlockVolumes
//
//  Closing a volume's handle unlocks it, and Windows mounts what it finds
//  there on its next use. Called at each close, so the partitions are read
//  again after any write.
//
////////////////////////////////////////////////////////////////////////////////

void Win32RawDevice::UnlockVolumes()
{
    DWORD  returned = 0;



    if (m_drive != INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers (m_drive);
        DeviceIoControl (m_drive, IOCTL_DISK_UPDATE_PROPERTIES, nullptr, 0, nullptr, 0, &returned, nullptr);
    }

    for (HANDLE volume : m_volumes)
    {
        CloseHandle (volume);
    }

    m_volumes.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::ListVolumes
//
//  Read without administrator access. A volume with no file system has an
//  empty label and file system.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::ListVolumes (int number, std::vector<WindowsVolume> & outVolumes)
{
    HRESULT   hr             = S_OK;
    wchar_t   name[MAX_PATH] = {};
    HANDLE    search         = INVALID_HANDLE_VALUE;
    BOOL      more           = FALSE;



    outVolumes.clear();

    search = FindFirstVolumeW (name, ARRAYSIZE (name));
    CWR (search != INVALID_HANDLE_VALUE);

    more = TRUE;

    while (more)
    {
        std::set<DWORD>  drives;
        WindowsVolume    volume;
        wchar_t          label[MAX_PATH + 1]      = {};
        wchar_t          fileSystem[MAX_PATH + 1] = {};
        wchar_t          paths[MAX_PATH * 4]      = {};
        DWORD            length                   = 0;
        HANDLE           handle                   = CreateFileW (TrimVolumeName (name).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        bool             onDrive                  = false;

        if (handle != INVALID_HANDLE_VALUE)
        {
            onDrive = TryGetVolumeDrives (handle, drives) && drives.contains ((DWORD) number);
            CloseHandle (handle);
        }

        if (onDrive)
        {
            if (GetVolumeInformationW (name, label, ARRAYSIZE (label), nullptr, nullptr, nullptr, fileSystem, ARRAYSIZE (fileSystem)))
            {
                volume.label      = label;
                volume.fileSystem = _wcsicmp (fileSystem, L"RAW") == 0 ? std::wstring() : std::wstring (fileSystem);
            }

            if (GetVolumePathNamesForVolumeNameW (name, paths, ARRAYSIZE (paths), &length) && paths[0] != L'\0' && paths[1] == L':')
            {
                volume.letter = std::wstring (paths, 2);
            }

            outVolumes.push_back (std::move (volume));
        }

        more = FindNextVolumeW (search, name, ARRAYSIZE (name));
    }

Error:
    if (search != INVALID_HANDLE_VALUE)
    {
        FindVolumeClose (search);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::FindVolumeDrive
//
//  The volume is opened by its letter with no access, which every user may
//  do.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::FindVolumeDrive (const std::wstring & root, int & outNumber)
{
    HRESULT          hr     = S_OK;
    HANDLE           volume = INVALID_HANDLE_VALUE;
    bool             valid  = root.size() >= 2 && root[1] == L':';
    bool             single = false;
    std::set<DWORD>  drives;



    outNumber = -1;

    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    volume = CreateFileW (std::format (L"\\\\.\\{}:", root[0]).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    CWR (volume != INVALID_HANDLE_VALUE);

    single = TryGetVolumeDrives (volume, drives) && drives.size() == 1;
    CBREx (single, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    outNumber = (int) *drives.begin();

Error:
    if (volume != INVALID_HANDLE_VALUE)
    {
        CloseHandle (volume);
    }

    return hr;
}
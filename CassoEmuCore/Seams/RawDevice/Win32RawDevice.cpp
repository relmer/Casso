#include "Pch.h"

#include "Seams/RawDevice/Win32RawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::ListDrives
//
//  Drive numbers are not always consecutive, so every number is probed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::ListDrives (std::vector<PhysicalDrive> & outDrives)
{
    std::set<DWORD>  systemDrives;



    outDrives.clear();

    FindSystemDrives (systemDrives);

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

        hrQuery = QueryDrive (handle, number, systemDrives, drive, sectorBytes);

        if (SUCCEEDED (hrQuery) && drive.bytes > 0 && sectorBytes == kBlockBytes)
        {
            outDrives.push_back (drive);
        }

        CloseHandle (handle);
    }

    return S_OK;
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

    FindSystemDrives (systemDrives);

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

    hr = LockVolumes();
    CHR (hr);

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
//  Closing a volume's handle unlocks it, and Windows mounts it again on its
//  next use.
//
////////////////////////////////////////////////////////////////////////////////

void Win32RawDevice::Close()
{
    if (m_drive != INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers (m_drive);
        CloseHandle (m_drive);
    }

    for (HANDLE volume : m_volumes)
    {
        CloseHandle (volume);
    }

    if (m_transfer != nullptr)
    {
        VirtualFree (m_transfer, 0, MEM_RELEASE);
    }

    m_volumes.clear();

    m_drive    = INVALID_HANDLE_VALUE;
    m_transfer = nullptr;
    m_number   = -1;
    m_blocks   = 0;
    m_locked   = false;
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

    onCardBus          = !outDrive.bus.empty();
    outDrive.removable = (descriptor->RemovableMedia != FALSE || onCardBus) && !systemDrives.contains ((DWORD) number);

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

        text.push_back (ch >= 0x20 && ch < 0x7F ? (char) ch : ' ');
    }

    first = text.find_first_not_of (' ');
    last  = text.find_last_not_of (' ');

    return first == std::string::npos ? std::string() : text.substr (first, last - first + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDevice::FindSystemDrives
//
//  The drives the volume of the Windows folder is on. Nothing is added when
//  that cannot be read.
//
////////////////////////////////////////////////////////////////////////////////

void Win32RawDevice::FindSystemDrives (std::set<DWORD> & outDrives)
{
    wchar_t  folder[MAX_PATH] = {};
    UINT     length           = GetWindowsDirectoryW (folder, ARRAYSIZE (folder));
    bool     lettered         = length >= 2 && length < ARRAYSIZE (folder) && folder[1] == L':';
    HANDLE   volume           = INVALID_HANDLE_VALUE;



    outDrives.clear();

    if (lettered)
    {
        volume = CreateFileW (std::format (L"\\\\.\\{}:", folder[0]).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    }

    if (volume != INVALID_HANDLE_VALUE)
    {
        TryGetVolumeDrives (volume, outDrives);
        CloseHandle (volume);
    }
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
//  Win32RawDevice::LockVolumes
//
//  Every volume with an extent on this drive. A volume that cannot be opened
//  or has no extents, as a CD drive with no disc, is passed over.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDevice::LockVolumes()
{
    HRESULT   hr             = S_OK;
    wchar_t   name[MAX_PATH] = {};
    HANDLE    search         = INVALID_HANDLE_VALUE;
    BOOL      more           = FALSE;



    BAIL_OUT_IF (m_locked, S_OK);

    search = FindFirstVolumeW (name, ARRAYSIZE (name));
    more   = search != INVALID_HANDLE_VALUE;

    while (more)
    {
        std::wstring     path     = name;
        std::set<DWORD>  drives;
        HANDLE           volume   = INVALID_HANDLE_VALUE;
        DWORD            returned = 0;
        BOOL             ok       = FALSE;

        //  A volume is opened by its name without the final backslash.
        if (!path.empty() && path.back() == L'\\')
        {
            path.pop_back();
        }

        volume = CreateFileW (path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

        if (volume != INVALID_HANDLE_VALUE && TryGetVolumeDrives (volume, drives) && drives.contains ((DWORD) m_number))
        {
            m_volumes.push_back (volume);

            ok = DeviceIoControl (volume, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);
            CWR (ok);

            ok = DeviceIoControl (volume, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);
            CWR (ok);
        }
        else if (volume != INVALID_HANDLE_VALUE)
        {
            CloseHandle (volume);
        }

        more = FindNextVolumeW (search, name, ARRAYSIZE (name));
    }

    m_locked = true;

Error:
    if (search != INVALID_HANDLE_VALUE)
    {
        FindVolumeClose (search);
    }

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

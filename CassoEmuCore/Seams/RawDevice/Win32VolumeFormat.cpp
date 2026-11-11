#include "Pch.h"

#include "Seams/RawDevice/Win32VolumeFormat.h"
#include "Seams/RawDevice/Fat32Format.h"
#include "Seams/RawDevice/Win32StorageCardBroker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32VolumeFormat::FormatFirstPartition
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32VolumeFormat::FormatFirstPartition (int number, const WriteProgress & progress, bool & outDone)
{
    HRESULT                     hr       = S_OK;
    HANDLE                      volume   = INVALID_HANDLE_VALUE;
    GET_LENGTH_INFORMATION      length   = {};
    PARTITION_INFORMATION_EX    info     = {};
    SET_PARTITION_INFORMATION   type     = {};
    DWORD                       returned = 0;
    BOOL                        ok       = FALSE;
    DWORD                       error    = 0;
    bool                        found    = false;
    std::wstring                name;



    outDone = false;

    hr = FindFirstPartition (number, name, found);
    CHR (hr);
    BAIL_OUT_IF (!found, S_OK);

    volume = CreateFileW (name.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    Log (std::format (L"format {} in place: open", name), volume == INVALID_HANDLE_VALUE ? GetLastError() : 0);
    BAIL_OUT_IF (volume == INVALID_HANDLE_VALUE, S_OK);

    ok = DeviceIoControl (volume, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof (length), &returned, nullptr);
    CWR (ok);

    ok = DeviceIoControl (volume, IOCTL_DISK_GET_PARTITION_INFO_EX, nullptr, 0, &info, sizeof (info), &returned, nullptr);
    CWR (ok);

    BAIL_OUT_IF (!Fat32Format::CanFormatVolume ((uint64_t) length.Length.QuadPart / kSectorBytes), S_OK);

    //  A program with a file open fails every lock. The volume is then
    //  dismounted, which closes every handle to it, this one too, so it is
    //  opened again before the next lock.
    ok = FALSE;

    for (int attempt = 0; !ok && attempt < kLockAttempts; attempt++)
    {
        ok    = DeviceIoControl (volume, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);
        error = ok ? 0 : GetLastError();

        if (!ok)
        {
            DeviceIoControl (volume, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);
            CloseHandle (volume);

            volume = CreateFileW (name.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            error  = volume == INVALID_HANDLE_VALUE ? GetLastError() : 0;
            CBREx (volume != INVALID_HANDLE_VALUE, HRESULT_FROM_WIN32 (error));

            ok    = DeviceIoControl (volume, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);
            error = ok ? 0 : GetLastError();
        }

        if (!ok)
        {
            Sleep (kLockRetryMs);
        }
    }

    Log (L"  lock", error);
    CBREx (ok, HRESULT_FROM_WIN32 (error));

    ok    = DeviceIoControl (volume, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr);
    error = ok ? 0 : GetLastError();
    Log (L"  dismount", error);
    CBREx (ok, HRESULT_FROM_WIN32 (error));

    hr = Fat32Format::WriteVolume ([volume] (uint64_t first, std::span<const Byte> bytes) { return WriteAt (volume, first, bytes); },
                                   (uint64_t) length.Length.QuadPart / kSectorBytes,
                                   (uint64_t) info.StartingOffset.QuadPart / kSectorBytes,
                                   progress);
    Log (L"  write", (DWORD) hr);
    CHR (hr);

    //  The partition's type is a hint Windows and some readers use, so a
    //  failure to set it is not an error.
    type.PartitionType = kFat32Lba;
    DeviceIoControl (volume, IOCTL_DISK_SET_PARTITION_INFO, &type, sizeof (type), nullptr, 0, &returned, nullptr);

    FlushFileBuffers (volume);

    outDone = true;

Error:
    if (volume != INVALID_HANDLE_VALUE)
    {
        CloseHandle (volume);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32VolumeFormat::FindFirstPartition
//
//  The volume whose only extent is on the drive and which is partition 1 of
//  an MBR.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32VolumeFormat::FindFirstPartition (int number, std::wstring & outVolume, bool & outFound)
{
    wchar_t  name[MAX_PATH] = {};
    HANDLE   search         = FindFirstVolumeW (name, ARRAYSIZE (name));
    BOOL     more           = search != INVALID_HANDLE_VALUE;



    outVolume.clear();
    outFound = false;

    while (more && !outFound)
    {
        std::wstring              path     = name;
        VOLUME_DISK_EXTENTS       extents  = {};
        PARTITION_INFORMATION_EX  info     = {};
        DWORD                     returned = 0;
        HANDLE                    volume   = INVALID_HANDLE_VALUE;
        bool                      onDrive  = false;
        bool                      first    = false;

        if (!path.empty() && path.back() == L'\\')
        {
            path.pop_back();
        }

        volume = CreateFileW (path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

        if (volume != INVALID_HANDLE_VALUE)
        {
            onDrive = DeviceIoControl (volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, &extents, sizeof (extents), &returned, nullptr) &&
                      extents.NumberOfDiskExtents == 1 && extents.Extents[0].DiskNumber == (DWORD) number;
            first   = onDrive &&
                      DeviceIoControl (volume, IOCTL_DISK_GET_PARTITION_INFO_EX, nullptr, 0, &info, sizeof (info), &returned, nullptr) &&
                      info.PartitionStyle == PARTITION_STYLE_MBR && info.PartitionNumber == 1;

            CloseHandle (volume);
        }

        if (first)
        {
            outVolume = path;
            outFound  = true;
        }

        more = FindNextVolumeW (search, name, ARRAYSIZE (name));
    }

    if (search != INVALID_HANDLE_VALUE)
    {
        FindVolumeClose (search);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32VolumeFormat::WriteAt
//
//  Through a page-aligned buffer, as a write to a volume is from
//  sector-aligned memory.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32VolumeFormat::WriteAt (HANDLE volume, uint64_t first, std::span<const Byte> bytes)
{
    HRESULT        hr       = S_OK;
    LARGE_INTEGER  position = {};
    Byte         * buffer   = (Byte *) VirtualAlloc (nullptr, kTransferBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    size_t         done     = 0;
    BOOL           ok       = FALSE;



    CWR (buffer != nullptr);

    position.QuadPart = (LONGLONG) (first * kSectorBytes);

    ok = SetFilePointerEx (volume, position, nullptr, FILE_BEGIN);
    CWR (ok);

    while (done < bytes.size())
    {
        size_t  run     = std::min (bytes.size() - done, kTransferBytes);
        DWORD   written = 0;

        std::copy_n (bytes.begin() + (ptrdiff_t) done, run, buffer);

        ok = WriteFile (volume, buffer, (DWORD) run, &written, nullptr);

        if (!ok)
        {
            DWORD  failure = GetLastError();

            Log (std::format (L"  write {} bytes at block {}", run, first + done / kSectorBytes), failure);
            CBREx (false, HRESULT_FROM_WIN32 (failure));
        }

        CBREx (written == run, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));

        done += run;
    }

Error:
    if (buffer != nullptr)
    {
        VirtualFree (buffer, 0, MEM_RELEASE);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32VolumeFormat::Log
//
////////////////////////////////////////////////////////////////////////////////

void Win32VolumeFormat::Log (const std::wstring & step, DWORD error)
{
    Win32StorageCardBroker::AppendLog (std::format (L"{}: {}", step, error));
}
#include "Pch.h"

#include "Debugger/Channel/Win32HostPathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostPathResolver::TryGetFinalPath
//
//  The handle is opened with no access and every share mode, so a file
//  another process has open is resolved too. FILE_FLAG_BACKUP_SEMANTICS
//  opens a folder, and without FILE_FLAG_OPEN_REPARSE_POINT a link or a
//  junction is followed to its target.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32HostPathResolver::TryGetFinalPath (const std::wstring & path, std::wstring & finalPath)
{
    HRESULT       hr       = S_OK;
    HANDLE        handle   = INVALID_HANDLE_VALUE;
    DWORD         length   = 0;
    DWORD         capacity = 0;
    std::wstring  buffer;
    bool          isFound  = false;



    handle = CreateFileW (path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                          OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    CBR (handle != INVALID_HANDLE_VALUE);

    length = GetFinalPathNameByHandleW (handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CWR (length != 0);

    buffer.resize (length);
    capacity = length;
    length   = GetFinalPathNameByHandleW (handle, buffer.data(), capacity, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CWR (length != 0 && length < capacity);

    buffer.resize (length);
    finalPath = RemoveLongPathPrefix (buffer);
    isFound   = true;

Error:
    if (handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle (handle);
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostPathResolver::Exists
//
////////////////////////////////////////////////////////////////////////////////

bool Win32HostPathResolver::Exists (const std::wstring & path)
{
    return GetFileAttributesW (path.c_str()) != INVALID_FILE_ATTRIBUTES;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostPathResolver::TryGetKnownFolder
//
////////////////////////////////////////////////////////////////////////////////

bool Win32HostPathResolver::TryGetKnownFolder (HostKnownFolder folder, std::wstring & path)
{
    HRESULT                hr      = S_OK;
    PWSTR                  raw     = nullptr;
    const KNOWNFOLDERID  * id      = &FOLDERID_Documents;
    bool                   isFound = false;



    switch (folder)
    {
    case HostKnownFolder::Startup:        id = &FOLDERID_Startup;       break;
    case HostKnownFolder::CommonStartup:  id = &FOLDERID_CommonStartup; break;
    default:                              id = &FOLDERID_Documents;     break;
    }

    hr = SHGetKnownFolderPath (*id, 0, nullptr, &raw);
    CHR (hr);

    path    = raw;
    isFound = true;

Error:
    if (raw != nullptr)
    {
        CoTaskMemFree (raw);
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostPathResolver::RemoveLongPathPrefix
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32HostPathResolver::RemoveLongPathPrefix (const std::wstring & path)
{
    static constexpr std::wstring_view  kUncPrefix  = L"\\\\?\\UNC\\";
    static constexpr std::wstring_view  kLongPrefix = L"\\\\?\\";



    if (path.starts_with (kUncPrefix))
    {
        return L"\\\\" + path.substr (kUncPrefix.size());
    }

    if (path.starts_with (kLongPrefix))
    {
        return path.substr (kLongPrefix.size());
    }

    return path;
}

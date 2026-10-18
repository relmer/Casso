#include "Pch.h"

#include "Update/Win32UpdateFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateFileSystem::Exists
//
////////////////////////////////////////////////////////////////////////////////

bool Win32UpdateFileSystem::Exists (const std::wstring & path)
{
    DWORD  attributes = GetFileAttributesW (path.c_str());



    return attributes != INVALID_FILE_ATTRIBUTES;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateFileSystem::CreateDirectoryTree
//
//  The folder and any missing parents; an existing folder is success.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UpdateFileSystem::CreateDirectoryTree (const std::wstring & path)
{
    HRESULT          hr = S_OK;
    std::error_code  ec;



    fs::create_directories (path, ec);
    CBREx (!ec, HRESULT_FROM_WIN32 ((DWORD) ec.value()));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateFileSystem::WriteAllBytes
//
//  Creates or truncates the file and writes all of `bytes`.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UpdateFileSystem::WriteAllBytes (const std::wstring & path, std::span<const Byte> bytes)
{
    HRESULT  hr       = S_OK;
    HANDLE   hFile    = INVALID_HANDLE_VALUE;
    DWORD    written  = 0;
    BOOL     fOk      = FALSE;
    bool     isAll    = false;



    hFile = CreateFileW (path.c_str(),
                         GENERIC_WRITE,
                         0,
                         nullptr,
                         CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL,
                         nullptr);
    CWR (hFile != INVALID_HANDLE_VALUE);

    fOk = ::WriteFile (hFile, bytes.data(), (DWORD) bytes.size(), &written, nullptr);
    CWR (fOk);

    isAll = written == bytes.size();
    CBREx (isAll, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));

Error:
    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle (hFile);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateFileSystem::RemoveFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UpdateFileSystem::RemoveFile (const std::wstring & path)
{
    HRESULT  hr  = S_OK;
    BOOL     fOk = FALSE;



    fOk = DeleteFileW (path.c_str());
    CWR (fOk);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateFileSystem::RenameFile
//
//  Without MOVEFILE_REPLACE_EXISTING, so an existing target is an error.
//  Windows lets a running executable be renamed, which is what lets the
//  update move Casso.exe aside while it runs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UpdateFileSystem::RenameFile (const std::wstring & from, const std::wstring & to)
{
    HRESULT  hr  = S_OK;
    BOOL     fOk = FALSE;



    fOk = MoveFileExW (from.c_str(), to.c_str(), 0);
    CWR (fOk);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateFileSystem::RemoveDirectoryTree
//
//  The folder and everything in it; a missing folder is success.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UpdateFileSystem::RemoveDirectoryTree (const std::wstring & path)
{
    HRESULT          hr = S_OK;
    std::error_code  ec;



    fs::remove_all (path, ec);
    CBREx (!ec, HRESULT_FROM_WIN32 ((DWORD) ec.value()));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateFileSystem::GetFileVersion
//
//  MAJOR.MINOR.PATCH from the file's fixed version resource, whose
//  FILEVERSION Casso.rc fills from Version.h.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UpdateFileSystem::GetFileVersion (const std::wstring & path, ReleaseVersion & outVersion)
{
    HRESULT              hr        = S_OK;
    DWORD                handle    = 0;
    DWORD                size      = 0;
    BOOL                 fOk       = FALSE;
    UINT                 infoSize  = 0;
    VS_FIXEDFILEINFO   * info      = nullptr;
    std::vector<Byte>    buffer;



    outVersion = {};

    size = GetFileVersionInfoSizeW (path.c_str(), &handle);
    CWR (size != 0);

    buffer.resize (size);

    fOk = GetFileVersionInfoW (path.c_str(), 0, size, buffer.data());
    CWR (fOk);

    fOk = VerQueryValueW (buffer.data(), L"\\", (LPVOID *) &info, &infoSize);
    CBREx (fOk && info != nullptr, HRESULT_FROM_WIN32 (ERROR_RESOURCE_TYPE_NOT_FOUND));

    outVersion.major = HIWORD (info->dwFileVersionMS);
    outVersion.minor = LOWORD (info->dwFileVersionMS);
    outVersion.patch = HIWORD (info->dwFileVersionLS);

Error:
    return hr;
}

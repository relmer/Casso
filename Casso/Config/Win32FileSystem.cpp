#include "Pch.h"

#include "Win32FileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReadAllText
//
//  Reads the entire file content as raw bytes (no encoding
//  conversion). Caller is responsible for any UTF-8 BOM / line
//  ending normalization — see MachineConfigUpgrade::NormalizeBytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileSystem::ReadAllText (
    const std::wstring  & path,
    std::string         & outContent)
{
    HRESULT        hr            = S_OK;
    HANDLE         hFile         = INVALID_HANDLE_VALUE;
    LARGE_INTEGER  size          = {};
    DWORD          bytesRead     = 0;
    BOOL           fGotSize      = FALSE;
    BOOL           fGotBytes     = FALSE;



    outContent.clear();

    hFile = CreateFileW (path.c_str(),
                         GENERIC_READ,
                         FILE_SHARE_READ,
                         nullptr,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         nullptr);

    CWR (hFile != INVALID_HANDLE_VALUE);

    fGotSize = GetFileSizeEx (hFile, &size);
    CWR (fGotSize);

    // We don't support files >2GB through this path — they're never
    // going to be configs.
    CBREx (size.QuadPart <= 0x7FFFFFFF, E_BOUNDS);

    if (size.QuadPart > 0)
    {
        outContent.resize (static_cast<size_t> (size.QuadPart));

        fGotBytes = ReadFile (hFile,
                              outContent.data(),
                              static_cast<DWORD> (size.QuadPart),
                              &bytesRead,
                              nullptr);

        CWR (fGotBytes);
        CBR (bytesRead == size.QuadPart);
    }


Error:
    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle (hFile);
    }

    if (FAILED (hr))
    {
        outContent.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteAllText
//
//  Atomic write: stages content into a sibling temp file, then
//  uses MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
//  to swap it into place. Any process reading `path` between the
//  CreateFileW and the MoveFileExW sees the prior content; after
//  the move, sees the new content.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileSystem::WriteAllText (
    const std::wstring  & path,
    const std::string   & content)
{
    HRESULT          hr             = S_OK;
    HANDLE           hTemp          = INVALID_HANDLE_VALUE;
    DWORD            bytesWritten   = 0;
    BOOL             fWrote         = FALSE;
    BOOL             fMoved         = FALSE;
    std::wstring     tempPath;
    std::wstring     parentDir;
    size_t           lastSep        = 0;
    std::error_code  ec;



    // Ensure parent directory exists. create_directories returns
    // false (and sets ec) when the directory already exists, which
    // we treat as success.
    lastSep = path.find_last_of (L"\\/");

    if (lastSep != std::wstring::npos)
    {
        parentDir = path.substr (0, lastSep);
        std::filesystem::create_directories (parentDir, ec);
    }

    tempPath = path + L".casso-tmp";

    hTemp = CreateFileW (tempPath.c_str(),
                         GENERIC_WRITE,
                         0,
                         nullptr,
                         CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL,
                         nullptr);

    CWR (hTemp != INVALID_HANDLE_VALUE);

    if (!content.empty())
    {
        fWrote = WriteFile (hTemp,
                            content.data(),
                            static_cast<DWORD> (content.size()),
                            &bytesWritten,
                            nullptr);

        CWR (fWrote);
        CBR (bytesWritten == content.size());
    }

    CloseHandle (hTemp);
    hTemp = INVALID_HANDLE_VALUE;

    fMoved = MoveFileExW (tempPath.c_str(),
                          path.c_str(),
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);

    CWR (fMoved);


Error:
    if (hTemp != INVALID_HANDLE_VALUE)
    {
        CloseHandle (hTemp);
    }

    if (FAILED (hr))
    {
        // Best-effort temp cleanup; don't override hr.
        DeleteFileW (tempPath.c_str());
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Exists
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FileSystem::Exists (const std::wstring & path)
{
    DWORD  attrs = GetFileAttributesW (path.c_str());



    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Delete
//
//  Idempotent — succeeds (S_OK) when the file is already absent.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileSystem::Delete (const std::wstring & path)
{
    HRESULT  hr    = S_OK;
    BOOL     fDel  = FALSE;
    DWORD    err   = 0;



    fDel = DeleteFileW (path.c_str());

    if (!fDel)
    {
        err = GetLastError();

        // Already gone — treated as success.
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND)
        {
            hr = HRESULT_FROM_WIN32 (err);
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnumerateFiles
//
//  Non-recursive listing. Reports only regular files; "." / ".." /
//  subdirectories are skipped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileSystem::EnumerateFiles (
    const std::wstring         & directory,
    std::vector<std::wstring>  & outFilenames)
{
    HRESULT          hr           = S_OK;
    HANDLE           hFind        = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW findData     = {};
    std::wstring     pattern;
    DWORD            err          = 0;



    outFilenames.clear();

    pattern = directory + L"\\*";

    hFind = FindFirstFileW (pattern.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        err = GetLastError();

        // ERROR_FILE_NOT_FOUND == directory exists but is empty.
        CBRF (err == ERROR_FILE_NOT_FOUND,
              hr = HRESULT_FROM_WIN32 (err));

        // Empty directory — fall through with empty outFilenames.
        goto Error;
    }

    do
    {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            continue;
        }

        outFilenames.push_back (findData.cFileName);
    }
    while (FindNextFileW (hFind, &findData));


Error:
    if (hFind != INVALID_HANDLE_VALUE)
    {
        FindClose (hFind);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnumerateDirectories
//
//  Non-recursive listing of sub-directories under `directory`. Skips
//  "." / ".." and regular files. Mirrors EnumerateFiles' error
//  convention: empty (S_OK) when the directory exists but holds no
//  sub-dirs; ERROR_PATH_NOT_FOUND (wrapped) when the directory
//  itself is missing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileSystem::EnumerateDirectories (
    const std::wstring         & directory,
    std::vector<std::wstring>  & outDirNames)
{
    HRESULT          hr           = S_OK;
    HANDLE           hFind        = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW findData     = {};
    std::wstring     pattern;
    DWORD            err          = 0;



    outDirNames.clear();

    pattern = directory + L"\\*";

    hFind = FindFirstFileW (pattern.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        err = GetLastError();

        CBRF (err == ERROR_FILE_NOT_FOUND,
              hr = HRESULT_FROM_WIN32 (err));

        goto Error;
    }

    do
    {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            continue;
        }

        // Skip "." and ".."
        if (findData.cFileName[0] == L'.' &&
            (findData.cFileName[1] == L'\0' ||
             (findData.cFileName[1] == L'.' && findData.cFileName[2] == L'\0')))
        {
            continue;
        }

        outDirNames.push_back (findData.cFileName);
    }
    while (FindNextFileW (hFind, &findData));


Error:
    if (hFind != INVALID_HANDLE_VALUE)
    {
        FindClose (hFind);
    }

    return hr;
}

#include "Pch.h"

#include "Seams/Win32ProcessLauncher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ProcessLauncher::Launch
//
//  The command line is the quoted executable path followed by the arguments,
//  in a writable buffer because CreateProcessW may modify it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ProcessLauncher::Launch (const std::wstring & exePath, const std::wstring & arguments)
{
    HRESULT               hr          = S_OK;
    STARTUPINFOW          startup     = {};
    PROCESS_INFORMATION   process     = {};
    std::wstring          commandLine = L"\"" + exePath + L"\"";
    std::wstring          directory   = std::filesystem::path (exePath).parent_path().wstring();
    std::vector<wchar_t>  buffer;
    BOOL                  created     = FALSE;
    BOOL                  closed      = FALSE;



    if (!arguments.empty())
    {
        commandLine += L" " + arguments;
    }

    buffer.assign (commandLine.begin(), commandLine.end());
    buffer.push_back (L'\0');

    startup.cb = sizeof (startup);

    created = CreateProcessW (exePath.c_str(), buffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                              directory.empty() ? nullptr : directory.c_str(), &startup, &process);
    CWR (created);

    closed = CloseHandle (process.hThread);
    IGNORE_RETURN_VALUE (closed, TRUE);

    closed = CloseHandle (process.hProcess);
    IGNORE_RETURN_VALUE (closed, TRUE);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ProcessLauncher::Exists
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ProcessLauncher::Exists (const std::wstring & exePath)
{
    DWORD  attributes = GetFileAttributesW (exePath.c_str());



    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

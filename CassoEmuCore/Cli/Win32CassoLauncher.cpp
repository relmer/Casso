#include "Pch.h"

#include "Cli/CommandLineQuoting.h"
#include "Cli/ICassoLauncher.h"

#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32CassoLauncher::GetCassoPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32CassoLauncher::GetCassoPath()
{
    std::wstring  path (MAX_PATH, L'\0');
    DWORD         length = 0;



    length = GetModuleFileNameW (nullptr, path.data(), (DWORD) path.size());
    path.resize (length);

    return (std::filesystem::path (path).parent_path() / L"Casso.exe").wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32CassoLauncher::Launch
//
//  Its window opens without taking the foreground from the client.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Win32CassoLauncher::Launch (const std::wstring & arguments, std::string & error)
{
    std::wstring         exe         = GetCassoPath();
    std::wstring         commandLine = CommandLineQuoting::QuoteArgument (exe) + L" " + arguments;
    STARTUPINFOW         startup     = {};
    PROCESS_INFORMATION  process     = {};
    BOOL                 isStarted   = FALSE;



    startup.cb          = sizeof (startup);
    startup.dwFlags     = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNOACTIVATE;

    isStarted = CreateProcessW (exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
                                std::filesystem::path (exe).parent_path().c_str(), &startup, &process);

    if (!isStarted)
    {
        error = std::format ("Casso could not be started from {} (error {}).", TextEncoding::WideToNarrow (exe), GetLastError());
        return 0;
    }

    CloseHandle (process.hThread);
    CloseHandle (process.hProcess);

    return process.dwProcessId;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32CassoLauncher::IsRunning
//
////////////////////////////////////////////////////////////////////////////////

bool Win32CassoLauncher::IsRunning (uint32_t processId)
{
    HANDLE  process  = OpenProcess (SYNCHRONIZE, FALSE, processId);
    bool    isAlive  = false;



    if (process != nullptr)
    {
        isAlive = WaitForSingleObject (process, 0) == WAIT_TIMEOUT;
        CloseHandle (process);
    }

    return isAlive;
}

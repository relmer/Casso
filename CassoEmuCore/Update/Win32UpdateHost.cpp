#include "Pch.h"

#include "Update/Win32UpdateHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateHost::Win32UpdateHost
//
//  Counts this instance in for as long as the object lives. A semaphore
//  that cannot be created leaves the count unknown, which reads as no other
//  instance; the update then proceeds as it would have before the count
//  existed.
//
////////////////////////////////////////////////////////////////////////////////

Win32UpdateHost::Win32UpdateHost()
{
    constexpr LONG  kMaxInstances = 0x7FFF;



    BOOL isRaised = FALSE;



    m_instanceSemaphore = CreateSemaphoreW (nullptr, 0, kMaxInstances, kpszInstanceName);

    if (m_instanceSemaphore != nullptr)
    {
        isRaised = ReleaseSemaphore (m_instanceSemaphore, 1, nullptr);
        IGNORE_RETURN_VALUE (isRaised, TRUE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateHost::~Win32UpdateHost
//
////////////////////////////////////////////////////////////////////////////////

Win32UpdateHost::~Win32UpdateHost()
{
    DWORD  waited = 0;



    if (m_instanceSemaphore != nullptr)
    {
        waited = WaitForSingleObject (m_instanceSemaphore, 0);
        IGNORE_RETURN_VALUE (waited, WAIT_OBJECT_0);

        CloseHandle (m_instanceSemaphore);
        m_instanceSemaphore = nullptr;
    }

    if (m_checkLock != nullptr)
    {
        CloseHandle (m_checkLock);
        m_checkLock = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateHost::TryAcquireCheckLock
//
////////////////////////////////////////////////////////////////////////////////

bool Win32UpdateHost::TryAcquireCheckLock()
{
    std::lock_guard<std::mutex>  guard   (m_lockMutex);
    HANDLE                       created = nullptr;
    DWORD                        error   = ERROR_SUCCESS;



    if (m_checkLock == nullptr)
    {
        created = CreateMutexW (nullptr, FALSE, kpszCheckLockName);
        error   = GetLastError();

        if (created != nullptr && error == ERROR_ALREADY_EXISTS)
        {
            CloseHandle (created);
            created = nullptr;
        }

        m_checkLock = created;
    }

    return m_checkLock != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateHost::IsOtherInstanceRunning
//
//  Reads the count by raising it and lowering it again: ReleaseSemaphore
//  reports the count before the raise, which includes this instance.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32UpdateHost::IsOtherInstanceRunning()
{
    LONG   previous = 0;
    BOOL   isRaised = FALSE;
    DWORD  waited   = 0;



    if (m_instanceSemaphore == nullptr)
    {
        return false;
    }

    isRaised = ReleaseSemaphore (m_instanceSemaphore, 1, &previous);

    if (isRaised)
    {
        waited = WaitForSingleObject (m_instanceSemaphore, 0);
        IGNORE_RETURN_VALUE (waited, WAIT_OBJECT_0);
    }

    return isRaised && previous > 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateHost::GetCurrentPid
//
////////////////////////////////////////////////////////////////////////////////

DWORD Win32UpdateHost::GetCurrentPid()
{
    return GetCurrentProcessId();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateHost::LaunchProcess
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UpdateHost::LaunchProcess (const std::wstring & exePath, const std::wstring & arguments)
{
    HRESULT              hr          = S_OK;
    STARTUPINFOW         startup     = { sizeof (startup) };
    PROCESS_INFORMATION  process     = {};
    std::wstring         commandLine = std::format (L"\"{}\" {}", exePath, arguments);
    BOOL                 isCreated   = FALSE;



    isCreated = CreateProcessW (exePath.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                                0, nullptr, nullptr, &startup, &process);
    CWR (isCreated);

    CloseHandle (process.hThread);
    CloseHandle (process.hProcess);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateHost::WaitForProcessExit
//
//  A process that cannot be opened has already exited, which is what the
//  caller was waiting for.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UpdateHost::WaitForProcessExit (DWORD processId, DWORD timeoutMs)
{
    HRESULT  hr      = S_OK;
    HANDLE   process = nullptr;
    DWORD    waited  = 0;



    process = OpenProcess (SYNCHRONIZE, FALSE, processId);
    BAIL_OUT_IF (process == nullptr, S_OK);

    waited = WaitForSingleObject (process, timeoutMs);
    CBREx (waited == WAIT_OBJECT_0, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

Error:
    if (process != nullptr)
    {
        CloseHandle (process);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateHost::GetDownloadFolder
//
//  %LOCALAPPDATA%\Casso\Updates, beside the rest of Casso's per-user data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UpdateHost::GetDownloadFolder (std::wstring & outPath)
{
    HRESULT  hr        = S_OK;
    PWSTR    localData = nullptr;



    hr = SHGetKnownFolderPath (FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &localData);
    CHR (hr);

    outPath  = localData;
    outPath += L"\\Casso\\Updates";

Error:
    CoTaskMemFree (localData);
    return hr;
}

#pragma once

#include "Pch.h"

#include "Update/IUpdateHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateHost
//
//  IUpdateHost for the running process.
//
//  The check lock is a named mutex whose existence is the lock: the first
//  instance to create it keeps the handle for life, and a later one finds
//  it already there. The instance count is a named semaphore every instance
//  raises by one for as long as it runs, so its count is the number of open
//  instances; a mutex can say whether it is held, not how many hold it.
//
////////////////////////////////////////////////////////////////////////////////

class Win32UpdateHost : public IUpdateHost
{
public:
    static constexpr LPCWSTR  kpszCheckLockName = L"Local\\Casso.UpdateCheck";
    static constexpr LPCWSTR  kpszInstanceName  = L"Local\\Casso.Instance";

    Win32UpdateHost();
    ~Win32UpdateHost() override;

    Win32UpdateHost             (const Win32UpdateHost &) = delete;
    Win32UpdateHost & operator= (const Win32UpdateHost &) = delete;

    bool     TryAcquireCheckLock    () override;
    bool     IsOtherInstanceRunning () override;
    DWORD    GetCurrentPid          () override;
    HRESULT  LaunchProcess          (const std::wstring & exePath, const std::wstring & arguments) override;
    HRESULT  WaitForProcessExit     (DWORD processId, DWORD timeoutMs) override;
    HRESULT  GetDownloadFolder      (std::wstring & outPath) override;

private:
    std::mutex  m_lockMutex;
    HANDLE      m_checkLock         = nullptr;
    HANDLE      m_instanceSemaphore = nullptr;
};

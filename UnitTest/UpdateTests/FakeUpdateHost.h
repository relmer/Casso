#pragma once

#include "Pch.h"

#include "Update/IUpdateHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeUpdateHost
//
//  IUpdateHost with every answer set by the test and every call recorded.
//  No real lock, process or folder is touched.
//
//  Header-only on purpose; lives only in the test binary.
//
////////////////////////////////////////////////////////////////////////////////

class FakeUpdateHost : public IUpdateHost
{
public:
    static constexpr DWORD  kPid = 4242;

    bool          hasCheckLock        = true;
    bool          isOtherInstanceOpen = false;
    HRESULT       launchResult        = S_OK;
    std::wstring  launchedExe;
    std::wstring  launchedArgs;
    int           launches            = 0;
    DWORD         waitedPid           = 0;
    int           lockRequests        = 0;
    std::wstring  downloadFolder      = L"C:\\Users\\me\\AppData\\Local\\Casso\\Updates";

    bool TryAcquireCheckLock() override
    {
        lockRequests++;
        return hasCheckLock;
    }

    bool IsOtherInstanceRunning() override
    {
        return isOtherInstanceOpen;
    }

    DWORD GetCurrentPid() override
    {
        return kPid;
    }

    HRESULT LaunchProcess (const std::wstring & exePath, const std::wstring & arguments) override
    {
        launches++;
        launchedExe  = exePath;
        launchedArgs = arguments;
        return launchResult;
    }

    HRESULT WaitForProcessExit (DWORD processId, DWORD timeoutMs) override
    {
        (void) timeoutMs;
        waitedPid = processId;
        return S_OK;
    }

    HRESULT GetDownloadFolder (std::wstring & outPath) override
    {
        outPath = downloadFolder;
        return S_OK;
    }
};


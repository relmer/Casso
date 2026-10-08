#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IUpdateHost
//
//  The process-level operations an update needs, behind one seam so a test
//  never touches a real mutex, process or folder:
//
//    TryAcquireCheckLock     true for the one instance that may run the
//                            automatic check (it keeps the lock for life)
//    IsOtherInstanceRunning  whether another Casso is open, which an update
//                            must not replace files under
//    GetCurrentPid           this process's id, for the relaunch arguments
//    LaunchProcess           starts an executable with arguments
//    WaitForProcessExit      waits up to `timeoutMs` for a process to end
//    GetDownloadFolder       where a downloaded package is written
//
////////////////////////////////////////////////////////////////////////////////

class IUpdateHost
{
public:
    virtual ~IUpdateHost() = default;

    virtual bool     TryAcquireCheckLock    () = 0;
    virtual bool     IsOtherInstanceRunning () = 0;
    virtual DWORD    GetCurrentPid          () = 0;
    virtual HRESULT  LaunchProcess          (const std::wstring & exePath, const std::wstring & arguments) = 0;
    virtual HRESULT  WaitForProcessExit     (DWORD processId, DWORD timeoutMs) = 0;
    virtual HRESULT  GetDownloadFolder      (std::wstring & outPath) = 0;
};

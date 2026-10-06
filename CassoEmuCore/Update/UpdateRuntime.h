#pragma once

#include "Pch.h"

#include "Net/WinHttpClient.h"
#include "Update/AuthenticodeVerifier.h"
#include "Update/MsixPackageDeployer.h"
#include "Update/UpdateService.h"
#include "Update/Win32InstallEnvironment.h"
#include "Update/Win32UpdateFileSystem.h"
#include "Update/Win32UpdateHost.h"
#include "Update/WindowUpdateResultPoster.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateRuntime
//
//  The production wiring of UpdateService: every seam's real
//  implementation, owned together so the service is destroyed (and its
//  workers joined) before any of them. Results are posted to `hwnd` as
//  `message`.
//
////////////////////////////////////////////////////////////////////////////////

class UpdateRuntime
{
public:
    UpdateRuntime (HWND hwnd, UINT message);

    UpdateRuntime             (const UpdateRuntime &) = delete;
    UpdateRuntime & operator= (const UpdateRuntime &) = delete;

    UpdateService & GetService() { return *m_service; }

    static std::int64_t  GetUtcNow();
    static ReleaseArch   GetRunningArch();

private:
    WinHttpClient                   m_http;
    AuthenticodeVerifier            m_verifier;
    Win32InstallEnvironment         m_environment;
    Win32UpdateFileSystem           m_fileSystem;
    MsixPackageDeployer             m_deployer;
    Win32UpdateHost                 m_host;
    WindowUpdateResultPoster        m_poster;
    std::unique_ptr<UpdateService>  m_service;
};

#include "Pch.h"

#include "Update/UpdateRuntime.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateRuntime::UpdateRuntime
//
////////////////////////////////////////////////////////////////////////////////

UpdateRuntime::UpdateRuntime (HWND hwnd, UINT message) :
    m_poster (hwnd, message)
{
    UpdateServiceDeps  deps;



    deps.http        = &m_http;
    deps.verifier    = &m_verifier;
    deps.environment = &m_environment;
    deps.fileSystem  = &m_fileSystem;
    deps.deployer    = &m_deployer;
    deps.host        = &m_host;
    deps.poster      = &m_poster;
    deps.clock       = &UpdateRuntime::GetUtcNow;

    m_service = std::make_unique<UpdateService> (deps);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateRuntime::GetUtcNow
//
////////////////////////////////////////////////////////////////////////////////

std::int64_t UpdateRuntime::GetUtcNow()
{
    return (std::int64_t) std::chrono::duration_cast<std::chrono::seconds> (
               std::chrono::system_clock::now().time_since_epoch()).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateRuntime::GetRunningArch
//
//  The processor this build was compiled for, which picks the release zip.
//
////////////////////////////////////////////////////////////////////////////////

ReleaseArch UpdateRuntime::GetRunningArch()
{
#if defined(_M_ARM64)
    return ReleaseArch::Arm64;
#else
    return ReleaseArch::X64;
#endif
}

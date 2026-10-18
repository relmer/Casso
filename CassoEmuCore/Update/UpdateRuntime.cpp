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
    UpdateServiceDeps            deps;
    std::wstring                 variable = ReadVariable (LocalFeedHttpClient::kpszFeedVariable);
    std::optional<std::wstring>  feedPath = LocalFeedHttpClient::SelectFeedPath (variable.c_str());



    // A test of a signed update reads the release from a JSON on disk;
    // every other run asks GitHub.
    if (feedPath.has_value())
    {
        m_localFeed = std::make_unique<LocalFeedHttpClient> (*feedPath, m_feedReader, &m_http);
    }

    deps.http        = m_localFeed ? static_cast<IHttpClient *> (m_localFeed.get()) : &m_http;
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





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateRuntime::ReadVariable
//
//  An environment variable's value, empty when it is not set.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateRuntime::ReadVariable (LPCWSTR name)
{
    std::wstring  value;
    DWORD         length = GetEnvironmentVariableW (name, nullptr, 0);



    if (length > 1)
    {
        value.resize (length);
        length = GetEnvironmentVariableW (name, value.data(), length);
        value.resize (length);
    }

    return value;
}
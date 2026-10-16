#include "Pch.h"

#include "AppleLink/RunningCassoRegistry.h"
#include "Config/IFileSystem.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/JsonWriter.h"
#include "Core/PathResolver.h"
#include "Seams/IProcessProbe.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RunningCassoRegistry::RunningCassoRegistry
//
////////////////////////////////////////////////////////////////////////////////

RunningCassoRegistry::RunningCassoRegistry (IFileSystem & fileSystem, IProcessProbe & probe, std::wstring folder) :
    m_fileSystem (fileSystem),
    m_probe      (probe),
    m_folder     (std::move (folder))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunningCassoRegistry::GetDefaultFolder
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RunningCassoRegistry::GetDefaultFolder()
{
    return (PathResolver::GetLocalAppDataDir (L"Casso") / L"Links").wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunningCassoRegistry::Publish
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RunningCassoRegistry::Publish (DWORD pid, int serialPort, const std::string & machine, uint16_t tcpPort)
{
    HRESULT       hr      = S_OK;
    RunningCasso  running;
    bool          alive   = false;



    alive = m_probe.TryGetStartTime (pid, running.started);
    CBREx (alive, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    running.pid        = pid;
    running.machine    = machine;
    running.serialPort = serialPort;
    running.address    = std::format ("127.0.0.1:{}", tcpPort);

    hr = m_fileSystem.WriteAllText (MakePath (pid, serialPort), ToJson (running));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunningCassoRegistry::Withdraw
//
////////////////////////////////////////////////////////////////////////////////

void RunningCassoRegistry::Withdraw (DWORD pid, int serialPort)
{
    std::wstring  path = MakePath (pid, serialPort);
    HRESULT       hr   = S_OK;



    if (m_fileSystem.Exists (path))
    {
        hr = m_fileSystem.Delete (path);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunningCassoRegistry::Enumerate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RunningCassoRegistry::Enumerate (std::vector<RunningCasso> & outRunning)
{
    HRESULT                    hr    = S_OK;
    std::vector<std::wstring>  files;



    outRunning.clear();

    BAIL_OUT_IF (!m_fileSystem.DirectoryExists (m_folder), S_OK);

    hr = m_fileSystem.EnumerateFiles (m_folder, files);
    CHR (hr);

    for (const std::wstring & name : files)
    {
        std::wstring  path    = m_folder + L"\\" + name;
        std::string   text;
        RunningCasso  running;
        uint64_t      started = 0;
        bool          current = false;
        HRESULT       read    = S_OK;

        if (!name.ends_with (L".json"))
        {
            continue;
        }

        read    = m_fileSystem.ReadAllText (path, text);
        current = SUCCEEDED (read) && FromJson (text, running)
               && m_probe.TryGetStartTime (running.pid, started) && started == running.started;

        if (current)
        {
            outRunning.push_back (running);
        }
        else
        {
            read = m_fileSystem.Delete (path);
            IGNORE_RETURN_VALUE (read, S_OK);
        }
    }

    std::sort (outRunning.begin(), outRunning.end(), [] (const RunningCasso & a, const RunningCasso & b)
    {
        return a.pid != b.pid ? a.pid < b.pid : a.serialPort < b.serialPort;
    });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunningCassoRegistry::ToJson
//
////////////////////////////////////////////////////////////////////////////////

std::string RunningCassoRegistry::ToJson (const RunningCasso & running)
{
    std::vector<std::pair<std::string, JsonValue>>  fields;



    fields.emplace_back ("pid",        JsonValue ((double) running.pid));
    fields.emplace_back ("started",    JsonValue (std::to_string (running.started)));
    fields.emplace_back ("machine",    JsonValue (running.machine));
    fields.emplace_back ("serialPort", JsonValue ((double) running.serialPort));
    fields.emplace_back ("address",    JsonValue (running.address));

    return JsonWriter::Write (JsonValue (std::move (fields)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunningCassoRegistry::FromJson
//
////////////////////////////////////////////////////////////////////////////////

bool RunningCassoRegistry::FromJson (const std::string & text, RunningCasso & outRunning)
{
    JsonValue       root;
    JsonParseError  error;
    std::string     started;
    int             pid    = 0;
    HRESULT         parsed = JsonParser::Parse (text, root, error);
    bool            usable = false;



    outRunning = RunningCasso();

    usable = SUCCEEDED (parsed) && root.GetType() == JsonType::Object
          && root.HasInt ("pid", pid) && pid > 0
          && root.HasString ("started", started) && !started.empty()
          && root.HasInt ("serialPort", outRunning.serialPort)
          && root.HasString ("address", outRunning.address);

    if (usable)
    {
        root.HasString ("machine", outRunning.machine);

        outRunning.pid     = (DWORD) pid;
        outRunning.started = std::strtoull (started.c_str(), nullptr, 10);
    }

    return usable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunningCassoRegistry::MakePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RunningCassoRegistry::MakePath (DWORD pid, int serialPort) const
{
    return std::format (L"{}\\{}-{}.json", m_folder, pid, serialPort);
}

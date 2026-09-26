#include "Pch.h"

#include "Cli/DebugMode.h"

#include "Cli/CliOutput.h"
#include "Cli/DebugAttachRunner.h"
#include "Cli/DebugBatchRunner.h"
#include "Cli/HostFile.h"
#include "Config/Win32FileSystem.h"
#include "Core/TextEncoding.h"
#include "Debugger/Channel/InstanceLister.h"
#include "Debugger/Channel/Win32InstanceDirectory.h"
#include "Shell/FileRomSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMode::Run
//
//  Output goes to standard output as the runner composed it, and diagnostics
//  to the error stream re-encoded for the console, as the disk edge does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugMode::Run (const CommandLineOptions & options, int & exitCode)
{
    HRESULT           hr = S_OK;
    std::string       script;
    Win32FileSystem   files;
    FileRomSource     roms;
    DebugBatchRunner  runner (roms, files);
    DebugBatchResult  result;
    bool              isWritten = false;



    exitCode = DebugBatchRunner::kNothingStarted;

    if (options.debug.list)
    {
        ListInstances (exitCode);
        BAIL_OUT_IF (true, S_OK);
    }

    if (!options.debug.scriptPath.empty())
    {
        hr = ReadScript (options.debug.scriptPath, script);
        CHRF (hr, std::cerr << "Error: the script " << options.debug.scriptPath << " could not be read\n");
    }

    if (options.debug.isAttach)
    {
        hr = AttachAndRun (options, script, exitCode);
        BAIL_OUT_IF (true, hr);
    }

    hr = runner.Run (options.debug, script, result);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (!result.output.empty())
    {
        isWritten = CliOutput::TryWrite (stdout, result.output);
        IGNORE_RETURN_VALUE (isWritten, false);
    }

    if (!result.diagnostics.empty())
    {
        std::cerr << TextEncoding::NarrowToConsole (result.diagnostics);
        std::cerr.flush();
    }

    exitCode = result.exitStatus;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMode::ReadScript
//
//  A dash reads standard input to its end.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugMode::ReadScript (const std::string & path, std::string & text)
{
    HRESULT            hr = S_OK;
    std::stringstream  buffer;



    if (path == "-")
    {
        buffer << std::cin.rdbuf();
        text = buffer.str();
    }
    else
    {
        hr = HostFile::ReadAll (path, text);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMode::ListInstances
//
////////////////////////////////////////////////////////////////////////////////

void DebugMode::ListInstances (int & exitCode)
{
    Win32InstanceDirectory  directory;
    std::string             text      = InstanceLister::Format (InstanceLister::List (directory));
    bool                    isWritten = CliOutput::TryWrite (stdout, text);



    IGNORE_RETURN_VALUE (isWritten, false);
    exitCode = DebugAttachRunner::kOk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMode::AttachAndRun
//
//  An instance that cannot be reached is the channel-closed status, the one a
//  script already treats as "the Casso it was talking to is not there".
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugMode::AttachAndRun (const CommandLineOptions & options, const std::string & script, int & exitCode)
{
    HRESULT                          hr        = S_OK;
    Win32InstanceDirectory           directory;
    std::unique_ptr<IChannelClient>  client    = directory.Connect (options.debug.attachPid);
    DebugAttachResult                result;
    bool                             isWritten = false;



    exitCode = DebugAttachRunner::kChannelClosed;

    CBREx (client != nullptr, S_OK);

    DebugAttachRunner::Run (*client, options.debug, script, result);

    if (!result.output.empty())
    {
        isWritten = CliOutput::TryWrite (stdout, result.output);
        IGNORE_RETURN_VALUE (isWritten, false);
    }

    if (!result.diagnostics.empty())
    {
        std::cerr << TextEncoding::NarrowToConsole (result.diagnostics);
        std::cerr.flush();
    }

    exitCode = result.exitStatus;

Error:
    if (client == nullptr)
    {
        std::cerr << "Error: no Casso with a debug channel open is running as process " << options.debug.attachPid << "\n"
                  << "       Start Casso with --debugger. Use CassoCli debug --list to list the running ones.\n";
    }

    return hr;
}

#include "Pch.h"

#include "Cli/DebugMode.h"

#include "Cli/CliOutput.h"
#include "Cli/DebugBatchRunner.h"
#include "Cli/HostFile.h"
#include "Config/Win32FileSystem.h"
#include "Core/TextEncoding.h"
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

    if (!options.debug.scriptPath.empty())
    {
        hr = ReadScript (options.debug.scriptPath, script);
        CHRF (hr, std::cerr << "Error: the script " << options.debug.scriptPath << " could not be read\n");
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

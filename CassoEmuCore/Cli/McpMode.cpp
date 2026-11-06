#include "Pch.h"

#include "Cli/McpMode.h"

#include "Cli/ICassoLauncher.h"
#include "Cli/McpServer.h"
#include "Cli/McpTools.h"
#include "CommandLineOptions.h"
#include "Core/TextEncoding.h"
#include "Debugger/Channel/ChannelProtocol.h"
#include "Debugger/Channel/Win32InstanceDirectory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  McpMode::Run
//
//  Binary mode on both streams, so a line ends in the LF the client sent and
//  the server writes, and no CR is added. The scratch folder for screenshots
//  is removed when the client closes standard input.
//
////////////////////////////////////////////////////////////////////////////////

int McpMode::Run (const CommandLineOptions & options)
{
    Win32InstanceDirectory  directory;
    Win32CassoLauncher      launcher;
    std::wstring            scratch   = CreateScratchFolder();
    McpTools                tools (directory, launcher, MakeClient (options, scratch), scratch);
    McpServer               server (tools);
    std::string             line;
    int                     ch        = 0;
    bool                    isEnd     = false;



    (void) _setmode (_fileno (stdin),  _O_BINARY);
    (void) _setmode (_fileno (stdout), _O_BINARY);

    while (!isEnd)
    {
        std::string  response;



        ch    = getchar();
        isEnd = ch == EOF;

        if (!isEnd && ch != '\n')
        {
            line += (char) ch;
            continue;
        }

        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        response = line.empty() ? std::string() : server.HandleLine (line);
        line.clear();

        if (!response.empty())
        {
            response += '\n';
            fwrite (response.data(), 1, response.size(), stdout);
            fflush (stdout);
        }
    }

    if (!scratch.empty())
    {
        RemoveDirectoryW (scratch.c_str());
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpMode::MakeClient
//
//  A relative --allow-path is made full here, against this process's current
//  directory, since Casso resolves a relative path against the directory
//  Casso runs in.
//
////////////////////////////////////////////////////////////////////////////////

ChannelClient McpMode::MakeClient (const CommandLineOptions & options, const std::wstring & scratchFolder)
{
    ChannelClient    client;
    std::error_code  code;
    std::wstring     folder;



    client.name         = McpTools::kClientName;
    client.allowScripts = options.mcp.allowScripts;

    for (const std::string & path : options.mcp.allowPaths)
    {
        folder = std::filesystem::absolute (TextEncoding::NarrowToWide (path), code).wstring();
        client.allowPaths.push_back (code ? TextEncoding::NarrowToWide (path) : folder);
    }

    if (!scratchFolder.empty())
    {
        client.allowPaths.push_back (scratchFolder);
    }

    return client;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpMode::CreateScratchFolder
//
//  A folder for this process in the temporary folder, so a screenshot
//  is allowed without allowing the whole temporary folder. Empty when it
//  could not be made, and screenshots then go to the temporary folder.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring McpMode::CreateScratchFolder()
{
    HRESULT       hr               = S_OK;
    wchar_t       temp[MAX_PATH]   = {};
    DWORD         length           = 0;
    std::wstring  folder;
    BOOL          isMade           = FALSE;



    length = GetTempPathW (MAX_PATH, temp);
    CWR (length != 0);

    folder = std::format (L"{}casso-mcp-{}", temp, GetCurrentProcessId());
    isMade = CreateDirectoryW (folder.c_str(), nullptr);
    CWR (isMade);

Error:
    if (FAILED (hr))
    {
        folder.clear();
    }

    return folder;
}

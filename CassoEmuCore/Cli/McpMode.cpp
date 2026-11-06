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



    (void) _setmode (_fileno (stdin),  _O_BINARY);
    (void) _setmode (_fileno (stdout), _O_BINARY);

    Serve (server, std::cin, std::cout);

    if (!scratch.empty())
    {
        RemoveDirectoryW (scratch.c_str());
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpMode::Serve
//
//  A line longer than the debug channel's ChannelProtocol::kMaxLineBytes gets
//  the "line too long" error as soon as it passes the limit. The rest of that
//  line is discarded as it arrives, so the buffer never grows past the limit.
//
////////////////////////////////////////////////////////////////////////////////

void McpMode::Serve (McpServer & server, std::istream & in, std::ostream & out)
{
    std::string  line;
    int          ch           = 0;
    bool         isEnd        = false;
    bool         isDiscarding = false;
    bool         isTooLong    = false;



    while (!isEnd)
    {
        std::string  response;



        ch    = in.get();
        isEnd = ch == std::istream::traits_type::eof();

        if (isDiscarding)
        {
            isDiscarding = !isEnd && ch != '\n';
            continue;
        }

        if (!isEnd && ch != '\n')
        {
            line      += (char) ch;
            isTooLong  = line.size() > ChannelProtocol::kMaxLineBytes;

            if (!isTooLong)
            {
                continue;
            }

            response     = McpServer::RespondLineTooLong();
            isDiscarding = true;
        }
        else
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            response = line.empty() ? std::string() : server.HandleLine (line);
        }

        line.clear();

        if (!response.empty())
        {
            out << response << '\n';
            out.flush();
        }
    }
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

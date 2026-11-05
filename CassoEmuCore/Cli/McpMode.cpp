#include "Pch.h"

#include "Cli/McpMode.h"

#include "Cli/ICassoLauncher.h"
#include "Cli/McpServer.h"
#include "Cli/McpTools.h"
#include "Debugger/Channel/Win32InstanceDirectory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  McpMode::Run
//
//  Binary mode on both streams, so a line ends in the LF the client sent and
//  the server writes, and no CR is added.
//
////////////////////////////////////////////////////////////////////////////////

int McpMode::Run()
{
    Win32InstanceDirectory  directory;
    Win32CassoLauncher      launcher;
    McpTools                tools (directory, launcher);
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

    return 0;
}

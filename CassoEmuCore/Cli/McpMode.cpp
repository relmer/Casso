#include "Pch.h"

#include "Cli/McpMode.h"

#include "Cli/ICassoLauncher.h"
#include "Cli/McpServer.h"
#include "Cli/McpTools.h"
#include "Debugger/Channel/ChannelProtocol.h"
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



    (void) _setmode (_fileno (stdin),  _O_BINARY);
    (void) _setmode (_fileno (stdout), _O_BINARY);

    Serve (server, std::cin, std::cout);

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
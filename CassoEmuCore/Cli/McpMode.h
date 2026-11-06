#pragma once

#include "Pch.h"

struct ChannelClient;
struct CommandLineOptions;





////////////////////////////////////////////////////////////////////////////////
//
//  McpMode
//
//  `CassoCli mcp`: the MCP server on standard input and output, until the
//  client closes standard input. Nothing but protocol messages is written to
//  standard output; a diagnostic goes to standard error.
//
////////////////////////////////////////////////////////////////////////////////

class McpMode
{
public:
    static int  Run (const CommandLineOptions & options);

    //  The hello's client: the name, each --allow-path folder made full
    //  against the current directory, the scratch folder when there is one,
    //  and --allow-scripts.
    static ChannelClient  MakeClient (const CommandLineOptions & options, const std::wstring & scratchFolder);

private:
    static std::wstring   CreateScratchFolder ();
};

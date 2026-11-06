#pragma once

#include "Pch.h"

class McpServer;





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
    static int   Run   ();

    //  Each line of `in` to the server and each response to `out`, until
    //  `in` ends.
    static void  Serve (McpServer & server, std::istream & in, std::ostream & out);
};

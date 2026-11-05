#pragma once

#include "Pch.h"





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
    static int  Run ();
};

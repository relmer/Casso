#pragma once

#include "Core/JsonValue.h"

class McpTools;





////////////////////////////////////////////////////////////////////////////////
//
//  McpServer
//
//  The Model Context Protocol over JSON-RPC 2.0, one message a line, as
//  `CassoCli mcp` serves it on standard input and output: initialize, ping,
//  tools/list and tools/call, the tools being McpTools'. A notification has
//  no response; a request with a method the server doesn't have gets the
//  JSON-RPC error for it.
//
////////////////////////////////////////////////////////////////////////////////

class McpServer
{
public:
    static constexpr const char * kProtocolVersion = "2025-06-18";

    static constexpr int  kParseError     = -32700;
    static constexpr int  kInvalidRequest = -32600;
    static constexpr int  kMethodNotFound = -32601;
    static constexpr int  kInvalidParams  = -32602;

    explicit McpServer (McpTools & tools);

    //  One line in; the response line, or an empty string for none.
    std::string  HandleLine (const std::string & line);

    //  Whether a client's protocol version is one the server supports, and
    //  so the version in its response.
    static bool  IsKnownVersion (const std::string & version);

    //  A JSON value on one line.
    static std::string  WriteCompact (const JsonValue & value);

    //  The error response to a line longer than ChannelProtocol::kMaxLineBytes.
    static std::string  RespondLineTooLong ();

private:
    using Members = std::vector<std::pair<std::string, JsonValue>>;

    JsonValue    Initialize (const JsonValue & params);
    JsonValue    CallTool   (const JsonValue & params, bool & isBadParams);

    static std::string  Respond      (const JsonValue & id, JsonValue result);
    static std::string  RespondError (const JsonValue & id, int code, const std::string & message);

    McpTools  & m_tools;
};

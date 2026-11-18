#include "Pch.h"

#include "Cli/McpServer.h"

#include "Version.h"
#include "Cli/McpTools.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Debugger/Channel/ChannelProtocol.h"





////////////////////////////////////////////////////////////////////////////////
//
//  McpServer::McpServer
//
////////////////////////////////////////////////////////////////////////////////

McpServer::McpServer (McpTools & tools) :
    m_tools (tools)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpServer::HandleLine
//
////////////////////////////////////////////////////////////////////////////////

std::string McpServer::HandleLine (const std::string & line)
{
    JsonValue          message;
    JsonParseError     error;
    HRESULT            hr          = JsonParser::Parse (line, message, error);
    std::string        method;
    std::string        version;
    const JsonValue  * id          = nullptr;
    const JsonValue  * params      = nullptr;
    JsonValue          noParams    = JsonValue (Members {});
    JsonValue          result;
    bool               isBadParams = false;



    if (FAILED (hr))
    {
        return RespondError (JsonValue (nullptr), kParseError, "The line is not JSON.");
    }

    for (const auto & [key, value] : message.GetObjectEntries())
    {
        id = (key == "id") ? &value : id;
    }

    if (message.GetType() != JsonType::Object || !message.HasString ("jsonrpc", version) || version != "2.0" || !message.HasString ("method", method))
    {
        return (id != nullptr) ? RespondError (*id, kInvalidRequest, "The message is not a JSON-RPC 2.0 request.") : std::string();
    }

    //  A notification: notifications/initialized and the others have no
    //  response.
    if (id == nullptr)
    {
        return std::string();
    }

    if (!message.HasObject ("params", params))
    {
        params = &noParams;
    }

    if (method == "initialize")
    {
        return Respond (*id, Initialize (*params));
    }

    if (method == "ping")
    {
        return Respond (*id, JsonValue (Members {}));
    }

    if (method == "tools/list")
    {
        return Respond (*id, JsonValue (Members { { "tools", McpTools::GetToolList() } }));
    }

    if (method == "tools/call")
    {
        result = CallTool (*params, isBadParams);

        return isBadParams ? RespondError (*id, kInvalidParams, result.GetString()) : Respond (*id, std::move (result));
    }

    return RespondError (*id, kMethodNotFound, std::format ("The server has no method {}.", method));
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpServer::Initialize
//
//  The client's protocol version when the server supports it, else the
//  server's latest, as the protocol's version negotiation has it.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue McpServer::Initialize (const JsonValue & params)
{
    std::string  version;



    if (!params.HasString ("protocolVersion", version) || !IsKnownVersion (version))
    {
        version = kProtocolVersion;
    }

    return JsonValue (Members { { "protocolVersion", JsonValue (version) },
                                { "capabilities",    JsonValue (Members { { "tools", JsonValue (Members { { "listChanged", JsonValue (false) } }) } }) },
                                { "serverInfo",      JsonValue (Members { { "name", JsonValue (std::string ("casso")) }, { "version", JsonValue (std::string (VERSION_STRING)) } }) },
                                { "instructions",    JsonValue (std::string ("Tools for Casso, an Apple II emulator, through its debugger. Call launch to start one, or "
                                                                             "list_instances and attach for one the user started. Addresses are hex.")) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpServer::IsKnownVersion
//
////////////////////////////////////////////////////////////////////////////////

bool McpServer::IsKnownVersion (const std::string & version)
{
    return version == "2025-06-18" || version == "2025-03-26" || version == "2024-11-05";
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpServer::CallTool
//
//  A tool that fails returns a result with isError set; a malformed call is
//  a JSON-RPC error instead.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue McpServer::CallTool (const JsonValue & params, bool & isBadParams)
{
    std::string        name;
    const JsonValue  * arguments = nullptr;
    JsonValue          none      = JsonValue (Members {});
    McpToolResult      result;
    Members            members;



    isBadParams = false;

    if (!params.HasString ("name", name))
    {
        isBadParams = true;
        return JsonValue (std::string ("tools/call requires the tool's name."));
    }

    if (!params.HasObject ("arguments", arguments))
    {
        arguments = &none;
    }

    if (!m_tools.TryCall (name, *arguments, result))
    {
        isBadParams = true;
        return JsonValue (std::format ("The server has no tool {}.", name));
    }

    members.emplace_back ("content", JsonValue (std::move (result.content)));

    if (result.structured.has_value())
    {
        members.emplace_back ("structuredContent", std::move (*result.structured));
    }

    members.emplace_back ("isError", JsonValue (result.isError));

    return JsonValue (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpServer::Respond
//
////////////////////////////////////////////////////////////////////////////////

std::string McpServer::Respond (const JsonValue & id, JsonValue result)
{
    return WriteCompact (JsonValue (Members { { "jsonrpc", JsonValue (std::string ("2.0")) }, { "id", id }, { "result", std::move (result) } }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpServer::RespondError
//
////////////////////////////////////////////////////////////////////////////////

std::string McpServer::RespondError (const JsonValue & id, int code, const std::string & message)
{
    return WriteCompact (JsonValue (Members { { "jsonrpc", JsonValue (std::string ("2.0")) },
                                              { "id",      id },
                                              { "error",   JsonValue (Members { { "code", JsonValue ((double) code) }, { "message", JsonValue (message) } }) } }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpServer::RespondLineTooLong
//
//  The line is never parsed, so its id is unknown and the response has a null
//  id, the same as a parse error.
//
////////////////////////////////////////////////////////////////////////////////

std::string McpServer::RespondLineTooLong()
{
    return RespondError (JsonValue (nullptr), kInvalidRequest,
                         std::format ("line too long: A message line may be at most {} bytes.", ChannelProtocol::kMaxLineBytes));
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpServer::WriteCompact
//
////////////////////////////////////////////////////////////////////////////////

std::string McpServer::WriteCompact (const JsonValue & value)
{
    JsonWriter::Options  options;
    std::string          text;
    HRESULT              hr      = S_OK;



    options.fPretty = false;
    hr              = JsonWriter::Write (value, options, text);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return text;
}

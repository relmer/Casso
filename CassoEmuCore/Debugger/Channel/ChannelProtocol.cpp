#include "Pch.h"

#include "Debugger/Channel/ChannelProtocol.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Debugger/ReplyJson.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelProtocol::TryParseRequest
//
//  One line to one request, or an error record's worth of reasons.
//
//  UNKNOWN FIELDS ARE IGNORED, which is the whole of the versioning promise:
//  a client built against a later contract may send fields this server has
//  never heard of, and the server runs the command rather than refusing it.
//
////////////////////////////////////////////////////////////////////////////////

bool ChannelProtocol::TryParseRequest (const std::string & line, ChannelRequest & request, ReplyError & error)
{
    JsonValue       root;
    JsonParseError  parseError;
    std::string     type;
    std::string     mode;
    double          budget = 0.0;
    int             id     = 0;
    HRESULT         hr     = S_OK;



    request = ChannelRequest();

    if (line.size() > kMaxLineBytes)
    {
        SetError (error, "line too long", std::format ("A request line may be at most {} bytes.", kMaxLineBytes));
        return false;
    }

    //  A TRAILING CR IS NOT STRIPPED HERE. The contract tolerates CR before
    //  LF on input, and the parser already treats it as the trailing
    //  whitespace it is. A stripping loop passed every test with or without
    //  it, which is the definition of a line that looks load-bearing and
    //  never changes an outcome.
    hr = JsonParser::Parse (line, root, parseError);

    if (FAILED (hr) || root.GetType() != JsonType::Object)
    {
        SetError (error, "malformed request", "The line is not a JSON object.");
        return false;
    }

    if (!root.HasString ("type", type))
    {
        SetError (error, "malformed request", "The record has no type.");
        return false;
    }

    //  Every request carries an id, because every reply echoes one. A record
    //  without one could be answered but never matched to what asked.
    if (!root.HasInt ("id", id))
    {
        SetError (error, "malformed request", "The record has no id.");
        return false;
    }

    request.id = id;

    if (type == "hello")
    {
        request.type = ChannelRequestType::Hello;
        root.HasString ("client", request.client);
        root.HasInt    ("protocol", request.protocol);
        return true;
    }

    if (type == "pause")
    {
        request.type = ChannelRequestType::Pause;
        return true;
    }

    if (type != "command")
    {
        SetError (error, "unknown request", std::format ("{} is not a request type.", type));
        return false;
    }

    request.type = ChannelRequestType::Command;

    if (!root.HasString ("line", request.line))
    {
        SetError (error, "malformed request", "A command needs a line.");
        return false;
    }

    if (root.HasString ("mode", mode))
    {
        CommandMode  chosen = CommandMode::AppleWin;

        if (!TryGetMode (mode, chosen))
        {
            SetError (error, "unknown mode", std::format ("{} is not a mode; the modes are applewin and monitor.", mode));
            return false;
        }

        request.mode = chosen;
    }

    if (root.HasNumber ("budget", budget))
    {
        request.budget = (uint64_t) budget;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelProtocol::WriteHello
//
//  THE SERVER ANSWERS WITH ITS OWN PROTOCOL VERSION, whatever the client
//  asked for. A client built against a later contract learns what it is
//  talking to and decides for itself whether to go on, which is more useful
//  than being refused at the door.
//
////////////////////////////////////////////////////////////////////////////////

std::string ChannelProtocol::WriteHello (const ChannelHello & hello)
{
    Members  members;



    members.emplace_back ("type",     JsonValue (std::string ("hello")));
    members.emplace_back ("id",       JsonValue ((double) hello.id));
    members.emplace_back ("protocol", JsonValue ((double) kProtocolVersion));
    members.emplace_back ("pid",      JsonValue ((double) hello.pid));
    members.emplace_back ("title",    JsonValue (hello.title));
    members.emplace_back ("machine",  JsonValue (hello.machine));
    members.emplace_back ("disks",    MakeDisks (hello.disks));
    members.emplace_back ("mode",     JsonValue (std::string (ReplyJson::GetModeName (hello.mode))));
    members.emplace_back ("state",    JsonValue (std::string (hello.isPaused ? "paused" : "running")));

    return WriteLine (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelProtocol::WriteError
//
//  For a line that never became a request. It carries no id, because the
//  reason it is being sent is usually that there was none to echo.
//
////////////////////////////////////////////////////////////////////////////////

std::string ChannelProtocol::WriteError (const std::string & label, const std::string & detail)
{
    Members  members;
    Members  inner;



    inner.emplace_back ("label",  JsonValue (label));
    inner.emplace_back ("detail", JsonValue (detail));

    members.emplace_back ("type",  JsonValue (std::string ("error")));
    members.emplace_back ("error", JsonValue (std::move (inner)));

    return WriteLine (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelProtocol::GetRequestTypeName
//
////////////////////////////////////////////////////////////////////////////////

const char * ChannelProtocol::GetRequestTypeName (ChannelRequestType type)
{
    switch (type)
    {
    case ChannelRequestType::Hello:   return "hello";
    case ChannelRequestType::Pause:   return "pause";
    default:                          return "command";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelProtocol::WriteLine
//
//  Compact, because the framing is one object per line: the writer's default
//  is pretty-printed, which would put a raw newline inside every record and
//  break every reader of the stream.
//
////////////////////////////////////////////////////////////////////////////////

std::string ChannelProtocol::WriteLine (Members && members)
{
    JsonWriter::Options  options;
    JsonValue            value (std::move (members));
    std::string          text;
    HRESULT              hr = S_OK;



    options.fPretty = false;

    hr = JsonWriter::Write (value, options, text);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelProtocol::MakeDisks
//
//  An empty drive is null rather than an empty string.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ChannelProtocol::MakeDisks (const std::vector<std::optional<std::string>> & disks)
{
    std::vector<JsonValue>  entries;



    for (const std::optional<std::string> & disk : disks)
    {
        entries.push_back (disk.has_value() ? JsonValue (*disk) : JsonValue (nullptr));
    }

    return JsonValue (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelProtocol::TryGetMode
//
////////////////////////////////////////////////////////////////////////////////

bool ChannelProtocol::TryGetMode (const std::string & name, CommandMode & mode)
{
    if (name == "applewin")
    {
        mode = CommandMode::AppleWin;
        return true;
    }

    if (name == "monitor")
    {
        mode = CommandMode::Monitor;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelProtocol::SetError
//
////////////////////////////////////////////////////////////////////////////////

void ChannelProtocol::SetError (ReplyError & error, const std::string & label, const std::string & detail)
{
    error.label  = label;
    error.detail = detail;
}

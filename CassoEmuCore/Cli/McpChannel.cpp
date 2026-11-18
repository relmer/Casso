#include "Pch.h"

#include "Cli/McpChannel.h"

#include "Cli/DebugAttachRunner.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Debugger/Channel/ChannelProtocol.h"
#include "Debugger/Channel/IInstanceDirectory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel::McpChannel
//
////////////////////////////////////////////////////////////////////////////////

McpChannel::McpChannel (std::unique_ptr<IChannelClient> client) :
    m_client (std::move (client))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel::~McpChannel
//
////////////////////////////////////////////////////////////////////////////////

McpChannel::~McpChannel() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel::Hello
//
////////////////////////////////////////////////////////////////////////////////

bool McpChannel::Hello (DWORD timeoutMs, const ChannelClient & client, std::string & record)
{
    int64_t      id    = m_nextId++;
    JsonValue    reply;
    Wait         wait  = Wait::Received;
    std::string  state;



    if (!m_client->WriteLine (ChannelProtocol::WriteHelloRequest (id, client)))
    {
        return false;
    }

    wait = ReadUntil (timeoutMs, [id] (const JsonValue & each, const std::string & type)
    {
        int  replyId = 0;

        return type == "hello" && each.HasInt ("id", replyId) && replyId == id;
    }, reply);

    if (wait != Wait::Received)
    {
        return false;
    }

    m_isPaused = reply.HasString ("state", state) && state == "paused";
    record     = JsonWriter::Write (reply);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel::Execute
//
//  The reply first; then, when the reply has `running` set, the stop whose
//  cause is this request. A run that outlasts the timeout is paused.
//
////////////////////////////////////////////////////////////////////////////////

void McpChannel::Execute (const std::string & line, const std::string & mode, uint64_t budget, bool overwrite, DWORD timeoutMs, McpChannelResult & result)
{
    int64_t    id        = m_nextId++;
    JsonValue  reply;
    JsonValue  stop;
    Wait       wait      = Wait::Received;
    bool       isRunning = false;



    result = McpChannelResult();

    if (!m_client->WriteLine (DebugAttachRunner::BuildCommand (id, line, mode, budget, overwrite)))
    {
        result.isClosed = true;
        return;
    }

    wait = ReadUntil (timeoutMs, [id] (const JsonValue & each, const std::string & type)
    {
        int  replyId = 0;

        return type == "reply" && each.HasInt ("id", replyId) && replyId == id;
    }, reply);

    if (wait != Wait::Received)
    {
        result.isClosed   = wait == Wait::Closed;
        result.isTimedOut = wait == Wait::TimedOut;
        return;
    }

    ReadReply (reply, result);

    if (!reply.HasBool ("running", isRunning) || !isRunning)
    {
        return;
    }

    wait = ReadUntil (timeoutMs, [id] (const JsonValue & each, const std::string & type)
    {
        int  cause = 0;

        return type == "stopped" && each.HasInt ("causeId", cause) && cause == id;
    }, stop);

    if (wait == Wait::Received)
    {
        result.stop = std::move (stop);
    }
    else if (wait == Wait::Closed)
    {
        result.isClosed = true;
    }
    else
    {
        PauseAfter (id, result);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel::Start
//
////////////////////////////////////////////////////////////////////////////////

void McpChannel::Start (const std::string & line, const std::string & mode, DWORD timeoutMs, McpChannelResult & result)
{
    int64_t                                          id      = m_nextId++;
    std::vector<std::pair<std::string, JsonValue>>   members;
    JsonWriter::Options                              options;
    std::string                                      request;
    JsonValue                                        reply;
    Wait                                             wait    = Wait::Received;
    HRESULT                                          hr      = S_OK;



    result = McpChannelResult();

    members.emplace_back ("type", JsonValue (std::string ("command")));
    members.emplace_back ("id",   JsonValue ((double) id));
    members.emplace_back ("line", JsonValue (line));
    members.emplace_back ("mode", JsonValue (mode));

    options.fPretty = false;
    hr              = JsonWriter::Write (JsonValue (std::move (members)), options, request);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (!m_client->WriteLine (request))
    {
        result.isClosed = true;
        return;
    }

    wait = ReadUntil (timeoutMs, [id] (const JsonValue & each, const std::string & type)
    {
        int  replyId = 0;

        return type == "reply" && each.HasInt ("id", replyId) && replyId == id;
    }, reply);

    result.isClosed   = wait == Wait::Closed;
    result.isTimedOut = wait == Wait::TimedOut;

    if (wait == Wait::Received)
    {
        ReadReply (reply, result);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel::ReadReply
//
////////////////////////////////////////////////////////////////////////////////

void McpChannel::ReadReply (const JsonValue & reply, McpChannelResult & result)
{
    const JsonValue  * text  = nullptr;
    const JsonValue  * data  = nullptr;
    const JsonValue  * error = nullptr;



    (void) reply.HasString ("status", result.status);

    if (reply.HasArray ("text", text))
    {
        for (size_t i = 0; i < text->GetArraySize(); i++)
        {
            result.text.push_back (text->GetArrayElement (i).GetString());
        }
    }

    if (reply.HasObject ("data", data))
    {
        result.data = *data;
    }

    if (reply.HasObject ("error", error))
    {
        (void) error->HasString ("label",  result.errorLabel);
        (void) error->HasString ("detail", result.errorDetail);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel::PauseAfter
//
//  A run past its timeout: paused, and its stop waited for a short while.
//
////////////////////////////////////////////////////////////////////////////////

void McpChannel::PauseAfter (int64_t causeId, McpChannelResult & result)
{
    int64_t    id   = m_nextId++;
    JsonValue  stop;
    Wait       wait = Wait::Received;



    result.isTimedOut = true;
    result.wasPaused  = m_client->WriteLine (std::format ("{{\"type\":\"pause\",\"id\":{}}}", id));

    if (!result.wasPaused)
    {
        result.isClosed = true;
        return;
    }

    wait = ReadUntil (kPauseWaitMs, [causeId] (const JsonValue & each, const std::string & type)
    {
        int  cause = 0;

        return type == "stopped" && (!each.HasInt ("causeId", cause) || cause == causeId);
    }, stop);

    if (wait == Wait::Received)
    {
        result.stop = std::move (stop);
    }

    result.isClosed = wait == Wait::Closed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel::IsClosed
//
////////////////////////////////////////////////////////////////////////////////

bool McpChannel::IsClosed() const
{
    return m_client->IsClosed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel::Drain
//
////////////////////////////////////////////////////////////////////////////////

void McpChannel::Drain()
{
    JsonValue  none;



    (void) ReadUntil (0, [] (const JsonValue &, const std::string &) { return false; }, none);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel::Note
//
//  A stop pauses the machine and a resume ends the pause, whichever client
//  or the window caused them.
//
////////////////////////////////////////////////////////////////////////////////

void McpChannel::Note (const JsonValue & record, const std::string & type)
{
    const JsonValue  * registers = nullptr;



    if (type == "stopped")
    {
        m_isPaused = true;

        if (!record.HasInt ("pc", m_pc) && record.HasObject ("registers", registers))
        {
            (void) registers->HasInt ("pc", m_pc);
        }
    }
    else if (type == "resumed")
    {
        m_isPaused = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel::ReadUntil
//
//  Records other than the one looked for, notifications among them, are read
//  past. `closing` ends the wait, as the pipe closing does.
//
////////////////////////////////////////////////////////////////////////////////

McpChannel::Wait McpChannel::ReadUntil (DWORD timeoutMs, const IsWantedFn & isWanted, JsonValue & found)
{
    ULONGLONG  deadline = GetTickCount64() + timeoutMs;



    for (;;)
    {
        ULONGLONG       now       = GetTickCount64();
        DWORD           remaining = (now >= deadline) ? 0 : (DWORD) (deadline - now);
        std::string     raw;
        JsonValue       record;
        JsonParseError  error;
        std::string     type;
        HRESULT         hr        = S_OK;



        if (!m_client->ReadLine (raw, remaining))
        {
            return m_client->IsClosed() ? Wait::Closed : Wait::TimedOut;
        }

        hr = JsonParser::Parse (raw, record, error);

        if (FAILED (hr) || !record.HasString ("type", type))
        {
            continue;
        }

        if (type == "closing")
        {
            return Wait::Closed;
        }

        Note (record, type);

        if (isWanted (record, type))
        {
            found = std::move (record);
            return Wait::Received;
        }
    }
}

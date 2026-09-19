#include "Pch.h"

#include "Cli/DebugAttachRunner.h"

#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/JsonWriter.h"
#include "Debugger/Channel/IInstanceDirectory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugAttachRunner::BuildCommand
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugAttachRunner::BuildCommand (int64_t id, const std::string & line, const std::string & mode, uint64_t budget)
{
    std::vector<std::pair<std::string, JsonValue>>  members;
    JsonWriter::Options                             options;
    std::string                                     text;
    HRESULT                                         hr = S_OK;



    members.emplace_back ("type",   JsonValue (std::string ("command")));
    members.emplace_back ("id",     JsonValue ((double) id));
    members.emplace_back ("line",   JsonValue (line));
    members.emplace_back ("mode",   JsonValue (mode));
    members.emplace_back ("budget", JsonValue ((double) budget));

    options.fPretty = false;
    hr = JsonWriter::Write (JsonValue (std::move (members)), options, text);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugAttachRunner::Run
//
//  Lines in order, each waited for: its reply, and when the reply says a run
//  is still going, the stop that ends it. Notifications that arrive in between
//  are printed where they arrive, so the output reads in the order things
//  happened to the machine.
//
////////////////////////////////////////////////////////////////////////////////

void DebugAttachRunner::Run (IChannelClient                         & client,
                             const CommandLineOptions::DebugOptions & options,
                             const std::string                      & scriptText,
                             DebugAttachResult                      & result)
{
    std::vector<std::string>  lines;
    std::istringstream        stream (scriptText);
    std::string               raw;
    int64_t                   nextId      = 1;
    DWORD                     timeoutMs   = (DWORD) std::min<uint64_t> ((uint64_t) options.timeoutSeconds * 1000, UINT32_MAX - 1);
    bool                      anyFailed   = false;
    bool                      budgetStop  = false;



    while (std::getline (stream, raw))
    {
        lines.push_back (raw);
    }

    lines.insert (lines.end(), options.commands.begin(), options.commands.end());

    result = DebugAttachResult();

    for (const std::string & each : lines)
    {
        std::string  line  = each;
        int64_t      id    = nextId++;
        JsonValue    reply;
        JsonValue    stop;
        Wait         wait      = Wait::Received;
        std::string  status;
        std::string  reason;
        bool         isRunning = false;



        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
        {
            line.pop_back();
        }

        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
        {
            line.erase (0, 1);
        }

        if (line.empty() || line.starts_with (';'))
        {
            continue;
        }

        if (!options.json)
        {
            result.output += (options.mode == "monitor" ? "*" : options.mode == "windbg" ? "0:000> " : ">") + line + "\n";
        }

        if (!client.WriteLine (BuildCommand (id, line, options.mode, options.maxCycles)))
        {
            result.diagnostics += "Error: the debug channel closed.\n";
            result.exitStatus   = kChannelClosed;
            return;
        }

        wait = ReadUntil (client, timeoutMs,
            [id] (const JsonValue & record, const std::string & type)
            {
                int  replyId = 0;

                return type == "reply" && record.HasInt ("id", replyId) && replyId == id;
            },
            options, result, reply);

        if (wait != Wait::Received)
        {
            result.diagnostics += (wait == Wait::Closed) ? "Error: the debug channel closed.\n"
                                                         : "Error: no reply arrived before the timeout.\n";
            result.exitStatus   = (wait == Wait::Closed) ? kChannelClosed : kRunUnfinished;
            return;
        }

        if (reply.HasString ("status", status) && (status == "error" || status == "unknown"))
        {
            anyFailed = true;
        }

        if (!reply.HasBool ("running", isRunning) || !isRunning)
        {
            continue;
        }

        wait = ReadUntil (client, timeoutMs,
            [id] (const JsonValue & record, const std::string & type)
            {
                int  cause = 0;

                return type == "stopped" && record.HasInt ("causeId", cause) && cause == id;
            },
            options, result, stop);

        if (wait == Wait::Closed)
        {
            result.diagnostics += "Error: the debug channel closed.\n";
            result.exitStatus   = kChannelClosed;
            return;
        }

        if (wait == Wait::TimedOut)
        {
            //  Left paused where it got to, so the machine is not running on
            //  after the script that started it has gone.
            if (client.WriteLine (std::format (R"({{"type":"pause","id":{}}})", nextId++)))
            {
                wait = ReadUntil (client, kPauseWaitMs,
                    [] (const JsonValue &, const std::string & type) { return type == "stopped"; },
                    options, result, stop);
            }

            result.diagnostics += std::format ("Error: the run did not stop within {} seconds, so the machine was paused.\n",
                                               options.timeoutSeconds);
            result.exitStatus   = (wait == Wait::Closed) ? kChannelClosed : kRunUnfinished;
            return;
        }

        budgetStop = stop.HasString ("reason", reason) && reason == "budget";
    }

    if (anyFailed)
    {
        result.exitStatus = kCommandFailed;
    }
    else if (budgetStop)
    {
        result.exitStatus = kRunUnfinished;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugAttachRunner::ReadUntil
//
//  Reads records until one is wanted, printing every record it reads --
//  including the wanted one -- so nothing that arrived is lost from the output.
//  The timeout is for the whole wait, not for each record.
//
////////////////////////////////////////////////////////////////////////////////

DebugAttachRunner::Wait DebugAttachRunner::ReadUntil (
    IChannelClient                                                         & client,
    DWORD                                                                    timeoutMs,
    const std::function<bool (const JsonValue &, const std::string &)>     & isWanted,
    const CommandLineOptions::DebugOptions                                 & options,
    DebugAttachResult                                                      & result,
    JsonValue                                                              & found)
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



        if (!client.ReadLine (raw, remaining))
        {
            return client.IsClosed() ? Wait::Closed : Wait::TimedOut;
        }

        hr = JsonParser::Parse (raw, record, error);

        if (FAILED (hr) || !record.HasString ("type", type))
        {
            continue;
        }

        PrintRecord (record, raw, options, result);

        if (type == "closing")
        {
            return Wait::Closed;
        }

        if (isWanted (record, type))
        {
            found = std::move (record);
            return Wait::Received;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugAttachRunner::PrintRecord
//
//  JSON mode passes each record through as it came. Text mode prints a
//  reply's text, and a one-line account of each notification.
//
////////////////////////////////////////////////////////////////////////////////

void DebugAttachRunner::PrintRecord (const JsonValue                        & record,
                                     const std::string                      & raw,
                                     const CommandLineOptions::DebugOptions & options,
                                     DebugAttachResult                      & result)
{
    std::string        type;
    std::string        reason;
    const JsonValue  * text = nullptr;
    int                pc   = 0;



    if (options.json)
    {
        result.output += raw + "\n";
        return;
    }

    if (!record.HasString ("type", type))
    {
        return;
    }

    if (type == "reply")
    {
        if (record.HasArray ("text", text))
        {
            for (size_t i = 0; i < text->GetArraySize(); i++)
            {
                result.output += text->GetArrayElement (i).GetString() + "\n";
            }
        }

        return;
    }

    if (type == "stopped")
    {
        if (!record.HasString ("reason", reason))
        {
            reason = "unknown";
        }

        if (!record.HasInt ("pc", pc))
        {
            pc = 0;
        }

        result.output += std::format ("Stopped: {} at ${:04X}\n", reason, pc);
        return;
    }

    result.output += "[" + type + "]\n";
}

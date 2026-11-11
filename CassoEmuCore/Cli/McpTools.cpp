#include "Pch.h"

#include "Cli/McpTools.h"

#include "Cli/CommandLineQuoting.h"
#include "Cli/ICassoLauncher.h"
#include "Cli/McpChannel.h"
#include "Core/JsonParser.h"
#include "Core/TextEncoding.h"
#include "Debugger/Channel/IInstanceDirectory.h"
#include "Debugger/Channel/InstanceLister.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kTools
//
//  Each tool's name, description, input schema and handler. A schema is the
//  JSON Schema text tools/list sends.
//
////////////////////////////////////////////////////////////////////////////////

const McpTools::Tool McpTools::s_kTools[] =
{
    { "list_instances", "List the running Casso instances whose debugger is open: process id, title, machine and disks.",
      R"({"type":"object","properties":{}})", &McpTools::ListInstances },

    { "launch", "Start Casso with its debugger open and attach to it. The other tools then act on it, and close can close it.",
      R"({"type":"object","properties":{"machine":{"type":"string","description":"Machine name, such as Apple2eEnhanced or Apple2c"},"disk1":{"type":"string","description":"Disk image for drive 1"},"disk2":{"type":"string","description":"Disk image for drive 2"},"title":{"type":"string","description":"Window title prefix"}}})", &McpTools::Launch },

    { "attach", "Choose the Casso the other tools act on, by process id.",
      R"({"type":"object","properties":{"pid":{"type":"integer"}},"required":["pid"]})", &McpTools::Attach },

    { "close", "Close a Casso this server launched.",
      R"({"type":"object","properties":{}})", &McpTools::Close },

    { "command", "Run one debugger command line, as typed in the debugger's console, and return its reply. A command that writes a host file fails when the file exists, unless overwrite is true.",
      R"({"type":"object","properties":{"line":{"type":"string"},"overwrite":{"type":"boolean","description":"Let a write replace a file that exists; false by default"},"mode":{"type":"string","enum":["applewin","monitor","gssquared","windbg","casso"],"description":"Command mode; applewin by default"}},"required":["line"]})", &McpTools::Command },

    { "run", "Run the machine under the debugger until a breakpoint, the address given, the cycle budget or the timeout. A run past the timeout is paused.",
      R"({"type":"object","properties":{"until":{"type":"string","description":"Address or symbol to run to"},"timeout_seconds":{"type":"integer","description":"30 by default, at most 600"},"max_cycles":{"type":"integer","description":"100,000,000 by default"}}})", &McpTools::Run },

    { "resume", "Let a paused machine run on, as the debugger's Go does, and return at once. It runs until a breakpoint or pause.",
      R"({"type":"object","properties":{}})", &McpTools::Resume },

    { "pause", "Pause the machine.",
      R"({"type":"object","properties":{}})", &McpTools::Pause },

    { "step", "Step into, over or out of instructions.",
      R"({"type":"object","properties":{"kind":{"type":"string","enum":["into","over","out"]},"count":{"type":"integer"}}})", &McpTools::Step },

    { "registers", "Show the CPU registers and flags.",
      R"({"type":"object","properties":{}})", &McpTools::Registers },

    { "read_memory", "Read bytes of memory.",
      R"({"type":"object","properties":{"address":{"type":"string","description":"Hex address or symbol, such as 0300 or $C000"},"length":{"type":"integer","description":"Bytes; 16 by default, at most 4096"}},"required":["address"]})", &McpTools::ReadMemory },

    { "write_memory", "Write bytes to memory.",
      R"({"type":"object","properties":{"address":{"type":"string"},"bytes":{"type":"array","items":{"type":"integer","minimum":0,"maximum":255}}},"required":["address","bytes"]})", &McpTools::WriteMemory },

    { "disassemble", "Disassemble memory, at the PC by default.",
      R"({"type":"object","properties":{"address":{"type":"string"},"length":{"type":"integer","description":"Bytes to disassemble"}}})", &McpTools::Disassemble },

    { "set_breakpoint", "Set an execution breakpoint at an address, a symbol or a source line (file:line), with an optional condition.",
      R"({"type":"object","properties":{"location":{"type":"string"},"condition":{"type":"string","description":"An expression, such as A == 41"}},"required":["location"]})", &McpTools::SetBreakpoint },

    { "breakpoint_messages", "List the last messages logging breakpoints (BPLOG) wrote at their hits, oldest first.",
      R"({"type":"object","properties":{"count":{"type":"integer","description":"Messages; 20 by default, at most 1000"}}})", &McpTools::BreakpointMessages },

    { "clear_breakpoint", "Clear a breakpoint by its id, or every breakpoint.",
      R"({"type":"object","properties":{"id":{"type":"integer"},"all":{"type":"boolean"}}})", &McpTools::ClearBreakpoint },

    { "list_breakpoints", "List the breakpoints.",
      R"({"type":"object","properties":{}})", &McpTools::ListBreakpoints },

    { "reset", "Reset the machine, as Ctrl+Reset does, or power cycle it.",
      R"({"type":"object","properties":{"kind":{"type":"string","enum":["soft","power"]}}})", &McpTools::Reset },

    { "type_text", "Type text into the machine as Edit > Paste does. A newline is Return.",
      R"({"type":"object","properties":{"text":{"type":"string"}},"required":["text"]})", &McpTools::TypeText },

    { "press_keys", "Press keys in order: a character, Return, Escape, Tab, Space, Delete, Left, Right, Up, Down, Ctrl+letter or Ctrl+Reset.",
      R"({"type":"object","properties":{"keys":{"type":"array","items":{"type":"string"}}},"required":["keys"]})", &McpTools::PressKeys },

    { "insert_disk", "Insert a disk image in a drive.",
      R"({"type":"object","properties":{"path":{"type":"string"},"drive":{"type":"integer","enum":[1,2]}},"required":["path"]})", &McpTools::InsertDisk },

    { "eject_disk", "Eject the disk in a drive.",
      R"({"type":"object","properties":{"drive":{"type":"integer","enum":[1,2]}}})", &McpTools::EjectDisk },

    { "read_screen", "Read the text screen as displayed.",
      R"({"type":"object","properties":{}})", &McpTools::ReadScreen },

    { "screenshot", "Take a screenshot of the machine's picture, as a PNG image.",
      R"({"type":"object","properties":{}})", &McpTools::Screenshot },

    { "wait", "Wait a number of seconds while the machine runs.",
      R"({"type":"object","properties":{"seconds":{"type":"number","description":"At most 120"}},"required":["seconds"]})", &McpTools::Wait },
};





////////////////////////////////////////////////////////////////////////////////
//
//  McpToolResult::AddText
//
////////////////////////////////////////////////////////////////////////////////

void McpToolResult::AddText (const std::string & text)
{
    content.push_back (JsonValue (std::vector<std::pair<std::string, JsonValue>> { { "type", JsonValue (std::string ("text")) }, { "text", JsonValue (text) } }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpToolResult::AddImage
//
////////////////////////////////////////////////////////////////////////////////

void McpToolResult::AddImage (const std::string & base64, const std::string & mimeType)
{
    content.push_back (JsonValue (std::vector<std::pair<std::string, JsonValue>> { { "type",     JsonValue (std::string ("image")) },
                                                                                   { "data",     JsonValue (base64) },
                                                                                   { "mimeType", JsonValue (mimeType) } }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpToolResult::Fail
//
////////////////////////////////////////////////////////////////////////////////

void McpToolResult::Fail (const std::string & text)
{
    AddText (text);
    isError = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::McpTools
//
////////////////////////////////////////////////////////////////////////////////

McpTools::McpTools (IInstanceDirectory & directory, ICassoLauncher & launcher, ChannelClient client, std::wstring scratchFolder) :
    m_directory     (directory),
    m_launcher      (launcher),
    m_client        (std::move (client)),
    m_scratchFolder (std::move (scratchFolder))
{
    if (m_client.name.empty())
    {
        m_client.name = kClientName;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::~McpTools
//
////////////////////////////////////////////////////////////////////////////////

McpTools::~McpTools() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::GetToolList
//
////////////////////////////////////////////////////////////////////////////////

JsonValue McpTools::GetToolList()
{
    std::vector<JsonValue>  tools;



    for (const Tool & tool : s_kTools)
    {
        JsonValue       schema;
        JsonParseError  error;
        HRESULT         hr     = JsonParser::Parse (tool.schema, schema, error);



        IGNORE_RETURN_VALUE (hr, S_OK);

        tools.push_back (JsonValue (std::vector<std::pair<std::string, JsonValue>> { { "name",        JsonValue (std::string (tool.name)) },
                                                                                     { "description", JsonValue (std::string (tool.description)) },
                                                                                     { "inputSchema", std::move (schema) } }));
    }

    return JsonValue (std::move (tools));
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::TryCall
//
////////////////////////////////////////////////////////////////////////////////

bool McpTools::TryCall (const std::string & name, const JsonValue & arguments, McpToolResult & result)
{
    result = McpToolResult();

    for (const Tool & tool : s_kTools)
    {
        if (name == tool.name)
        {
            (this->*tool.handler) (arguments, result);
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::EnsureAttached
//
//  The instance attached to while its channel is open; else the one this
//  server launched last, or the only one running.
//
////////////////////////////////////////////////////////////////////////////////

bool McpTools::EnsureAttached (McpToolResult & result)
{
    std::vector<uint32_t>  running;
    std::string            error;



    if (m_channel != nullptr && !m_channel->IsClosed())
    {
        return true;
    }

    m_channel.reset();
    running = m_directory.ListProcessIds();

    if (running.size() != 1)
    {
        result.Fail (running.empty() ? "No Casso with its debugger open is running. Call launch, or ask the user to open Casso's debugger."
                                     : "More than one Casso is running. Call list_instances, then attach to one.");
        return false;
    }

    if (!Connect (running.front(), error))
    {
        result.Fail (error);
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Connect
//
////////////////////////////////////////////////////////////////////////////////

bool McpTools::Connect (uint32_t processId, std::string & error)
{
    std::unique_ptr<IChannelClient>  client = m_directory.Connect (processId);
    std::unique_ptr<McpChannel>      channel;
    std::string                      hello;



    if (client == nullptr)
    {
        error = std::format ("Casso {} has no debug channel open to this user.", processId);
        return false;
    }

    channel = std::make_unique<McpChannel> (std::move (client));

    if (!channel->Hello (kHelloTimeoutMs, m_client, hello))
    {
        error = std::format ("Casso {} didn't answer on its debug channel.", processId);
        return false;
    }

    m_channel   = std::move (channel);
    m_processId = processId;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::RunLine
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::RunLine (const std::string & line, const std::string & mode, DWORD timeoutMs, uint64_t budget, bool overwrite, McpToolResult & result)
{
    McpChannelResult  channel;



    if (!EnsureAttached (result))
    {
        return;
    }

    m_channel->Execute (line, mode, budget, overwrite, timeoutMs, channel);
    ToResult (channel, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::RunLine
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::RunLine (const std::string & line, McpToolResult & result)
{
    RunLine (line, "applewin", kCommandTimeoutMs, kDefaultBudget, false, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::ToResult
//
//  The reply's text as text content, and its data and stop as structured
//  content. A command that failed makes the result an error.
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::ToResult (const McpChannelResult & channel, McpToolResult & result)
{
    std::string                                      text;
    std::vector<std::pair<std::string, JsonValue>>   members;
    std::string                                      reason;
    int                                              pc      = 0;



    for (const std::string & line : channel.text)
    {
        text += (text.empty() ? "" : "\n") + line;
    }

    if (channel.isClosed)
    {
        text += (text.empty() ? "" : "\n") + std::string ("The debug channel closed.");
        m_channel.reset();
    }
    else if (channel.isTimedOut)
    {
        text += (text.empty() ? "" : "\n") + std::string (channel.wasPaused ? "The run outlasted its timeout and was paused." : "No reply arrived before the timeout.");
    }
    else if (channel.status != "ok" && !channel.errorDetail.empty())
    {
        text += (text.empty() ? "" : "\n") + channel.errorLabel + ": " + channel.errorDetail;
    }

    if (channel.stop.has_value() && channel.stop->HasString ("reason", reason))
    {
        (void) channel.stop->HasInt ("pc", pc);
        text += (text.empty() ? "" : "\n") + std::format ("Stopped: {} at ${:04X}", reason, pc);
    }

    members.emplace_back ("status", JsonValue (channel.status.empty() ? std::string ("none") : channel.status));

    if (channel.data.has_value())
    {
        members.emplace_back ("data", *channel.data);
    }

    if (channel.stop.has_value())
    {
        members.emplace_back ("stop", *channel.stop);
    }

    result.AddText (text.empty() ? std::string ("Done.") : text);
    result.structured = JsonValue (std::move (members));
    result.isError    = !channel.IsOk();
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::AddMachineState
//
//  A paused machine doesn't read what is typed, so the tools that type or
//  wait add a line when it is paused.
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::AddMachineState (McpToolResult & result)
{
    if (m_channel == nullptr)
    {
        return;
    }

    m_channel->Drain();

    if (m_channel->IsPaused())
    {
        result.AddText (std::format ("The machine is paused at ${:04X}. Call resume to let it run.", m_channel->GetPc()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::GetString
//
////////////////////////////////////////////////////////////////////////////////

std::string McpTools::GetString (const JsonValue & arguments, const char * key, const std::string & fallback)
{
    std::string  value;



    return arguments.HasString (key, value) ? value : fallback;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::GetInt
//
////////////////////////////////////////////////////////////////////////////////

int McpTools::GetInt (const JsonValue & arguments, const char * key, int fallback)
{
    int  value = 0;



    return arguments.HasInt (key, value) ? value : fallback;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::ListInstances
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::ListInstances (const JsonValue &, McpToolResult & result)
{
    std::vector<ListedInstance>  instances = InstanceLister::List (m_directory);
    std::vector<JsonValue>       rows;



    for (const ListedInstance & each : instances)
    {
        std::vector<JsonValue>  disks;



        for (const std::optional<std::string> & disk : each.disks)
        {
            disks.push_back (disk.has_value() ? JsonValue (*disk) : JsonValue (nullptr));
        }

        rows.push_back (JsonValue (std::vector<std::pair<std::string, JsonValue>> { { "pid",      JsonValue ((double) each.processId) },
                                                                                    { "title",    JsonValue (each.title) },
                                                                                    { "machine",  JsonValue (each.machine) },
                                                                                    { "disks",    JsonValue (std::move (disks)) },
                                                                                    { "attached", JsonValue (each.processId == m_processId && m_channel != nullptr) } }));
    }

    result.AddText (instances.empty() ? std::string ("No Casso with its debugger open is running.") : InstanceLister::Format (instances));
    result.structured = JsonValue (std::vector<std::pair<std::string, JsonValue>> { { "instances", JsonValue (std::move (rows)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Launch
//
//  Casso's channel opens with its debugger window, a moment after the
//  process starts, so the directory is looked at until it lists the process.
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Launch (const JsonValue & arguments, McpToolResult & result)
{
    std::wstring           commandLine = L"--debugger";
    std::string            error;
    uint32_t               processId   = 0;
    ULONGLONG              waited      = 0;
    std::vector<uint32_t>  running;



    for (const auto & [key, flag] : { std::pair { "title", L"--title" }, std::pair { "machine", L"--machine" }, std::pair { "disk1", L"--disk1" }, std::pair { "disk2", L"--disk2" } })
    {
        std::string  value = GetString (arguments, key);



        if (!value.empty())
        {
            commandLine += std::format (L" {} {}", flag, CommandLineQuoting::QuoteArgument (TextEncoding::NarrowToWide (value)));
        }
    }

    processId = m_launcher.Launch (commandLine, error);

    if (processId == 0)
    {
        result.Fail (error);
        return;
    }

    m_launched.insert (processId);

    for (waited = 0; waited < kLaunchTimeoutMs; waited += kPollMs)
    {
        running = m_directory.ListProcessIds();

        if (std::ranges::find (running, processId) != running.end() && Connect (processId, error))
        {
            result.AddText (std::format ("Casso {} started with its debugger open.", processId));
            result.structured = JsonValue (std::vector<std::pair<std::string, JsonValue>> { { "pid", JsonValue ((double) processId) } });
            return;
        }

        if (!m_launcher.IsRunning (processId))
        {
            result.Fail (std::format ("Casso {} exited before its debug channel opened.", processId));
            return;
        }

        m_launcher.Wait (kPollMs);
    }

    result.Fail (std::format ("Casso {} started, but its debug channel didn't open within {} seconds.", processId, kLaunchTimeoutMs / 1000));
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Attach
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Attach (const JsonValue & arguments, McpToolResult & result)
{
    int          processId = GetInt (arguments, "pid", 0);
    std::string  error;



    if (processId <= 0)
    {
        result.Fail ("attach requires a pid.");
        return;
    }

    m_channel.reset();

    if (!Connect ((uint32_t) processId, error))
    {
        result.Fail (error);
        return;
    }

    result.AddText (std::format ("Attached to Casso {}.", processId));
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Close
//
//  EXIT, then the process waited for. A Casso the user started stays open.
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Close (const JsonValue &, McpToolResult & result)
{
    uint32_t           processId = m_processId;
    McpChannelResult   channel;
    ULONGLONG          waited    = 0;



    if (m_channel == nullptr || !m_launched.contains (processId))
    {
        result.Fail ("close only closes a Casso this server launched.");
        return;
    }

    m_channel->Execute ("EXIT", "applewin", kDefaultBudget, false, kCommandTimeoutMs, channel);
    m_channel.reset();

    for (waited = 0; waited < kCloseTimeoutMs && m_launcher.IsRunning (processId); waited += kPollMs)
    {
        m_launcher.Wait (kPollMs);
    }

    if (m_launcher.IsRunning (processId))
    {
        result.Fail (std::format ("Casso {} is open, possibly behind a dialog box that opened on closing.", processId));
        return;
    }

    m_launched.erase (processId);
    m_processId = 0;
    result.AddText (std::format ("Casso {} closed.", processId));
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Command
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Command (const JsonValue & arguments, McpToolResult & result)
{
    std::string  line      = GetString (arguments, "line");
    bool         overwrite = false;



    if (line.empty())
    {
        result.Fail ("command requires a line.");
        return;
    }

    (void) arguments.HasBool ("overwrite", overwrite);

    RunLine (line, GetString (arguments, "mode", "applewin"), (DWORD) kDefaultRunSeconds * 1000, kDefaultBudget, overwrite, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Run
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Run (const JsonValue & arguments, McpToolResult & result)
{
    std::string  until   = GetString (arguments, "until");
    int          seconds = std::clamp (GetInt (arguments, "timeout_seconds", kDefaultRunSeconds), 1, kMaxRunSeconds);
    int          budget  = GetInt (arguments, "max_cycles", 0);



    RunLine (until.empty() ? std::string ("G") : "G " + until, "applewin", (DWORD) seconds * 1000,
             (budget > 0) ? (uint64_t) budget : kDefaultBudget, false, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Resume
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Resume (const JsonValue &, McpToolResult & result)
{
    McpChannelResult  channel;



    if (!EnsureAttached (result))
    {
        return;
    }

    m_channel->Start ("G", "applewin", kCommandTimeoutMs, channel);
    ToResult (channel, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Pause
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Pause (const JsonValue &, McpToolResult & result)
{
    RunLine ("PAUSE", result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Step
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Step (const JsonValue & arguments, McpToolResult & result)
{
    std::string  kind  = GetString (arguments, "kind", "into");
    int          count = std::max (1, GetInt (arguments, "count", 1));
    std::string  verb  = (kind == "over") ? "P" : (kind == "out") ? "RTS" : (kind == "into") ? "T" : "";



    if (verb.empty())
    {
        result.Fail ("step's kind is into, over or out.");
        return;
    }

    RunLine (std::format ("{} {:X}", verb, count), "applewin", (DWORD) kDefaultRunSeconds * 1000, kDefaultBudget, false, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Registers
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Registers (const JsonValue &, McpToolResult & result)
{
    RunLine ("R", result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::BreakpointMessages
//
//  This server reads the channel only during a call, so it doesn't take the
//  log notifications a logging breakpoint sends; BPLOGS lists the session's
//  recent messages.
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::BreakpointMessages (const JsonValue & arguments, McpToolResult & result)
{
    int  count = std::clamp (GetInt (arguments, "count", 20), 1, 1000);



    RunLine (std::format ("BPLOGS {}", count), result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::ReadMemory
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::ReadMemory (const JsonValue & arguments, McpToolResult & result)
{
    std::string  address = GetString (arguments, "address");
    int          length  = std::clamp (GetInt (arguments, "length", 16), 1, kMaxReadLength);



    if (address.empty())
    {
        result.Fail ("read_memory requires an address.");
        return;
    }

    RunLine (std::format ("D {},{:X}", address, length), result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::WriteMemory
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::WriteMemory (const JsonValue & arguments, McpToolResult & result)
{
    std::string        address = GetString (arguments, "address");
    const JsonValue  * bytes   = nullptr;
    std::string        line;



    if (address.empty() || !arguments.HasArray ("bytes", bytes) || bytes->GetArraySize() == 0)
    {
        result.Fail ("write_memory requires an address and bytes.");
        return;
    }

    line = "MEB " + address;

    for (size_t i = 0; i < bytes->GetArraySize(); i++)
    {
        line += std::format (" {:02X}", bytes->GetArrayElement (i).GetInt() & 0xFF);
    }

    RunLine (line, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Disassemble
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Disassemble (const JsonValue & arguments, McpToolResult & result)
{
    std::string  address = GetString (arguments, "address");
    int          length  = GetInt (arguments, "length", 0);
    std::string  line    = "U";



    if (!address.empty())
    {
        line += " " + address + ((length > 0) ? std::format (",{:X}", std::min (length, kMaxReadLength)) : std::string());
    }

    RunLine (line, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::SetBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::SetBreakpoint (const JsonValue & arguments, McpToolResult & result)
{
    std::string  location  = GetString (arguments, "location");
    std::string  condition = GetString (arguments, "condition");



    if (location.empty())
    {
        result.Fail ("set_breakpoint requires a location.");
        return;
    }

    RunLine ("BP " + location + (condition.empty() ? std::string() : " IF " + condition), result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::ClearBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::ClearBreakpoint (const JsonValue & arguments, McpToolResult & result)
{
    bool  all = false;
    int   id  = GetInt (arguments, "id", -1);



    (void) arguments.HasBool ("all", all);

    if (!all && id < 0)
    {
        result.Fail ("clear_breakpoint requires an id, or all.");
        return;
    }

    RunLine (all ? std::string ("BPC *") : std::format ("BPC {:X}", id), result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::ListBreakpoints
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::ListBreakpoints (const JsonValue &, McpToolResult & result)
{
    RunLine ("BPL", result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Reset
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Reset (const JsonValue & arguments, McpToolResult & result)
{
    RunLine ((GetString (arguments, "kind", "soft") == "power") ? "RESET POWER" : "RESET SOFT", result);
    AddMachineState (result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::TypeText
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::TypeText (const JsonValue & arguments, McpToolResult & result)
{
    std::string  text = GetString (arguments, "text");



    if (text.empty())
    {
        result.Fail ("type_text requires text.");
        return;
    }

    RunLine ("PASTE " + EscapePaste (text), result);
    AddMachineState (result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::EscapePaste
//
////////////////////////////////////////////////////////////////////////////////

std::string McpTools::EscapePaste (const std::string & text)
{
    std::string  escaped = "\"";



    for (size_t i = 0; i < text.size(); i++)
    {
        char  ch = text[i];



        if (ch == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
        {
            continue;
        }

        switch (ch)
        {
        case '\r':
        case '\n': escaped += "\\n";  break;
        case '\t': escaped += "\\t";  break;
        case '"':  escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        default:   escaped += ch;     break;
        }
    }

    return escaped + "\"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::PressKeys
//
//  Codes run together go as one KEY line; Ctrl+Reset sends what came before
//  it, then the reset.
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::PressKeys (const JsonValue & arguments, McpToolResult & result)
{
    const JsonValue           * keys   = nullptr;
    std::vector<Byte>           codes;
    std::vector<std::string>    lines;
    std::string                 error;
    std::string                 text;
    std::string                 line;



    if (!arguments.HasArray ("keys", keys) || keys->GetArraySize() == 0)
    {
        result.Fail ("press_keys requires keys.");
        return;
    }

    for (size_t i = 0; i < keys->GetArraySize(); i++)
    {
        if (!TryAddKey (keys->GetArrayElement (i).GetString(), codes, lines, error))
        {
            result.Fail (error);
            return;
        }
    }

    if (!codes.empty())
    {
        line = "KEY";

        for (Byte code : codes)
        {
            line += std::format (" {:02X}", code);
        }

        lines.push_back (line);
    }

    for (const std::string & each : lines)
    {
        McpToolResult  step;



        RunLine (each, step);

        if (step.isError)
        {
            result = std::move (step);
            return;
        }
    }

    result.AddText (std::format ("Pressed {} key{}.", keys->GetArraySize(), (keys->GetArraySize() == 1) ? "" : "s"));
    AddMachineState (result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::TryAddKey
//
//  The codes are what the keyboard latch reads, the high bit set.
//
////////////////////////////////////////////////////////////////////////////////

bool McpTools::TryAddKey (const std::string & key, std::vector<Byte> & codes, std::vector<std::string> & lines, std::string & error)
{
    static constexpr std::pair<const char *, Byte>  kNamed[] =
    {
        { "RETURN", 0x8D }, { "ENTER", 0x8D }, { "ESCAPE", 0x9B }, { "ESC", 0x9B }, { "TAB", 0x89 }, { "SPACE", 0xA0 },
        { "DELETE", 0xFF }, { "LEFT", 0x88 }, { "BACKSPACE", 0x88 }, { "RIGHT", 0x95 }, { "UP", 0x8B }, { "DOWN", 0x8A },
    };
    std::string  upper = key;
    std::string  line;



    std::ranges::transform (upper, upper.begin(), [] (char c) { return (char) std::toupper ((unsigned char) c); });

    if (upper == "CTRL+RESET" || upper == "CONTROL+RESET")
    {
        if (!codes.empty())
        {
            line = "KEY";

            for (Byte code : codes)
            {
                line += std::format (" {:02X}", code);
            }

            lines.push_back (line);
            codes.clear();
        }

        lines.push_back ("RESET SOFT");
        return true;
    }

    for (const auto & [name, code] : kNamed)
    {
        if (upper == name)
        {
            codes.push_back (code);
            return true;
        }
    }

    if ((upper.starts_with ("CTRL+") && upper.size() == 6) || (upper.starts_with ("CONTROL+") && upper.size() == 9))
    {
        char  letter = upper.back();



        if (letter >= '@' && letter <= '_')
        {
            codes.push_back ((Byte) ((letter & 0x1F) | 0x80));
            return true;
        }
    }

    if (key.size() == 1 && key[0] >= ' ' && key[0] <= '~')
    {
        codes.push_back ((Byte) (key[0] | 0x80));
        return true;
    }

    error = std::format ("press_keys has no key \"{}\".", key);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::InsertDisk
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::InsertDisk (const JsonValue & arguments, McpToolResult & result)
{
    std::string  path  = GetString (arguments, "path");
    int          drive = GetInt (arguments, "drive", 1);



    if (path.empty() || (drive != 1 && drive != 2))
    {
        result.Fail ("insert_disk requires a path, and drive 1 or 2.");
        return;
    }

    RunLine (std::format ("DISK INSERT D{} \"{}\"", drive, path), result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::EjectDisk
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::EjectDisk (const JsonValue & arguments, McpToolResult & result)
{
    int  drive = GetInt (arguments, "drive", 1);



    if (drive != 1 && drive != 2)
    {
        result.Fail ("eject_disk takes drive 1 or 2.");
        return;
    }

    RunLine (std::format ("DISK EJECT D{}", drive), result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::ReadScreen
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::ReadScreen (const JsonValue &, McpToolResult & result)
{
    RunLine ("SCREEN", result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Screenshot
//
//  SCREENSHOT writes a file in the scratch folder, or with none the temporary
//  folder, which this process shares with Casso; its bytes come back as the
//  image, and the file is deleted. The scratch folder is one of the client's
//  allowed folders, so Casso's file access policy lets the write through.
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Screenshot (const JsonValue &, McpToolResult & result)
{
    wchar_t              folder[MAX_PATH] = {};
    std::wstring         base;
    std::wstring         path;
    std::ifstream        file;
    std::vector<Byte>    png;
    McpToolResult        shot;



    GetTempPathW (MAX_PATH, folder);

    base = m_scratchFolder.empty() ? std::wstring (folder) : m_scratchFolder + L"\\";
    path = std::format (L"{}casso-mcp-{}-{}.png", base, GetCurrentProcessId(), GetTickCount64());

    RunLine ("SCREENSHOT \"" + TextEncoding::WideToNarrow (path) + "\"", shot);

    if (shot.isError)
    {
        result = std::move (shot);
        return;
    }

    file.open (path, std::ios::binary);
    png.assign (std::istreambuf_iterator<char> (file), std::istreambuf_iterator<char>());
    file.close();
    DeleteFileW (path.c_str());

    if (png.empty())
    {
        result.Fail ("The screenshot file could not be read.");
        return;
    }

    result.AddImage (EncodeBase64 (png), "image/png");
    result.structured = shot.structured;
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::Wait
//
////////////////////////////////////////////////////////////////////////////////

void McpTools::Wait (const JsonValue & arguments, McpToolResult & result)
{
    double  seconds = 0.0;



    if (!arguments.HasNumber ("seconds", seconds) || seconds < 0.0)
    {
        result.Fail ("wait requires a number of seconds.");
        return;
    }

    seconds = std::min (seconds, (double) kMaxWaitSeconds);
    m_launcher.Wait ((DWORD) (seconds * 1000.0));
    result.AddText (std::format ("Waited {} seconds.", seconds));
    AddMachineState (result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools::EncodeBase64
//
////////////////////////////////////////////////////////////////////////////////

std::string McpTools::EncodeBase64 (const std::vector<Byte> & bytes)
{
    static constexpr char  kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string            text;
    size_t                 i           = 0;



    text.reserve ((bytes.size() + 2) / 3 * 4);

    for (i = 0; i + 2 < bytes.size(); i += 3)
    {
        uint32_t  group = ((uint32_t) bytes[i] << 16) | ((uint32_t) bytes[i + 1] << 8) | bytes[i + 2];



        text += kAlphabet[(group >> 18) & 0x3F];
        text += kAlphabet[(group >> 12) & 0x3F];
        text += kAlphabet[(group >>  6) & 0x3F];
        text += kAlphabet[group & 0x3F];
    }

    if (i < bytes.size())
    {
        uint32_t  group = (uint32_t) bytes[i] << 16;



        group |= (i + 1 < bytes.size()) ? ((uint32_t) bytes[i + 1] << 8) : 0;

        text += kAlphabet[(group >> 18) & 0x3F];
        text += kAlphabet[(group >> 12) & 0x3F];
        text += (i + 1 < bytes.size()) ? kAlphabet[(group >> 6) & 0x3F] : '=';
        text += '=';
    }

    return text;
}

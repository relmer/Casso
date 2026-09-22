#include "Pch.h"

#include "Debugger/Handlers/ConfigHandlers.h"

#include "Version.h"
#include "Config/IFileSystem.h"
#include "Core/TextEncoding.h"
#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/WatchHandlers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool ConfigHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::PrintDirectory:       PrintDirectory  (session, reply);          return true;
    case DebugVerb::ChangeDirectory:      ChangeDirectory (session, command, reply); return true;
    case DebugVerb::RunScript:
    case DebugVerb::LoadConfig:           RunFile         (session, command.text, reply);  return true;
    case DebugVerb::RunStartup:           RunFile         (session, kStartupScript, reply); return true;
    case DebugVerb::SaveConfig:           SaveAll         (session, command, reply); return true;
    case DebugVerb::ConfigureDisassembly: Disassembly     (command, reply);          return true;
    case DebugVerb::DiskCommand:          Disk            (session, command, reply); return true;
    case DebugVerb::Log:                  Log             (session, command, reply); return true;
    case DebugVerb::Echo:                 Echo            (command, reply);          return true;
    case DebugVerb::Print:                Print           (session, command, reply); return true;
    case DebugVerb::PrintFormatted:       PrintFormatted  (session, command, reply); return true;
    case DebugVerb::Calculate:            reply.data = CalcData { command.a1 };      return true;
    case DebugVerb::Help:                 Help            (session, command, reply); return true;
    case DebugVerb::ShowVersion:          reply.data = MessageData { { "Casso " VERSION_STRING } }; return true;
    case DebugVerb::ShowOutputFormat:
    case DebugVerb::SetOutputFormat:      Output          (session, command, reply); return true;

    case DebugVerb::ShowMessageOfTheDay:
        reply.data = MessageData { { "Casso debugger: HELP lists the commands, MODE MONITOR switches to Apple II Monitor syntax." } };
        return true;

    //  Device panels are the window's; the window runs PANEL itself, so any
    //  PANEL that reaches here came from batch or the pipe.
    case DebugVerb::ListPanels:
    case DebugVerb::OpenPanel:
    case DebugVerb::ClosePanel:
        reply.SetError (CommandStatus::NotAvailable, "command not available", "PANEL needs the debugger window.");
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::RunScript
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::RunScript (DebugSession & session, const std::string & content, MessageData & output)
{
    size_t  start = 0;



    while (start <= content.size())
    {
        size_t       end  = content.find ('\n', start);
        std::string  line = content.substr (start, end == std::string::npos ? std::string::npos : end - start);
        size_t       firstNonBlank = line.find_first_not_of (" \t\r");
        Reply        reply;



        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (end == std::string::npos && line.empty())
        {
            break;
        }

        if (firstNonBlank == std::string::npos || line[firstNonBlank] != ';')
        {
            reply = session.ExecuteLine (line);
            session.FormatReply (reply);
            output.lines.insert (output.lines.end(), reply.text.begin(), reply.text.end());
        }

        if (end == std::string::npos)
        {
            break;
        }

        start = end + 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::PrintDirectory
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::PrintDirectory (DebugSession & session, Reply & reply)
{
    const std::wstring & directory = session.GetCurrentDirectory();



    reply.data = MessageData { { TextEncoding::WideToNarrow (directory) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::ChangeDirectory
//
//  A relative path is taken from the current directory.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::ChangeDirectory (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    if (command.text.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "CD takes a directory.");
        return;
    }

    session.SetCurrentDirectory (session.ResolvePath (command.text));
    PrintDirectory (session, reply);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::RunFile
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::RunFile (DebugSession & session, const std::string & name, Reply & reply)
{
    IFileSystem  * files = session.GetFileSystem();
    std::string    content;
    MessageData    output;
    HRESULT        hr    = S_OK;



    if (name.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "Give the script file to run.");
        return;
    }

    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    hr = files->ReadAllText (session.ResolvePath (name), content);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not found", std::format ("{} could not be read.", name));
        return;
    }

    RunScript (session, content, output);
    reply.data = output;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::SaveAll
//
//  The four scripts, breakpoints first, as one file LOAD replays.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::SaveAll (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IFileSystem  * files  = session.GetFileSystem();
    std::string    script;
    HRESULT        hr     = S_OK;



    if (command.text.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "SAVE takes a file name.");
        return;
    }

    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    script  = BreakpointHandlers::MakeScript (session);
    script += WatchHandlers::MakeScript (session, WatchListKind::Watch);
    script += WatchHandlers::MakeScript (session, WatchListKind::ZeroPage);
    script += WatchHandlers::MakeScript (session, WatchListKind::Bookmark);

    hr = files->WriteAllText (session.ResolvePath (command.text), script);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", command.text));
        return;
    }

    reply.data = MessageData { { std::format ("Saved the breakpoints, watches, zero-page pointers and bookmarks to {}.", command.text) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::Disassembly
//
//  DISASM lists the settings, DISASM name shows one, DISASM name 0|1 sets
//  it. The disassembly views read them.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::Disassembly (const DebugCommand & command, Reply & reply)
{
    std::istringstream  stream (ToUpper (command.text));
    std::string         name;
    std::string         value;
    MessageData         message;



    stream >> name >> value;

    if (!name.empty() && !m_disassembly.contains (name))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        "DISASM takes BRANCH, CLICK, COLON, FENCE, OPCODE, POINTER, SPACES or TARGET, then 0 or 1.");
        return;
    }

    if (!value.empty() && value != "0" && value != "1")
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "A DISASM setting is 0 or 1.");
        return;
    }

    if (!value.empty())
    {
        m_disassembly[name] = value == "1";
    }

    for (const auto & [setting, isOn] : m_disassembly)
    {
        if (name.empty() || setting == name)
        {
            message.lines.push_back (std::format ("{} = {}", setting, isOn ? 1 : 0));
        }
    }

    reply.data = message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::Disk
//
//  DISK and DISK INFO list the drives; DISK SLOT reports the slot. EJECT,
//  INSERT and PROTECT change the machine and need the emulator's disk
//  manager, which this session does not have.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::Disk (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::istringstream  stream (ToUpper (command.text));
    std::string         verb;
    DebugMachineInfo    info = session.GetTarget().GetMachineInfo();
    MessageData         message;



    stream >> verb;

    if (verb.empty() || verb == "INFO")
    {
        for (size_t drive = 0; drive < info.disks.size(); ++drive)
        {
            message.lines.push_back (std::format ("Slot {}, drive {}: {}", kDiskSlot, drive + 1, info.disks[drive].empty() ? "empty" : info.disks[drive]));
        }

        reply.data = message;
    }
    else if (verb == "SLOT")
    {
        reply.data = MessageData { { std::format ("Disk slot: {}", kDiskSlot) } };
    }
    else if (verb == "EJECT" || verb == "INSERT" || verb == "PROTECT")
    {
        reply.SetError (CommandStatus::NotAvailable, "command not available",
                        std::format ("DISK {} needs the emulator's disk manager, which this session does not have.", verb));
    }
    else
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "DISK takes INFO, SLOT, EJECT, INSERT or PROTECT.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::Log
//
//  AppleWin's levels map onto three: NONE, OFF and ERROR print stops only;
//  WARN, INFO, DEFAULT and ON add the other notifications; ALL adds every
//  reply's text.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::Log (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    static constexpr const char * kNames[] = { "ERROR", "INFO", "ALL" };
    std::string                   level    = ToUpper (command.text);



    if      (level == "NONE" || level == "OFF" || level == "ERROR")                       { session.SetLogLevel (LogLevel::Error); }
    else if (level == "WARN" || level == "INFO" || level == "DEFAULT" || level == "ON")   { session.SetLogLevel (LogLevel::Info); }
    else if (level == "ALL")                                                              { session.SetLogLevel (LogLevel::All); }
    else if (!level.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "LOG takes NONE, ERROR, WARN, INFO, DEFAULT, ALL, OFF or ON.");
        return;
    }

    reply.data = MessageData { { std::string ("Log: ") + kNames[(int) session.GetLogLevel()] } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::Echo
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::Echo (const DebugCommand & command, Reply & reply)
{
    std::string  text;



    if (!TryUnquote (command.text, text))
    {
        text = command.text;
    }

    reply.data = MessageData { { text } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::Print
//
//  PRINT item[,item]: a quoted string as itself, an expression as four hex
//  digits, on one line.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::Print (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::vector<std::string>  items;
    std::string               line;
    std::string               text;
    std::string               error;
    Word                      value = 0;



    SplitItems (command.text, items);

    for (const std::string & item : items)
    {
        if (TryUnquote (item, text))
        {
            line += text;
        }
        else if (TryEvaluate (session, item, value, error))
        {
            line += std::format ("{:04X}", value);
        }
        else
        {
            reply.SetError (CommandStatus::Error, "invalid arguments", error);
            return;
        }
    }

    reply.data = MessageData { { line } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::PrintFormatted
//
//  PRINTF "format"[,expr]: %x and %X give four hex digits, %d decimal, %z
//  eight binary digits, %c the character, %% a percent sign, and \n ends a
//  line.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::PrintFormatted (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::vector<std::string>  items;
    std::string               format;
    std::string               line;
    std::string               error;
    MessageData               message;
    size_t                    next  = 1;
    Word                      value = 0;



    SplitItems (command.text, items);

    if (items.empty() || !TryUnquote (items[0], format))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "PRINTF takes a quoted format, then expressions.");
        return;
    }

    for (size_t i = 0; i < format.size(); ++i)
    {
        char  ch   = format[i];
        char  spec = (i + 1 < format.size()) ? format[i + 1] : '\0';



        if (ch == '\\' && spec == 'n')
        {
            message.lines.push_back (line);
            line.clear();
            ++i;
            continue;
        }

        if (ch != '%')
        {
            line += ch;
            continue;
        }

        ++i;

        if (spec == '%')
        {
            line += '%';
            continue;
        }

        if (next >= items.size() || !TryEvaluate (session, items[next], value, error))
        {
            reply.SetError (CommandStatus::Error, "invalid arguments",
                            error.empty() ? "PRINTF has more conversions than expressions." : error);
            return;
        }

        ++next;

        switch (toupper ((unsigned char) spec))
        {
        case 'X': line += std::format ("{:04X}", value);        break;
        case 'D': line += std::format ("{}", value);            break;
        case 'Z': line += std::format ("{:08b}", (Byte) value); break;
        case 'C': line += (char) value;                         break;
        default:
            reply.SetError (CommandStatus::Error, "invalid arguments", "PRINTF conversions are %x, %d, %z, %c and %%.");
            return;
        }
    }

    message.lines.push_back (line);
    reply.data = message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::Help
//
//  HELP alone lists the names by family; HELP name describes one.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::Help (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    std::string                     name    = ToUpper (command.text);
    CommandMode                     mode    = session.GetMode();
    const AppleWinCommand         * entry   = nullptr;
    const CommandModeHelp::Entry  * word    = CommandModeHelp::Find (mode, command.text);
    std::map<int, std::string>      families;
    MessageData                     message;
    std::string                     text;
    size_t                          width   = 0;



    if (name.empty())
    {
        //  Another mode's help leads with its own commands, as its user types
        //  them, then the engine commands the mode reaches through its marker.
        for (const CommandModeHelp::Entry & form : CommandModeHelp::GetEntries (mode))
        {
            width = (std::max) (width, strlen (form.syntax));
        }

        if (width > 0)
        {
            message.lines.push_back (std::string (CommandModeHelp::GetTitle (mode)) + " commands:");

            for (const CommandModeHelp::Entry & form : CommandModeHelp::GetEntries (mode))
            {
                message.lines.push_back (std::format ("  {:<{}}  {}", form.syntax, width, form.description));
            }

            message.lines.push_back ("");
            message.lines.push_back (std::format ("Casso commands, {}:", CommandModeHelp::GetEngineRoute (mode)));
        }

        for (const AppleWinCommand & candidate : AppleWinCommandTable::GetAll())
        {
            if (candidate.availability == CommandAvailability::Headless)
            {
                families[(int) candidate.family] += std::string (families[(int) candidate.family].empty() ? "" : " ") + candidate.name;
            }
        }

        for (const auto & [family, names] : families)
        {
            message.lines.push_back (std::string (width > 0 ? "  " : "") + GetFamilyName (family) + ": " + names);
        }

        reply.data = message;
        return;
    }

    if (word != nullptr)
    {
        reply.data = MessageData { { std::format ("{}: {}", word->syntax, word->description) } };
        return;
    }

    entry = AppleWinCommandTable::Find (name);

    if (entry == nullptr)
    {
        reply.SetError (CommandStatus::Unknown, "unknown command", std::format ("{} is not a command.", name));
        return;
    }

    text = std::format ("{} ({})", entry->name, GetFamilyName ((int) entry->family));

    if      (entry->aliasOf != nullptr)                                  { text += std::string (": alias of ") + entry->aliasOf; }
    else if (entry->availability == CommandAvailability::WindowOnly)     { text += ": needs the debugger window"; }
    else if (entry->availability == CommandAvailability::NotAvailable)   { text += std::string (": ") + entry->reason; }

    reply.data = MessageData { { text } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::SplitItems
//
//  Comma-separated, with commas inside quotes kept; each item trimmed.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::SplitItems (const std::string & text, std::vector<std::string> & items)
{
    std::string  item;
    char         quote = '\0';



    for (char ch : text)
    {
        if (quote == '\0' && (ch == '"' || ch == '\''))
        {
            quote = ch;
        }
        else if (ch == quote)
        {
            quote = '\0';
        }

        if (ch == ',' && quote == '\0')
        {
            items.push_back (item);
            item.clear();
            continue;
        }

        item += ch;
    }

    items.push_back (item);

    for (std::string & each : items)
    {
        size_t  first = each.find_first_not_of (" \t");
        size_t  last  = each.find_last_not_of  (" \t");



        each = (first == std::string::npos) ? std::string() : each.substr (first, last - first + 1);
    }

    std::erase_if (items, [] (const std::string & each) { return each.empty(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::TryUnquote
//
////////////////////////////////////////////////////////////////////////////////

bool ConfigHandlers::TryUnquote (const std::string & item, std::string & text)
{
    bool  isQuoted = item.size() >= 2 && (item.front() == '"' || item.front() == '\'') && item.back() == item.front();



    if (!isQuoted)
    {
        return false;
    }

    text = item.substr (1, item.size() - 2);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::TryEvaluate
//
////////////////////////////////////////////////////////////////////////////////

bool ConfigHandlers::TryEvaluate (DebugSession & session, const std::string & text, Word & value, std::string & error)
{
    int32_t  result = 0;
    HRESULT  hr     = DebugExpressionEvaluator::ParseAndEvaluate (text, session, result, error);



    value = (Word) result;
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string ConfigHandlers::ToUpper (const std::string & text)
{
    std::string  upper (text);



    for (char & ch : upper)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    return upper;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::GetFamilyName
//
////////////////////////////////////////////////////////////////////////////////

const char * ConfigHandlers::GetFamilyName (int family)
{
    static constexpr const char * kNames[] =
    {
        "Assembler", "Cpu", "Bookmarks", "Breakpoints", "Config", "Cycles", "Data", "Disk", "Flags", "Help", "Memory",
        "Output", "Symbols", "Watch", "ZeroPage", "Startup", "Video", "Engine", "Cursor", "Window", "MiniMemory",
        "Views", "Appearance", "Unsupported",
    };



    return kNames[family];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::Output
//
//  OUTPUT reports the format replies are written in; OUTPUT name sets it and
//  leaves the mode lines are read in as it was. MODE sets both.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::Output (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    if (command.verb == DebugVerb::SetOutputFormat)
    {
        session.SetOutputFormat (command.output);
    }

    reply.data = MessageData { { "Output: " + CommandModeNames::GetUpperName (session.GetOutputFormat()) } };
}
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
    case DebugVerb::RunScript:            RunFile         (session, command.text, std::nullopt, reply);        return true;
    case DebugVerb::LoadConfig:           RunFile         (session, command.text, CommandMode::AppleWin, reply); return true;
    case DebugVerb::RunStartup:           RunFile         (session, kStartupScript, std::nullopt, reply);      return true;
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
    case DebugVerb::ShowMessageOfTheDay:  MessageOfTheDay (session, reply);          return true;
    case DebugVerb::ShowOutputFormat:
    case DebugVerb::SetOutputFormat:      Output          (session, command, reply); return true;

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
//  Each line executed in turn, in the given mode or else the session's, and
//  its reply's text collected. A byte-order mark is not part of the first
//  line. A line whose first non-blank character is ; is a comment; a blank
//  line is skipped, as batch skips it, except in the line assembler, where it
//  ends the assembly. The end of the script ends an assembly it left open, so
//  the next line typed is a command again rather than source.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::RunScript (
    DebugSession                & session,
    const std::string           & content,
    std::optional<CommandMode>    mode,
    MessageData                 & output,
    ScriptOutcome               & outcome)
{
    static constexpr std::string_view  kByteOrderMark = "\xEF\xBB\xBF";
    size_t                             start          = content.starts_with (kByteOrderMark) ? kByteOrderMark.size() : 0;
    int                                number         = 0;
    Reply                              ending;



    while (start <= content.size())
    {
        size_t       end           = content.find ('\n', start);
        std::string  line          = content.substr (start, end == std::string::npos ? std::string::npos : end - start);
        size_t       firstNonBlank = line.find_first_not_of (" \t\r");
        bool         isBlank       = firstNonBlank == std::string::npos;
        bool         isFailure     = false;
        Reply        reply;



        ++number;

        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (end == std::string::npos && line.empty())
        {
            break;
        }

        if ((!isBlank || session.IsAssembling()) && (isBlank || line[firstNonBlank] != ';'))
        {
            reply = mode.has_value() ? session.ExecuteLine (line, *mode) : session.ExecuteLine (line);
            session.FormatReply (reply);
            output.lines.insert (output.lines.end(), reply.text.begin(), reply.text.end());

            isFailure = reply.status == CommandStatus::Error || reply.status == CommandStatus::Unknown;
        }

        if (isFailure)
        {
            outcome.firstFailedLine = (outcome.failed == 0) ? number : outcome.firstFailedLine;
            ++outcome.failed;
        }

        if (end == std::string::npos)
        {
            break;
        }

        start = end + 1;
    }

    if (session.IsAssembling())
    {
        ending = session.ExecuteLine (std::string());
        session.FormatReply (ending);
        output.lines.insert (output.lines.end(), ending.text.begin(), ending.text.end());
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
    std::wstring  directory = session.ResolvePath (command.text);



    //  Empty quotes are no directory either.
    if (directory.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "CD takes a directory.");
        return;
    }

    session.SetCurrentDirectory (directory);
    PrintDirectory (session, reply);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::RunFile
//
//  A script whose lines all ran replies with their text. One with a line that
//  failed replies with the same text and fails itself, so batch's exit status
//  and a channel client see the failure; the error says which line. A script
//  that runs scripts that run it would otherwise recurse until the stack ran
//  out, so nesting stops at kMaxScriptDepth.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::RunFile (DebugSession & session, const std::string & name, std::optional<CommandMode> mode, Reply & reply)
{
    IFileSystem    * files = session.GetFileSystem();
    std::string      content;
    MessageData      output;
    ScriptOutcome    outcome;
    HRESULT          hr    = S_OK;



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

    if (m_scriptDepth >= kMaxScriptDepth)
    {
        reply.SetError (CommandStatus::Error, "scripts nested too deep",
                        std::format ("RUN, LOAD and STARTUP nest at most {} deep.", kMaxScriptDepth));
        return;
    }

    hr = files->ReadAllText (session.ResolvePath (name), content);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not found", std::format ("{} could not be read.", name));
        return;
    }

    ++m_scriptDepth;
    RunScript (session, content, mode, output, outcome);
    --m_scriptDepth;

    if (outcome.failed == 0)
    {
        reply.data = output;
        return;
    }

    reply.text = output.lines;
    reply.SetError (CommandStatus::Error, "script failed",
                    outcome.failed == 1 ? std::format ("Line {} of {} failed.", outcome.firstFailedLine, name)
                                        : std::format ("{} lines of {} failed, the first at line {}.", outcome.failed, name, outcome.firstFailedLine));
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
//  it. They last as long as the session; no disassembly reads them yet.
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
                        std::format ("DISK {} is not available in this session.", verb));
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
//  line. A conversion letter is checked before an expression is taken for
//  it, so a bad one is reported as bad whether or not an expression is left.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::PrintFormatted (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    static constexpr std::string_view  kConversions = "XDZC";
    std::vector<std::string>           items;
    std::string                        format;
    std::string                        line;
    std::string                        error;
    MessageData                        message;
    size_t                             next  = 1;
    Word                               value = 0;



    SplitItems (command.text, items);

    if (items.empty() || !TryUnquote (items[0], format))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "PRINTF takes a quoted format, then expressions.");
        return;
    }

    for (size_t i = 0; i < format.size(); ++i)
    {
        char  ch     = format[i];
        char  spec   = (i + 1 < format.size()) ? format[i + 1] : '\0';
        char  letter = (char) toupper ((unsigned char) spec);



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

        if (spec == '\0' || kConversions.find (letter) == std::string_view::npos)
        {
            reply.SetError (CommandStatus::Error, "invalid arguments", "PRINTF conversions are %x, %d, %z, %c and %%.");
            return;
        }

        if (next >= items.size())
        {
            reply.SetError (CommandStatus::Error, "invalid arguments", "PRINTF has more conversions than expressions.");
            return;
        }

        if (!TryEvaluate (session, items[next], value, error))
        {
            reply.SetError (CommandStatus::Error, "invalid arguments", error);
            return;
        }

        ++next;

        switch (letter)
        {
        case 'X': line += std::format ("{:04X}", value);        break;
        case 'D': line += std::format ("{}", value);            break;
        case 'Z': line += std::format ("{:08b}", (Byte) value); break;
        default:  line += GetPrintable (value);                 break;
        }
    }

    message.lines.push_back (line);
    reply.data = message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::GetPrintable
//
//  %c's character: the low byte with its high bit dropped, as Apple II text
//  sets it, and a period for a control character, as a memory dump shows
//  one. The raw byte could be half of no UTF-8 character, which a console
//  and a JSON record both mangle.
//
////////////////////////////////////////////////////////////////////////////////

char ConfigHandlers::GetPrintable (Word value)
{
    static constexpr Byte  kLowBits    = 0x7F;
    static constexpr Byte  kFirstPrint = 0x20;
    static constexpr Byte  kDelete     = 0x7F;
    Byte                   ch          = (Byte) (value & kLowBits);



    return (ch < kFirstPrint || ch == kDelete) ? '.' : (char) ch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::Help
//
//  HELP alone lists what can be typed in the session's mode; HELP name
//  describes one command, or says which modes run a Casso command this one
//  cannot. Only the first word is the name; anything after it is ignored, as
//  it is after a command that takes no arguments.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::Help (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    CommandMode               mode = session.GetMode();
    std::istringstream        stream (command.text);
    std::string               word;
    std::vector<std::string>  lines;



    stream >> word;

    if (word.empty())
    {
        reply.data = MessageData { CommandModeHelp::BuildHelp (mode) };
        return;
    }

    if (!CommandModeHelp::TryDescribe (mode, word, lines))
    {
        reply.SetError (CommandStatus::Unknown, "unknown command", std::format ("{} is not a command.", ToUpper (word)));
        return;
    }

    reply.data = MessageData { lines };
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
//  A value outside 16 bits is an error, as it is for CALC, rather than the
//  low 16 bits printed as though they were the value.
//
////////////////////////////////////////////////////////////////////////////////

bool ConfigHandlers::TryEvaluate (DebugSession & session, const std::string & text, Word & value, std::string & error)
{
    static constexpr int32_t  kMaxWord = 0xFFFF;
    int32_t                   result   = 0;
    HRESULT                   hr       = DebugExpressionEvaluator::ParseAndEvaluate (text, session, result, error);



    if (FAILED (hr))
    {
        return false;
    }

    if (result < 0 || result > kMaxWord)
    {
        error = std::format ("{} is outside $0000-$FFFF.", text);
        return false;
    }

    value = (Word) result;
    return true;
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





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers::MessageOfTheDay
//
//  The help and MODE commands as the session's mode types them, so the line
//  works when it is typed back. Monitor mode is offered AppleWin's syntax,
//  and every other mode the Monitor's.
//
////////////////////////////////////////////////////////////////////////////////

void ConfigHandlers::MessageOfTheDay (DebugSession & session, Reply & reply)
{
    CommandMode  mode      = session.GetMode();
    bool         isMonitor = mode == CommandMode::Monitor;
    std::string  target    = isMonitor ? "APPLEWIN to switch to AppleWin" : "MONITOR to switch to Apple II Monitor";



    reply.data = MessageData { { std::format ("Casso debugger. Use {} to list the commands, or {}MODE {} syntax.",
                                              CommandModeHelp::GetHelpCommand (mode), CommandModeHelp::GetMarker (mode), target) } };
}
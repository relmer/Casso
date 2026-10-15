#include "Pch.h"

#include "Debugger/Handlers/LogHandlers.h"

#include "Config/IFileSystem.h"
#include "Debugger/DebugSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool LogHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::VideoLog: Execute (session, command, IoLogKind::Video, reply); return true;
    case DebugVerb::SoundLog: Execute (session, command, IoLogKind::Sound, reply); return true;
    default:                                                                     return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers::Execute
//
//  CLEAR|LIST [n]|SAVE [file]. A bare command lists. LIST lists the last n
//  entries, 100 unless given; SAVE writes every entry since the log was
//  cleared.
//
////////////////////////////////////////////////////////////////////////////////

void LogHandlers::Execute (DebugSession & session, const DebugCommand & command, IoLogKind kind, Reply & reply)
{
    IIoEventSource          * source  = session.GetIoEventSource();
    bool                      isVideo = kind == IoLogKind::Video;
    std::istringstream        stream (command.text);
    std::string               verb;
    std::string               argument;
    std::string               extra;
    std::string               rest;
    std::vector<std::string>  lines;
    const char              * title   = isVideo ? "Video log" : "Sound log";
    size_t                    count   = kDefaultListCount;
    size_t                    first   = 0;



    if (source == nullptr)
    {
        reply.SetError (CommandStatus::NotAvailable, "command not available", "The video and sound logs exist only in the debugger window and in a batch run.");
        return;
    }

    stream >> verb;
    std::getline (stream, rest);
    verb  = SymbolTable::ToUpper (verb);
    first = rest.find_first_not_of (' ');
    rest  = (first == std::string::npos) ? std::string() : rest.substr (first, rest.find_last_not_of (' ') - first + 1);

    if (verb == "SAVE")
    {
        Save (session, *source, kind, rest.empty() ? (isVideo ? kDefaultVideoFile : kDefaultSoundFile) : rest, reply);
        return;
    }

    stream.clear();
    stream.str (rest);
    stream >> argument >> extra;

    if (verb == "CLEAR" && argument.empty())
    {
        source->ClearIoLog (kind);
        reply.data = MessageData { { std::format ("{} cleared.", title) } };
    }
    else if ((verb.empty() || verb == "LIST") && extra.empty() && (argument.empty() || (argument.size() <= kMaxCountDigits && std::all_of (argument.begin(), argument.end(), ::isdigit))))
    {
        count = argument.empty() ? kDefaultListCount : (size_t) std::stoull (argument);
        List (*source, kind, count, lines, reply);
    }
    else
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", std::format ("{} takes CLEAR, LIST [n] or SAVE [file].", isVideo ? "VIDEOLOG" : "SOUNDLOG"));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers::List
//
////////////////////////////////////////////////////////////////////////////////

void LogHandlers::List (IIoEventSource & source, IoLogKind kind, size_t count, std::vector<std::string> & lines, Reply & reply)
{
    IoLogEntries  listing;
    HRESULT       hr      = S_OK;



    hr = source.GetIoEntries (kind, count, listing);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "history not rebuilt", "The entries before the event cache could not be rebuilt from history.");
        return;
    }

    FormatTitle (listing, lines);
    IoEventLog::FormatEntries (listing.entries, lines);

    reply.data = MessageData { lines };
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers::FormatTitle
//
//  How far back the entries go: every one since the log was cleared or
//  began, or the newest, and whether older history was searched.
//
////////////////////////////////////////////////////////////////////////////////

void LogHandlers::FormatTitle (const IoLogEntries & listing, std::vector<std::string> & lines)
{
    size_t       count   = std::ranges::count_if (listing.entries, [] (const DebugEventEntry<IoEvent> & entry) { return entry.kind == DebugEventEntryKind::Event; });
    std::string  entries = (count == 1) ? "1 entry" : std::format ("{} entries", count);
    std::string  last    = (count == 1) ? "The last entry" : std::format ("The last {} entries", count);



    if (count == 0 && listing.isComplete)
    {
        lines.push_back ("No entries.");
    }
    else if (listing.isComplete)
    {
        lines.push_back (std::format ("{}{}:", entries, listing.wasCleared ? " since the log was cleared" : ""));
    }
    else if (listing.isTimeLimited)
    {
        lines.push_back (std::format ("{}; older history was not searched:", last));
    }
    else
    {
        lines.push_back (std::format ("{}:", last));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers::Save
//
////////////////////////////////////////////////////////////////////////////////

void LogHandlers::Save (DebugSession & session, IIoEventSource & source, IoLogKind kind, const std::string & name, Reply & reply)
{
    IFileSystem               * files = session.GetFileSystem();
    std::vector<std::string>    lines;
    std::string                 text;
    HRESULT                     hr    = S_OK;



    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    List (source, kind, SIZE_MAX, lines, reply);

    if (reply.status != CommandStatus::Ok)
    {
        return;
    }

    for (const std::string & line : lines)
    {
        text += line + "\n";
    }

    hr = files->WriteAllText (session.ResolvePath (name), text);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", name));
        return;
    }

    reply.data = MessageData { { std::format ("Saved the log{}.", session.GetPathEcho (name, "to")) } };
}

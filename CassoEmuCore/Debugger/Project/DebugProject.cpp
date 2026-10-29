#include "Pch.h"

#include "Debugger/Project/DebugProject.h"

#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"
#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "Debugger/Handlers/WatchHandlers.h"
#include "Debugger/ReplyJson.h"





using Members = std::vector<std::pair<std::string, JsonValue>>;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::CaptureEngine
//
//  BPSAVE's script and SAVE's watch scripts, then the step filter, a
//  routine by its name where it was added by one, and the data directives.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProject::CaptureEngine (DebugSession & session, const ExecutionHandlers * execution)
{
    BreakpointListData  list;
    std::string         script;



    //  The symbols the user added first, so a breakpoint or watch set by
    //  such a symbol's name resolves.
    for (const DebugSession::AddedSymbol & added : session.GetAddedSymbols())
    {
        script += std::format ("SYM{} {} = {:04X}\n", SymbolTable::ToUpper (ReplyJson::GetSymbolTableName (added.table)), added.name, added.address);
    }

    script += BreakpointHandlers::MakeScript (session);
    script += WatchHandlers::MakeScript (session, WatchListKind::Watch);
    script += WatchHandlers::MakeScript (session, WatchListKind::ZeroPage);
    script += WatchHandlers::MakeScript (session, WatchListKind::Bookmark);

    engineScript = SplitLines (script);
    engineScript.push_back ("SKIP CLEAR");

    for (const StepFilterEntry & entry : session.GetStepFilter().GetEntries())
    {
        engineScript.push_back (entry.name.empty() ? std::format ("SKIP {:04X}.{:04X}", entry.first, entry.last) : "SKIP " + entry.name);
    }

    for (const DataBlockEntry & entry : session.GetDataBlocks().GetAll())
    {
        engineScript.push_back (MakeDataLine (entry));
    }

    //  Each symbol table on or off, the CD directory, the instruction trace,
    //  the trace file, the profiler and the stopwatch; the command mode last,
    //  since a line after MODE would be read in the new mode.
    for (int table = 0; table < SymbolTable::kTableCount; table++)
    {
        engineScript.push_back (std::format ("SYM{} {}", SymbolTable::ToUpper (ReplyJson::GetSymbolTableName ((SymbolTableId) table)),
                                             session.GetSymbols().IsEnabled ((SymbolTableId) table) ? "ON" : "OFF"));
    }

    if (!session.GetCurrentDirectory().empty())
    {
        engineScript.push_back (std::format ("CD \"{}\"", TextEncoding::WideToNarrow (session.GetCurrentDirectory())));
    }

    engineScript.push_back (session.GetTarget().IsTraceOn() ? "HISTORY ON" : "HISTORY OFF");

    if (execution != nullptr)
    {
        std::ranges::copy (execution->MakeScript(), std::back_inserter (engineScript));
    }

    engineScript.push_back (std::format ("MODE {}", SymbolTable::ToUpper (CommandModeNames::GetName (session.GetMode()))));

    cycleMarker = session.GetCycleMarker();

    {
        BreakpointListData  listed;

        BreakpointHandlers::ListAll (session, listed);
        breakpointIds.entries.clear();

        for (const BreakpointInfo & each : listed.breakpoints)
        {
            breakpointIds.entries.push_back (each.id);
        }
    }

    breakpointIds.next = session.GetNextId();

    for (const auto & [table, ids] : { std::pair<WatchTable *, Ids *> { &session.GetWatches(), &watchIds }, std::pair<WatchTable *, Ids *> { &session.GetBookmarks(), &bookmarkIds } })
    {
        ids->entries.clear();

        for (const WatchItem & item : table->GetAll())
        {
            ids->entries.push_back (item.id);
        }

        ids->next = table->GetNextId();
    }

    BreakpointHandlers::ListAll (session, list);
    breakpointNames.clear();

    for (size_t i = 0; i < list.breakpoints.size(); i++)
    {
        if (!list.breakpoints[i].name.empty())
        {
            breakpointNames[i] = list.breakpoints[i].name;
        }
    }

    symbolFiles.clear();

    for (const DebugSession::SymbolLoad & load : session.GetSymbolLoads())
    {
        symbolFiles.push_back ({ load.table, load.path, load.offset });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::ApplyEngine
//
//  A symbol file already loaded into its table is not loaded again, so a
//  debug file whose load chose this project stays as it was loaded. The
//  script's own BPC *, WC *, ZPC *, BMC * and SKIP CLEAR clear the old
//  state; the data directives are cleared here. The script's breakpoint
//  numbers, in its BPD, BPE and BPCHANGE lines and in breakpointNames, are
//  its definition lines, mapped onto the ids those lines added, so a line
//  that added none moves no later number onto another breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProject::ApplyEngine (DebugSession & session, std::vector<std::string> & failures) const
{
    std::vector<int>    created;
    auto                run   = [&] (const std::string & line)
    {
        Reply  reply = session.ExecuteLine (line, CommandMode::AppleWin);



        //  A project saved before a duplicate was an error may list the same
        //  breakpoint twice; the second is dropped quietly.
        if (reply.status != CommandStatus::Ok && reply.error.label != "breakpoint already set")
        {
            failures.push_back (std::format ("{}: {}", line, reply.error.detail.empty() ? reply.error.label : reply.error.detail));
        }
    };



    for (const SymbolFile & file : symbolFiles)
    {
        bool  isLoaded = std::ranges::any_of (session.GetSymbolLoads(), [&] (const DebugSession::SymbolLoad & load)
        {
            return load.table == file.table && load.path == file.path;
        });



        if (!isLoaded)
        {
            run (std::format ("SYM{} LOAD \"{}\",{:04X}", SymbolTable::ToUpper (ReplyJson::GetSymbolTableName (file.table)),
                              TextEncoding::WideToNarrow (file.path), file.offset));
        }
    }

    session.GetDataBlocks().Clear();

    for (const std::string & line : engineScript)
    {
        RunScriptLine (session, line, created, run);
    }

    session.SetCycleMarker (cycleMarker);

    for (const auto & [index, name] : breakpointNames)
    {
        if (index < created.size() && created[index] >= 0 && !session.GetBreakpoints().TrySetName (created[index], name))
        {
            session.GetWatchpoints().TrySetName (created[index], name);
        }
    }

    RestoreBreakpointIds (session);
    RestoreWatchIds (session.GetWatches(),   watchIds);
    RestoreWatchIds (session.GetBookmarks(), bookmarkIds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::IdsToJson
//
////////////////////////////////////////////////////////////////////////////////

JsonValue DebugProject::IdsToJson (const Ids & ids)
{
    std::vector<JsonValue>  entries;



    for (int id : ids.entries)
    {
        entries.push_back (JsonValue ((double) id));
    }

    return JsonValue (Members { { "entries", JsonValue (std::move (entries)) }, { "next", JsonValue ((double) ids.next) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::IdsFromJson
//
//  For an id that is not a whole number of zero or more the list is empty,
//  and the entries are then numbered as the script adds them.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProject::IdsFromJson (const JsonValue & object, const char * name, Ids & ids)
{
    const JsonValue  * member  = nullptr;
    const JsonValue  * entries = nullptr;



    if (!object.HasObject (name, member))
    {
        return;
    }

    (void) member->HasInt ("next", ids.next);
    ids.next = std::max (ids.next, 0);

    if (!member->HasArray ("entries", entries))
    {
        return;
    }

    for (size_t i = 0; i < entries->GetArraySize(); i++)
    {
        const JsonValue  & entry = entries->GetArrayElement (i);

        if (entry.GetType() != JsonType::Number || entry.GetNumber() < 0)
        {
            ids.entries.clear();
            return;
        }

        ids.entries.push_back ((int) entry.GetNumber());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::RunScriptLine
//
//  A definition line, found by its command word so one that does not parse
//  is counted too, takes the next slot in created: the first id it added,
//  or -1. BPD, BPE and BPCHANGE n go to the id slot n has, and to none when
//  it has -1.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProject::RunScriptLine (
    DebugSession                                     & session,
    const std::string                                & line,
    std::vector<int>                                 & created,
    const std::function<void (const std::string &)>  & run)
{
    std::istringstream        words    (line);
    std::string               name;
    std::string               number;
    std::string               flags;
    std::vector<int>          before;
    const AppleWinCommand   * command  = nullptr;
    DebugVerb                 verb     = DebugVerb::None;
    size_t                    index    = created.size();
    bool                      isNumber = false;



    words >> name >> number >> flags;

    command  = AppleWinCommandTable::Find (SymbolTable::ToUpper (name));
    verb     = (command != nullptr) ? command->verb : DebugVerb::None;
    isNumber = !number.empty() && std::ranges::all_of (number, [] (char ch) { return isdigit ((unsigned char) ch) != 0; });

    if (isNumber)
    {
        index = std::stoul (number);
    }

    if (isNumber && (verb == DebugVerb::DisableBreakpoint || verb == DebugVerb::EnableBreakpoint || verb == DebugVerb::ChangeBreakpoint))
    {
        if (index >= created.size() || created[index] < 0)
        {
            return;
        }

        run (verb == DebugVerb::ChangeBreakpoint ? std::format ("BPCHANGE {} {}", created[index], flags)
                                                 : std::format ("{} {}", (verb == DebugVerb::DisableBreakpoint) ? "BPD" : "BPE", created[index]));
        return;
    }

    if (BreakpointHandlers::IsDefinition (verb))
    {
        before = BreakpointHandlers::GetIds (session);
        run (line);
        created.push_back (BreakpointHandlers::GetFirstAdded (before, BreakpointHandlers::GetIds (session)));
        return;
    }

    run (line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::RestoreBreakpointIds
//
//  The script added each entry under a new id, in the order the project
//  saved them, so the entry at each place in BPL's list takes the id saved
//  for that place: first an id past every one in use or saved, so no two
//  ever share one, then its own. Nothing changes when the counts differ,
//  as when a line of the script failed.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProject::RestoreBreakpointIds (DebugSession & session) const
{
    BreakpointListData  list;
    int                 spare = std::max (session.GetNextId(), breakpointIds.next);
    auto                change = [&session] (int from, int to)
    {
        return session.GetBreakpoints().TryChangeId (from, to) || session.GetWatchpoints().TryChangeId (from, to);
    };



    BreakpointHandlers::ListAll (session, list);

    if (list.breakpoints.size() != breakpointIds.entries.size())
    {
        session.SetNextId (std::max (session.GetNextId(), breakpointIds.next));
        return;
    }

    for (int id : breakpointIds.entries)
    {
        spare = std::max (spare, id + 1);
    }

    for (size_t i = 0; i < list.breakpoints.size(); i++)
    {
        (void) change (list.breakpoints[i].id, spare + (int) i);
    }

    for (size_t i = 0; i < list.breakpoints.size(); i++)
    {
        (void) change (spare + (int) i, breakpointIds.entries[i]);
    }

    session.SetNextId (breakpointIds.next);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::RestoreWatchIds
//
//  As the breakpoints' ids, for a watch or bookmark list.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProject::RestoreWatchIds (WatchTable & table, const Ids & ids)
{
    std::vector<int>  current;
    int               spare = std::max (table.GetNextId(), ids.next);



    for (const WatchItem & item : table.GetAll())
    {
        current.push_back (item.id);
    }

    if (current.size() != ids.entries.size())
    {
        table.SetNextId (std::max (table.GetNextId(), ids.next));
        return;
    }

    for (int id : ids.entries)
    {
        spare = std::max (spare, id + 1);
    }

    for (size_t i = 0; i < current.size(); i++)
    {
        (void) table.TryChangeId (current[i], spare + (int) i);
    }

    for (size_t i = 0; i < current.size(); i++)
    {
        (void) table.TryChangeId (spare + (int) i, ids.entries[i]);
    }

    table.SetNextId (ids.next);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::ToJson
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugProject::ToJson (const std::wstring & directory) const
{
    std::vector<JsonValue>  files;
    std::vector<JsonValue>  script;
    std::vector<JsonValue>  named;
    std::vector<JsonValue>  startup;
    Members                 window;
    Members                 root;



    for (const SymbolFile & file : symbolFiles)
    {
        files.push_back (JsonValue (Members { { "table",  JsonValue (std::string (ReplyJson::GetSymbolTableName (file.table))) },
                                              { "path",   JsonValue (TextEncoding::WideToNarrow (MakeRelative (file.path, directory))) },
                                              { "offset", JsonValue ((double) file.offset) } }));
    }

    for (const std::string & line : engineScript)
    {
        script.push_back (JsonValue (line));
    }

    for (const auto & [index, name] : breakpointNames)
    {
        named.push_back (JsonValue (Members { { "index", JsonValue ((double) index) }, { "name", JsonValue (name) } }));
    }

    for (const StartupCommand & command : startupCommands)
    {
        startup.push_back (JsonValue (Members { { "mode", JsonValue (std::string (CommandModeNames::GetName (command.mode))) },
                                                { "text", JsonValue (command.text) } }));
    }

    root.emplace_back ("format",  JsonValue (std::string (kpszFormat)));
    root.emplace_back ("version", JsonValue ((double) kVersion));
    root.emplace_back ("scope",   JsonValue (Members { { "kind",  JsonValue (std::string (ProjectScope::GetKindName (scope.kind))) },
                                                       { "key",   JsonValue (scope.key) },
                                                       { "label", JsonValue (scope.label) } }));
    root.emplace_back ("symbolFiles",     JsonValue (std::move (files)));
    root.emplace_back ("engineScript",    JsonValue (std::move (script)));
    root.emplace_back ("names",           JsonValue (std::move (named)));
    window.emplace_back ("layout",             JsonValue (layout));
    window.emplace_back ("rangeSets",          JsonValue (rangeSets));
    window.emplace_back ("disassemblyOptions", JsonValue (disassemblyOptions));
    window.emplace_back ("openViews",          JsonValue (openViews));

    for (const auto & [name, text] : windowStrings)
    {
        window.emplace_back (name, JsonValue (text));
    }

    root.emplace_back ("window",          JsonValue (std::move (window)));
    root.emplace_back ("cycleMarker",     JsonValue ((double) cycleMarker));
    root.emplace_back ("ids",             JsonValue (Members { { "breakpoints", IdsToJson (breakpointIds) },
                                                               { "watches",     IdsToJson (watchIds) },
                                                               { "bookmarks",   IdsToJson (bookmarkIds) } }));
    root.emplace_back ("breakpointUndo",  JsonValue (breakpointUndo));
    root.emplace_back ("startupCommands", JsonValue (std::move (startup)));

    return JsonWriter::Write (JsonValue (std::move (root)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::TryParse
//
//  A member unknown to this version is passed over; a known one with
//  the wrong type makes the text no project.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugProject::TryParse (const std::string & text, const std::wstring & directory, DebugProject & project, std::string & error)
{
    JsonValue          root;
    JsonParseError     parseError;
    std::string        format;
    std::string        kind;
    const JsonValue  * scope   = nullptr;
    const JsonValue  * array   = nullptr;
    const JsonValue  * window  = nullptr;
    HRESULT            hr      = JsonParser::Parse (text, root, parseError);



    project = DebugProject();

    if (FAILED (hr) || root.GetType() != JsonType::Object || !root.HasString ("format", format) || format != kpszFormat)
    {
        error = "The file is not a Casso debug project.";
        return false;
    }

    if (root.HasObject ("scope", scope))
    {
        scope->HasString ("key",   project.scope.key);
        scope->HasString ("label", project.scope.label);

        if (!scope->HasString ("kind", kind) || !ProjectScope::TryParseKind (kind, project.scope.kind))
        {
            error = "The project's scope has a kind unknown to this version.";
            return false;
        }
    }

    if (root.HasArray ("symbolFiles", array))
    {
        for (size_t i = 0; i < array->GetArraySize(); i++)
        {
            const JsonValue  & entry = array->GetArrayElement (i);
            std::string        table;
            std::string        path;
            SymbolFile         file;



            entry.HasString ("table", table);
            entry.HasString ("path",  path);
            entry.HasInt    ("offset", file.offset);

            if (SymbolTable::TryGetTableId (table, file.table) && !path.empty())
            {
                file.path = MakeAbsolute (TextEncoding::NarrowToWide (path), directory);
                project.symbolFiles.push_back (file);
            }
        }
    }

    if (root.HasArray ("engineScript", array))
    {
        for (size_t i = 0; i < array->GetArraySize(); i++)
        {
            if (array->GetArrayElement (i).GetType() == JsonType::String)
            {
                project.engineScript.push_back (array->GetArrayElement (i).GetString());
            }
        }
    }

    if (root.HasArray ("names", array))
    {
        for (size_t i = 0; i < array->GetArraySize(); i++)
        {
            int          index = -1;
            std::string  name;



            if (array->GetArrayElement (i).HasInt ("index", index) && array->GetArrayElement (i).HasString ("name", name) && index >= 0)
            {
                project.breakpointNames[(size_t) index] = name;
            }
        }
    }

    if (root.HasObject ("window", window))
    {
        window->HasString ("layout",             project.layout);
        window->HasString ("rangeSets",          project.rangeSets);
        window->HasString ("disassemblyOptions", project.disassemblyOptions);
        window->HasString ("openViews",          project.openViews);

        //  Every other string of the window's is one of its states, by name.
        for (const auto & [name, value] : window->GetObjectEntries())
        {
            if (value.GetType() == JsonType::String && name != "layout" && name != "rangeSets" && name != "disassemblyOptions" && name != "openViews")
            {
                project.windowStrings[name] = value.GetString();
            }
        }
    }

    {
        double             marker = 0;
        const JsonValue  * ids    = nullptr;

        if (root.HasNumber ("cycleMarker", marker) && marker >= 0)
        {
            project.cycleMarker = (uint64_t) marker;
        }

        if (root.HasObject ("ids", ids))
        {
            IdsFromJson (*ids, "breakpoints", project.breakpointIds);
            IdsFromJson (*ids, "watches",     project.watchIds);
            IdsFromJson (*ids, "bookmarks",   project.bookmarkIds);
        }

        root.HasString ("breakpointUndo", project.breakpointUndo);
    }

    if (root.HasArray ("startupCommands", array))
    {
        for (size_t i = 0; i < array->GetArraySize(); i++)
        {
            std::string     mode;
            StartupCommand  command;



            array->GetArrayElement (i).HasString ("mode", mode);
            array->GetArrayElement (i).HasString ("text", command.text);

            if (!CommandModeNames::TryParse (mode, command.mode))
            {
                command.mode = CommandMode::AppleWin;
            }

            project.startupCommands.push_back (command);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::SplitLines
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebugProject::SplitLines (const std::string & script)
{
    std::vector<std::string>  lines;
    std::istringstream        in (script);
    std::string               line;



    while (std::getline (in, line))
    {
        if (!line.empty())
        {
            lines.push_back (line);
        }
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::MakeDataLine
//
//  The directive that marks the block again: DB, DW (DW2 or DW4 for two or
//  four words a line), DA, ASC or DF, by name where it has one.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugProject::MakeDataLine (const DataBlockEntry & entry)
{
    std::string  verb  = (entry.kind == DataBlockKind::Words)   ? ((entry.perLine == 2) ? "DW2" : (entry.perLine == 4) ? "DW4" : "DW")
                       : (entry.kind == DataBlockKind::Address) ? "DA"
                       : (entry.kind == DataBlockKind::Text)    ? "ASC"
                       : (entry.kind == DataBlockKind::Float)   ? "DF"
                       :                                          "DB";
    std::string  range = std::format ("{:04X}:{:04X}", entry.first, entry.last);



    return entry.name.empty() ? std::format ("{} {}", verb, range) : std::format ("{} {} = {}", verb, entry.name, range);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::MakeRelative
//
//  Relative only for a path inside the directory; one elsewhere, on another
//  drive or above it, is written whole.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebugProject::MakeRelative (const std::wstring & path, const std::wstring & directory)
{
    fs::path  relative;



    if (directory.empty())
    {
        return path;
    }

    relative = fs::path (path).lexically_relative (fs::path (directory));

    return (relative.empty() || relative.native().starts_with (L"..")) ? path : relative.native();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject::MakeAbsolute
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebugProject::MakeAbsolute (const std::wstring & path, const std::wstring & directory)
{
    fs::path  given (path);



    return (given.is_absolute() || directory.empty()) ? path : (fs::path (directory) / given).lexically_normal().native();
}

#include "Pch.h"

#include "Debugger/Project/DebugProject.h"

#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
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

void DebugProject::CaptureEngine (DebugSession & session)
{
    BreakpointListData  list;
    std::string         script;



    script  = BreakpointHandlers::MakeScript (session);
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
//  state; the data directives are cleared here.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProject::ApplyEngine (DebugSession & session, std::vector<std::string> & failures) const
{
    BreakpointListData  list;
    auto                run   = [&] (const std::string & line)
    {
        Reply  reply = session.ExecuteLine (line, CommandMode::AppleWin);



        if (reply.status != CommandStatus::Ok)
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
        run (line);
    }

    BreakpointHandlers::ListAll (session, list);

    for (const auto & [index, name] : breakpointNames)
    {
        if (index < list.breakpoints.size() && !session.GetBreakpoints().TrySetName (list.breakpoints[index].id, name))
        {
            session.GetWatchpoints().TrySetName (list.breakpoints[index].id, name);
        }
    }
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
    root.emplace_back ("window",          JsonValue (Members { { "layout",             JsonValue (layout) },
                                                               { "rangeSets",          JsonValue (rangeSets) },
                                                               { "disassemblyOptions", JsonValue (disassemblyOptions) },
                                                               { "openViews",          JsonValue (openViews) } }));
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

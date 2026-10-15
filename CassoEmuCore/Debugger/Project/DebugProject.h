#pragma once

#include "Pch.h"

#include "Debugger/DataBlockTable.h"
#include "Debugger/DebugCommand.h"
#include "Debugger/Project/ProjectScope.h"
#include "Debugger/Reply.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProject
//
//  What the debugger has for one program (FR-166), as one value. The
//  engine's part is the script the debugger's own SAVE and BPSAVE write, in
//  AppleWin mode so it reads back exactly: breakpoints, watches, zero-page
//  pointers, bookmarks, data directives and step filters; beside it each
//  breakpoint's name, by its place in the script, and the symbol and debug
//  files to load first. The window's part is its saved strings: the layout,
//  the heat map's range sets, the disassembly options and the open views.
//  The startup commands run, each in its mode, once the rest is in place.
//
//  On disk it is JSON. A symbol file's path is written relative to the
//  project's file when it lies beside or below it, so the project moves
//  with its program (FR-170).
//
////////////////////////////////////////////////////////////////////////////////

struct DebugProject
{
    struct SymbolFile
    {
        SymbolTableId  table  = SymbolTableId::User;
        std::wstring   path;
        int            offset = 0;

        bool operator== (const SymbolFile &) const = default;
    };

    struct StartupCommand
    {
        std::string  text;
        CommandMode  mode = CommandMode::AppleWin;

        bool operator== (const StartupCommand &) const = default;
    };

    ProjectScope                   scope;
    std::vector<SymbolFile>        symbolFiles;
    std::vector<std::string>       engineScript;
    std::map<size_t, std::string>  breakpointNames;   // by the breakpoint's place in BPL's order
    std::string                    layout;
    std::string                    rangeSets;
    std::string                    disassemblyOptions;
    std::string                    openViews;
    std::vector<StartupCommand>    startupCommands;

    //  Where the project is saved, and whether that is its scope's automatic
    //  file rather than one the user chose.
    std::wstring                 filePath;
    bool                         isAutomatic = true;

    //  The engine's part, read from the session.
    void  CaptureEngine (DebugSession & session);

    //  The engine's part into the session, the old state cleared first: the
    //  symbol files, then the script, then each breakpoint's name. A line that fails is
    //  a line of the result, and the rest go on.
    void  ApplyEngine   (DebugSession & session, std::vector<std::string> & failures) const;

    //  The project's file text, its paths relative to `directory`
    //  where they lie inside it.
    std::string  ToJson (const std::wstring & directory) const;

    //  A project read from its file's text, relative paths taken from
    //  `directory`; false, with the reason, for text that is not one.
    static bool  TryParse (const std::string & text, const std::wstring & directory, DebugProject & project, std::string & error);

    static constexpr const char  * kpszFormat  = "casso-debug-project";
    static constexpr int           kVersion    = 1;

private:
    static std::vector<std::string>  SplitLines   (const std::string & script);
    static std::string               MakeDataLine (const DataBlockEntry & entry);
    static std::wstring              MakeRelative (const std::wstring & path, const std::wstring & directory);
    static std::wstring              MakeAbsolute (const std::wstring & path, const std::wstring & directory);
};

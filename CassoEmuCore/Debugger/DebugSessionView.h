#pragma once

#include "Debugger/CallStack.h"
#include "Debugger/DataBlockTable.h"
#include "Debugger/LineTable.h"
#include "Debugger/ProgramCheck.h"
#include "Debugger/Reply.h"
#include "Debugger/WatchTable.h"

class IFileSystem;
struct Breakpoint;
class BreakpointTable;
class SymbolTable;
struct Watchpoint;
class WatchpointTable;





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramChecks
//
//  What a session compares memory with at each stop: the debug file's
//  entries, and for each table loaded from a symbol file without lines, its
//  labels in RAM and the bytes there at the first stop in their range. Each
//  has whether it matched at the last stop.
//
////////////////////////////////////////////////////////////////////////////////

struct ProgramChecks
{
    //  The page after the last label is in the range too: the code that
    //  follows a program's last routine.
    static constexpr Word  kRangeSlack = 0x100;

    struct Labels
    {
        std::vector<Word>                 labels;
        Word                              first   = 0;
        Word                              last    = 0;
        std::vector<ProgramCheck::Entry>  entries;
        bool                              isStale = false;

        bool  IsInRange (Word pc) const { return pc >= first && (uint32_t) pc <= (uint32_t) last + kRangeSlack; }

        bool operator== (const Labels & other) const = default;
    };

    std::vector<ProgramCheck::Entry>  debugFile;
    bool                              isDebugFileStale = false;
    std::map<SymbolTableId, Labels>   tables;

    bool operator== (const ProgramChecks & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSessionFiles
//
//  A loaded debug file, its path, key and symbol table, and the line table
//  built from it: what changes only when a debug file is loaded or cleared.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugSessionFiles
{
    DebugFile                     debugFile;
    std::wstring                  path;
    std::string                   key;
    std::optional<SymbolTableId>  table;
    LineTable                     lineTable;
    DataBlockTable                data;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSessionView
//
//  The tables and settings the debugger's panes read from a session, copied
//  from the live session on the machine's thread and installed into a second
//  session that builds the panes on another. The symbols and the debug file
//  are shared rather than copied, and are copied again only when they change.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugSessionView
{
    std::vector<Breakpoint>                   breakpoints;
    std::vector<Watchpoint>                   watchpoints;
    WatchTable                                watches;
    WatchTable                                zeroPage;
    WatchTable                                bookmarks;
    DataBlockTable                            dataBlocks;
    std::shared_ptr<const SymbolTable>        symbols;
    std::shared_ptr<const DebugSessionFiles>  files;
    uint64_t                                  filesRevision   = 0;
    std::shared_ptr<const ProgramChecks>      programChecks;
    CommandMode                               mode            = CommandMode::AppleWin;
    std::optional<Word>                       assemblyAddress;
    bool                                      isStepBySource  = false;
    CallStackMechanism                        callMechanism   = CallStackMechanism::Hybrid;
    CallRecord                                callRecord;
    std::optional<float>                      callRebuild;
    uint64_t                                  cycleMarker     = 0;
    std::vector<Word>                         searchResults;
    uint64_t                                  searchSerial    = 0;
    std::vector<CompareDifference>            compareResults;
    uint64_t                                  compareSerial   = 0;
    IFileSystem                             * fileSystem      = nullptr;
    std::wstring                              currentDirectory;
    int                                       nextId          = 0;    // the id the next breakpoint set takes
};

#pragma once

#include "Debugger/BreakpointTable.h"
#include "Debugger/CallStack.h"
#include "Debugger/DataBlockTable.h"
#include "Debugger/LineTable.h"
#include "Debugger/SymbolTable.h"
#include "Debugger/WatchTable.h"
#include "Debugger/WatchpointTable.h"

class IFileSystem;





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
    CommandMode                               mode            = CommandMode::AppleWin;
    std::optional<Word>                       assemblyAddress;
    bool                                      isStepBySource  = false;
    CallStackMechanism                        callMechanism   = CallStackMechanism::Hybrid;
    CallRecord                                callRecord;
    std::optional<float>                      callRebuild;
    IFileSystem                             * fileSystem      = nullptr;
    std::wstring                              currentDirectory;
};

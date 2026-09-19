#pragma once

#include "Debugger/BreakpointTable.h"
#include "Debugger/DebugFile.h"
#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/WatchpointTable.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers
//
//  Setting: BP, BPX, BPA, BPR, BPM, BPMR, BPMW, BPMV, BPIO, BRK, BRKOP,
//  BRKINT. BP, BPX, BPM, BPMR, BPMW and BPMV take a trailing IF expression.
//  Managing: BPC, BPD, BPE, BPL, BPEDIT, BPCHANGE. Saving: BPSAVE.
//
//  Breakpoints and watchpoints share one numbering, so every command that
//  takes an id looks in both tables.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    static void            ListAll  (DebugSession & session, BreakpointListData & list);
    static BreakpointInfo  MakeInfo (const Breakpoint & entry);
    static BreakpointInfo  MakeInfo (const Watchpoint & entry);

    // The command line that recreates an entry, and the script that
    // recreates the whole table, as BPSAVE writes them.
    static std::string     MakeDefinition (const BreakpointInfo & info);
    static std::string     MakeScript     (DebugSession & session);

private:
    static constexpr int   kOpcodeCount  = 256;
    static constexpr int   kBrkSelector  = 0;
    static constexpr int   kAllSelector  = -1;
    static constexpr int   kMaxLength    = 3;

    static void  SetAddress     (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SetSourceLine  (DebugSession & session, const DebugCommand & command, Reply & reply);
    static std::optional<int>  FindSourceFile (const DebugFile & file, const std::string & name);
    static void  SetCondition   (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SetWatchpoint  (DebugSession & session, const DebugCommand & command, WatchAccess access, Reply & reply);
    static void  SetValue       (DebugSession & session, const DebugCommand & command, Reply & reply);
    static bool  TryValidateCondition (DebugSession & session, const Expression & condition, bool hasAccess, bool hasValue, Reply & reply);
    static void  SetBoth        (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SetBrk         (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SetOpcode      (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SetInterrupt   (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Clear          (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Enable         (DebugSession & session, const DebugCommand & command, bool enabled, Reply & reply);
    static void  Edit           (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Change         (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Save           (DebugSession & session, const DebugCommand & command, Reply & reply);

    static bool  TryParseBrkArguments (const std::string & text, int & selector, std::optional<bool> & isOn, std::string & error);
    static void  AddInvalidOpcodes    (DebugSession & session, int length, BreakpointListData & added);
    static void  RemoveInvalidOpcodes (DebugSession & session, int length);
    static bool  IsInvalidOfLength    (DebugSession & session, Byte opcode, int length);
    static void  ReportBrk            (DebugSession & session, Reply & reply);
    static bool  HasInterrupt         (DebugSession & session);

    static bool  TryFindInfo    (DebugSession & session, int id, BreakpointInfo & info);
    static bool  TryMakeEntry   (const DebugCommand & definition, int id, const BreakpointInfo & old, Breakpoint & breakpoint, Watchpoint & watchpoint, bool & isWatchpoint);
    static void  SetNoSuch      (Reply & reply, int id);
    static WatchMode  GetMode   (const DebugCommand & command);
};

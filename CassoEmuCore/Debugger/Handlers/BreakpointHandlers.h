#pragma once

#include "Debugger/BreakpointTable.h"
#include "Debugger/Handlers/BreakpointGroupHandlers.h"
#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/WatchpointTable.h"

class DebugSession;
class Microcode;
struct DebugFile;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers
//
//  Setting: BP, BPX, BPA, BPR, BPM, BPMR, BPMW, BPMV, BPIO, and through
//  BrkHandlers BRK, BRKOP, BRKINT, BRKUNINIT. BP, BPX, BPM, BPMR, BPMW and BPMV
//  take a trailing IF expression. Managing: BPC, BPD, BPE, BPL, BPEDIT,
//  BPCHANGE, BPNAME, and through BreakpointGroupHandlers BPGROUP. Saving: BPSAVE,
//  whose script has no name in it, as AppleWin's SAVE has none.
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

    //  The breakpoint groups, in the order of the first breakpoint in each;
    //  the group a name matches without regard to case, empty for none; and
    //  a breakpoint or watchpoint put in a group, the name matched to one.
    static std::vector<std::string>  GetGroups (DebugSession & session)                                   { return BreakpointGroupHandlers::GetGroups (session); }
    static std::string               FindGroup (DebugSession & session, const std::string & group)         { return BreakpointGroupHandlers::FindGroup (session, group); }
    static bool                      SetGroup  (DebugSession & session, int id, const std::string & group) { return BreakpointGroupHandlers::SetGroup (session, id, group); }

    // The command line that recreates an entry, and the script that
    // recreates the whole table, as BPSAVE writes them.
    static std::string     MakeDefinition (const BreakpointInfo & info);
    static std::string     MakeScript     (DebugSession & session);
    static std::string     Describe       (const BreakpointInfo & info);

    //  Moves each breakpoint set by a symbol to where the symbols put it
    //  now, after symbols are loaded, with a line for each one moved or
    //  whose symbol is no longer defined.
    static void            FollowSymbols  (DebugSession & session, std::vector<std::string> & lines);

    // One entry of either table as it stands, kept so an undo can add it
    // back exactly as it was, with no definition built or parsed.
    struct SavedEntry
    {
        bool        isWatchpoint = false;
        Breakpoint  breakpoint;
        Watchpoint  watchpoint;
    };

    //  Whether a verb sets a breakpoint, as a saved or imported script's
    //  definition lines do; every breakpoint's id, in list order; and the
    //  first id a line added.
    static bool              IsDefinition  (DebugVerb verb);
    static std::vector<int>  GetIds        (DebugSession & session);
    static int               GetFirstAdded (const std::vector<int> & before, const std::vector<int> & after);

    static bool            TrySave        (DebugSession & session, int id, SavedEntry & saved);
    static int             Add            (DebugSession & session, const SavedEntry & saved);
    static void            Replace        (DebugSession & session, int id, const SavedEntry & saved);
    static void            SetState       (DebugSession & session, int id, const BreakpointInfo & info);
    static void            Remove         (DebugSession & session, int id);

    //  False, with the error in the reply, when an IF expression cannot be
    //  evaluated: an unknown symbol, or a read of an I/O address.
    static bool            TryValidateCondition (DebugSession & session, const Expression & condition, bool hasAccess, bool hasValue, Reply & reply);

    //  An entry of either table by id, and the reply for an id in neither.
    static bool  TryFindInfo (DebugSession & session, int id, BreakpointInfo & info);
    static void  SetNoSuch   (const DebugSession & session, Reply & reply, int id);

private:
    static void                SetAddress     (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                SetSourceLine  (DebugSession & session, const DebugCommand & command, Reply & reply);
    static std::optional<int>  FindSourceFile (const DebugFile & file, const std::string & name);
    static void                SetCondition   (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                SetWatchpoint  (DebugSession & session, const DebugCommand & command, WatchAccess access, Reply & reply);
    static void                SetValue       (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                SetBoth        (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                SetDisk        (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                Clear          (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                Enable         (DebugSession & session, const DebugCommand & command, bool enabled, Reply & reply);
    static void                Edit           (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                Change         (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                SetName        (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                SetLog         (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                ListLogs       (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void                Save           (DebugSession & session, const DebugCommand & command, Reply & reply);

    //  A source line definition's line with code and the address ranges it
    //  produced; false with the reason in the reply.
    static bool  TryResolveSourceLine (DebugSession & session, const DebugCommand & command, int & target, std::vector<std::pair<Word, Word>> & ranges, Reply & reply);

    static bool       TryMakeEntry (const DebugCommand & definition, int id, const BreakpointInfo & old, Breakpoint & breakpoint, Watchpoint & watchpoint, bool & isWatchpoint);
    static WatchMode  GetMode      (const DebugCommand & command);
};

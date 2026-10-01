#pragma once

#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Reply.h"

class  DebugSession;
struct DebuggerAction;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory
//
//  Undo and redo for the breakpoints pane (FR-120). Each pane action is one
//  step: what every breakpoint was before it and after it, found by listing
//  the table on either side of the action. Undoing a step takes each
//  breakpoint back to its before state straight from the table entry saved
//  with the step, with no command built, parsed or echoed, and redoing takes
//  it forward again. Either prints one line saying what it did.
//
//  A breakpoint added again gets a new id, so a step refers to each
//  breakpoint by a handle of its own and the history keeps the id each handle
//  has now. A change typed at the console is not a step; an inverse whose
//  breakpoint the console has removed, or replaced with another, does nothing.
//
//  CPU thread only, like the session it reads.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointHistory
{
public:
    using LineRunner   = std::function<Reply (const std::string & line)>;
    using ActionRunner = std::function<Reply (const DebuggerAction & action)>;
    using Change       = std::function<void()>;

    //  Runs `change` and records what it did to the table as one step, which
    //  clears what could be redone. A change that alters nothing is no step.
    void  Record  (DebugSession & session, const Change & change);

    //  Takes the newest step back or forward straight from the table entries
    //  saved with it, and gives the one console line that says what it
    //  restored or removed. False when there is nothing to undo or redo.
    bool  TryUndo (DebugSession & session, std::string & line);
    bool  TryRedo (DebugSession & session, std::string & line);

    bool  CanUndo () const { return !m_undo.empty(); }
    bool  CanRedo () const { return !m_redo.empty(); }

    //  The BPCHANGE action that gives breakpoint `id` the enabled state and
    //  When hit setting of `info`.
    static DebuggerAction  GetFlagsAction (int id, const BreakpointInfo & info, CommandMode mode);

private:
    //  One breakpoint's part in a step: its state before and after, either
    //  missing where the breakpoint did not exist, and the table entry on
    //  each side to add it back from.
    using Saved = BreakpointHandlers::SavedEntry;

    struct Entry
    {
        int                            handle = 0;
        std::optional<BreakpointInfo>  before;
        std::optional<BreakpointInfo>  after;
        std::optional<Saved>           savedBefore;
        std::optional<Saved>           savedAfter;
    };

    using Step = std::vector<Entry>;

    //  What applying one entry did, for the console line.
    struct Outcome
    {
        enum class Kind { None, Restored, Removed };

        Kind            kind = Kind::None;
        int             id   = 0;
        BreakpointInfo  info;
    };

    Outcome              Apply        (DebugSession & session, const Entry & entry,
                                       const std::optional<BreakpointInfo> & from, const std::optional<BreakpointInfo> & to,
                                       const std::optional<Saved> & saved);
    void                 Add          (DebugSession & session, int handle, const Saved & saved);
    int                  GetHandle    (int id);
    std::optional<int>   GetLiveId    (DebugSession & session, int handle, const BreakpointInfo & expected) const;

    static std::map<int, BreakpointInfo>  ListById    (DebugSession & session);
    static std::map<int, Saved>           SaveAll     (DebugSession & session);
    static std::string                    Describe    (const std::vector<Outcome> & outcomes);
    static bool                           IsSame      (const BreakpointInfo & left, const BreakpointInfo & right);
    static bool                           HasSameFlags (const BreakpointInfo & left, const BreakpointInfo & right);

    std::vector<Step>   m_undo;
    std::vector<Step>   m_redo;
    std::map<int, int>  m_ids;
    int                 m_nextHandle = 1;
};

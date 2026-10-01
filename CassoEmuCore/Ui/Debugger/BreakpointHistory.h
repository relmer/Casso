#pragma once

#include "Debugger/Reply.h"

class  DebugSession;
struct DebuggerAction;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHistory
//
//  Undo and redo for the breakpoints pane (FR-120). Each pane action is one
//  step: what every breakpoint was before it and after it, found by listing
//  the table on either side of the action. Undoing a step runs the commands
//  that take each breakpoint back to its before state -- a deleted one is
//  added again from its definition, then given its enabled state and When
//  hit setting -- and redoing runs the commands that take it forward again.
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

    //  Takes the newest step back or forward through `run`, with each action
    //  echoed in the session's mode. False when there is nothing to undo or
    //  redo.
    bool  TryUndo (DebugSession & session, const ActionRunner & run);
    bool  TryRedo (DebugSession & session, const ActionRunner & run);

    bool  CanUndo () const { return !m_undo.empty(); }
    bool  CanRedo () const { return !m_redo.empty(); }

    //  The BPCHANGE action that gives breakpoint `id` the enabled state and
    //  When hit setting of `info`.
    static DebuggerAction  GetFlagsAction (int id, const BreakpointInfo & info, CommandMode mode);

private:
    //  One breakpoint's part in a step: its state before and after, either
    //  missing where the breakpoint did not exist.
    struct Entry
    {
        int                            handle = 0;
        std::optional<BreakpointInfo>  before;
        std::optional<BreakpointInfo>  after;
    };

    using Step = std::vector<Entry>;

    void                 Apply        (DebugSession & session, const ActionRunner & run, const Entry & entry,
                                       const std::optional<BreakpointInfo> & from, const std::optional<BreakpointInfo> & to);
    void                 Add          (DebugSession & session, const ActionRunner & run, int handle, const BreakpointInfo & to);
    int                  GetHandle    (int id);
    std::optional<int>   GetLiveId    (DebugSession & session, int handle, const BreakpointInfo & expected) const;

    static std::map<int, BreakpointInfo>  ListById    (DebugSession & session);
    static bool                           IsSame      (const BreakpointInfo & left, const BreakpointInfo & right);
    static bool                           HasSameFlags (const BreakpointInfo & left, const BreakpointInfo & right);

    std::vector<Step>   m_undo;
    std::vector<Step>   m_redo;
    std::map<int, int>  m_ids;
    int                 m_nextHandle = 1;
};

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
//  it forward again. Either prints one line for each breakpoint it changed,
//  saying what happened to it.
//
//  A breakpoint added again takes the id it had, unless another breakpoint
//  has taken that id since, so a step refers to each breakpoint by a handle
//  of its own and the history has the id each handle has now. A change typed at the console is not a step; an inverse whose
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
    //  saved with it, and gives one console line for each breakpoint it
    //  restored or removed. False when there is nothing to undo or redo.
    bool  TryUndo (DebugSession & session, std::vector<std::string> & lines);
    bool  TryRedo (DebugSession & session, std::vector<std::string> & lines);

    bool  CanUndo () const { return !m_undo.empty(); }
    bool  CanRedo () const { return !m_redo.empty(); }

    //  The step Undo or Redo would act on, as "delete breakpoint #3" or
    //  "disable 2 breakpoints"; empty for none.
    std::wstring  GetUndoText () const { return m_undo.empty() ? std::wstring() : DescribeStep (m_undo.back()); }
    std::wstring  GetRedoText () const { return m_redo.empty() ? std::wstring() : DescribeStep (m_redo.back()); }

    //  The history as a debug project saves it: each handle's id, then the
    //  steps to undo and to redo, each breakpoint's state on either side as
    //  its definition, its flags and its name. FromText adds each definition
    //  to the session for a moment to rebuild its table entry, then removes
    //  it, so the session's next id is the same afterward. For text that
    //  does not parse, the history is empty.
    std::string  ToText   () const;
    void         FromText (DebugSession & session, const std::string & text);
    void         Clear    ();

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
    static std::vector<std::string>       Describe    (const std::vector<Outcome> & outcomes);
    static std::wstring                   DescribeStep (const Step & step);
    static bool                           IsSame      (const BreakpointInfo & left, const BreakpointInfo & right);
    static bool                           HasSameFlags (const BreakpointInfo & left, const BreakpointInfo & right);
    static std::string                    FormatSide   (const std::optional<BreakpointInfo> & info);
    static bool                           TryParseSide (DebugSession & session, const std::string & text,
                                                        std::optional<BreakpointInfo> & info, std::optional<Saved> & saved);
    static bool                           TryRebuild   (DebugSession & session, const std::string & definition, BreakpointInfo & info, Saved & saved);
    static bool                           TryParseStep (DebugSession & session, const std::string & text, Step & step);

    std::vector<Step>   m_undo;
    std::vector<Step>   m_redo;
    std::map<int, int>  m_ids;
    int                 m_nextHandle = 1;
};

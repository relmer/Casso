#pragma once

#include "Ui/Debugger/DebuggerActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory
//
//  Undo and redo for the stack pane. Each edit the pane makes is one step:
//  the stack byte's address, its value before and after, and the stop it was
//  made at, which is the PC while the machine is paused. A step is undone or
//  redone by the byte-entry command a person would type.
//
//  An edit belongs to its stop. Once the machine runs, or the PC moves on,
//  the values the steps hold no longer describe the machine, so the whole
//  history is cleared.
//
////////////////////////////////////////////////////////////////////////////////

class StackHistory
{
public:
    //  An edit the pane made at the stop whose PC is `pc`. Clears what could
    //  be redone.
    void  Record (Word address, Byte before, Byte after, Word pc);

    //  The machine as the latest snapshot shows it. A run, or a stop at
    //  another PC, clears the history.
    void  OnSnapshot (bool isPaused, Word pc);

    //  The byte-entry action that takes the newest step back or forward, or
    //  nothing when there is none.
    std::optional<DebuggerAction>  TryUndo (CommandMode mode);
    std::optional<DebuggerAction>  TryRedo (CommandMode mode);

    bool  CanUndo () const { return !m_undo.empty(); }
    bool  CanRedo () const { return !m_redo.empty(); }

    void  Clear ();

private:
    struct Step
    {
        Word  address = 0;
        Byte  before  = 0;
        Byte  after   = 0;
    };

    std::vector<Step>  m_undo;
    std::vector<Step>  m_redo;
    Word               m_pc = 0;
};

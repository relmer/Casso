#pragma once

#include "Ui/Debugger/DebuggerActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StackHistory
//
//  Undo and redo for the stack pane. Each edit the pane makes is one step:
//  the stack byte's address, its value before and after, and the stop it was
//  made at, by its PC and the CPU's cycles while the machine is paused. A
//  step is undone or redone by the byte-entry command a person would type.
//
//  An edit applies to its stop only. Once the machine runs, or stops
//  elsewhere or later, the values in the steps are out of date, so the whole
//  history is cleared.
//
////////////////////////////////////////////////////////////////////////////////

class StackHistory
{
public:
    //  The cycles of a stop not known, as after a history read from a
    //  project: the next paused snapshot at its PC sets them.
    static constexpr uint64_t  kAnyCycles = UINT64_MAX;

    //  An edit the pane made at the stop at `pc` after `cycles`. Clears what
    //  could be redone.
    void  Record (Word address, Byte before, Byte after, Word pc, uint64_t cycles = kAnyCycles);

    //  The machine in the latest snapshot. A run, or another stop, at
    //  another PC or after other cycles, clears the history.
    void  OnSnapshot (bool isPaused, Word pc, uint64_t cycles = kAnyCycles);

    //  The byte-entry action that takes the newest step back or forward, or
    //  nothing when there is none.
    std::optional<DebuggerAction>  TryUndo (CommandMode mode);
    std::optional<DebuggerAction>  TryRedo (CommandMode mode);

    bool  CanUndo () const { return !m_undo.empty(); }
    bool  CanRedo () const { return !m_redo.empty(); }

    //  The newest step Undo or Redo would act on, as "change 1 byte at
    //  $01FD", or empty when there is none.
    std::wstring  GetUndoText () const;
    std::wstring  GetRedoText () const;

    void  Clear ();

    //  The history as a debug project saves it: the stop's PC, then the
    //  steps to undo and to redo, each list's steps apart at commas, the
    //  three apart at bars. FromText replaces the history,
    //  or empties it for text it cannot read.
    std::string  ToText   () const;
    void         FromText (const std::string & text);

private:
    struct Step
    {
        Word  address = 0;
        Byte  before  = 0;
        Byte  after   = 0;
    };

    std::vector<Step>  m_undo;
    std::vector<Step>  m_redo;
    Word               m_pc     = 0;
    uint64_t           m_cycles = kAnyCycles;    // the CPU's cycles at the stop; with the PC, one stop's key

    bool  IsSameStop (uint64_t cycles) const { return m_cycles == kAnyCycles || cycles == kAnyCycles || cycles == m_cycles; }
};

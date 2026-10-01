#pragma once

#include "Ui/Debugger/DebuggerActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterHistory
//
//  Undo and redo for the registers pane. Each edit the pane makes is one
//  step: the register, its value before and after, and the stop it was made
//  at, which is the PC while the machine is paused. A step is undone or
//  redone by the R command a person would type.
//
//  An edit belongs to its stop. Once the machine runs, or the PC moves on,
//  the values the steps hold no longer describe the machine, so the whole
//  history is cleared.
//
////////////////////////////////////////////////////////////////////////////////

class RegisterHistory
{
public:
    //  An edit the pane made at the stop whose PC is `pc`. Clears what could
    //  be redone.
    void  Record (const std::string & name, Byte before, Byte after, Word pc);

    //  The machine as the latest snapshot shows it. A run, or a stop at
    //  another PC, clears the history.
    void  OnSnapshot (bool isPaused, Word pc);

    //  The R action that takes the newest step back or forward, or nothing
    //  when there is none.
    std::optional<DebuggerAction>  TryUndo (CommandMode mode);
    std::optional<DebuggerAction>  TryRedo (CommandMode mode);

    bool  CanUndo () const { return !m_undo.empty(); }
    bool  CanRedo () const { return !m_redo.empty(); }

    //  The newest step Undo or Redo would act on, as "changed register S",
    //  or empty when there is none.
    std::wstring  GetUndoText () const;
    std::wstring  GetRedoText () const;

    void  Clear ();

private:
    struct Step
    {
        std::string  name;
        Byte         before = 0;
        Byte         after  = 0;
    };

    std::vector<Step>  m_undo;
    std::vector<Step>  m_redo;
    Word               m_pc = 0;
};

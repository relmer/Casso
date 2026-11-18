#pragma once

#include "Debugger/DebugCommand.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"

class DebuggerCommands;
struct DebuggerAction;
struct DebuggerViewSnapshot;





////////////////////////////////////////////////////////////////////////////////
//
//  ICommandBarHost
//
//  What the command bar's entries act on: the latest snapshot, the machine's
//  run state and the selected code row, and the window's actions.
//
////////////////////////////////////////////////////////////////////////////////

class ICommandBarHost
{
public:
    virtual ~ICommandBarHost() = default;

    virtual const DebuggerViewSnapshot * GetCommandBarSnapshot () const = 0;
    virtual bool                         IsReplayingHistory    () const = 0;
    virtual bool                         HasSelectedCodeRow    () const = 0;
    virtual bool                         IsDarkTheme           () const = 0;
    virtual CommandMode                  GetMode               () const = 0;
    virtual void                         RunAction             (const DebuggerAction & action) = 0;
    virtual bool                         RunMappedCommand      (int commandId) = 0;
    virtual void                         ShowNextStatement     () = 0;
    virtual void                         LoadFileIntoMemory    () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarController
//
//  The debugger's run controls: the command bar's entries, which of them are
//  enabled, and what each runs. The menus, the timeline and the keys run the
//  same entries through here, so a click and its key run the same command.
//
////////////////////////////////////////////////////////////////////////////////

class CommandBarController
{
public:
    explicit CommandBarController (ICommandBarHost & host);
    ~CommandBarController();

    //  The bar's entries, with the key scheme's ids, so a click and the key
    //  beside it in the tip run the same command.
    std::vector<DxuiToolbar::Entry>  CreateEntries  ();
    void                             ApplyKeyScheme (DebuggerKeyScheme scheme);

    void  Run       (int id);
    bool  IsEnabled (int id) const;

    //  Which entries a snapshot enables: a running machine has no state to
    //  step through, and Pause is off while it is already stopped.
    static bool  IsEnabled (int id, const DebuggerViewSnapshot * snapshot, bool isReplaying, bool hasSelectedRow);

private:
    ICommandBarHost                    & m_host;
    std::unique_ptr<DebuggerCommands>    m_commands;
};

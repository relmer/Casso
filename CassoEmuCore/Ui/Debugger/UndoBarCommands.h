#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UndoBarCommands
//
//  The toolbar over the registers, stack and watch panes: Undo and Redo for
//  the pane's own edits, each tip giving the edit it would act on, as "Undo
//  changed 1 byte at $01FD".
//
//  The window owns the behavior: it hands over one dispatch, one enabled test
//  and one tip, each taking an id.
//
////////////////////////////////////////////////////////////////////////////////

class UndoBarCommands
{
public:
    static constexpr int  kUndo = 1;
    static constexpr int  kRedo = 2;

    struct Handlers
    {
        std::function<void (int id)>          dispatch;
        std::function<bool (int id)>          isEnabled;
        std::function<std::wstring (int id)>  getTip;
    };

    explicit UndoBarCommands (Handlers handlers);

    std::vector<DxuiToolbar::Entry>  BuildEntries () const;
    std::shared_ptr<DxuiCommand>     Find         (int id) const;

private:
    Handlers                                   m_handlers;
    std::vector<std::shared_ptr<DxuiCommand>>  m_commands;
};

#include "Pch.h"

#include "Ui/Debugger/UndoBarCommands.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UndoBarCommands::UndoBarCommands
//
////////////////////////////////////////////////////////////////////////////////

UndoBarCommands::UndoBarCommands (Handlers handlers)
{
    struct Row
    {
        int                id    = 0;
        const wchar_t    * label = nullptr;
        const wchar_t    * glyph = nullptr;
        const wchar_t    * tip   = nullptr;
    };

    static constexpr Row  kRows[] =
    {
        { kUndo, L"Undo", s_kpszMdl2Undo, L"Undo the last change made in this pane" },
        { kRedo, L"Redo", s_kpszMdl2Redo, L"Redo the change just undone"            },
    };



    m_handlers = std::move (handlers);

    for (const Row & row : kRows)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        int                           id      = row.id;

        command->id    = row.id;
        command->label = row.label;
        command->glyph = row.glyph;
        command->tip   = row.tip;

        command->dispatch = [this, id]
        {
            if (m_handlers.dispatch)
            {
                m_handlers.dispatch (id);
            }
        };

        command->isEnabled = [this, id]
        {
            return m_handlers.isEnabled ? m_handlers.isEnabled (id) : true;
        };

        command->tipText = [this, id, tip = std::wstring (row.tip)]
        {
            std::wstring  text = m_handlers.getTip ? m_handlers.getTip (id) : std::wstring();

            return text.empty() ? tip : text;
        };

        m_commands.push_back (std::move (command));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoBarCommands::BuildEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> UndoBarCommands::BuildEntries() const
{
    std::vector<DxuiToolbar::Entry>  entries;



    for (const std::shared_ptr<DxuiCommand> & command : m_commands)
    {
        DxuiToolbar::Entry  entry;

        entry.command  = command;
        entry.kind     = DxuiToolbar::Kind::Command;
        entry.iconOnly = true;

        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoBarCommands::Find
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> UndoBarCommands::Find (int id) const
{
    for (const std::shared_ptr<DxuiCommand> & command : m_commands)
    {
        if (command->id == id)
        {
            return command;
        }
    }

    return nullptr;
}

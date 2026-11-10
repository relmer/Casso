#include "Pch.h"

#include "Ui/Debugger/BreakpointBarCommands.h"
#include "Ui/Debugger/DebuggerIcons.h"
#include "Core/UnicodeSymbols.h"





//  Chosen from a rendered sheet of Segoe MDL2 Assets. The other buttons'
//  icons are in DebuggerIcons, since the font has nothing like Visual Studio's.
static constexpr const wchar_t *  s_kGlyphDelete     = s_kpszMdl2Delete;   // trash can
static constexpr const wchar_t *  s_kGlyphUndo       = s_kpszMdl2Undo;     // arrow curling back to the left
static constexpr const wchar_t *  s_kGlyphRedo       = s_kpszMdl2Redo;     // arrow curling over to the right





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointBarCommands::GetRows
//
//  Visual Studio's order: adding and removing, then turning off, then undo,
//  then where a breakpoint is, then the file. The columns are chosen from the
//  heading's menu.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<BreakpointBarCommands::Row> & BreakpointBarCommands::GetRows()
{
    static const std::vector<Row>  rows =
    {
        { kNew,        L"New",               nullptr,            L"New breakpoint",                                                    DxuiToolbar::Kind::DropDown, 0 },
        { kDelete,     L"Delete",            s_kGlyphDelete,     L"Delete the selected breakpoints",                                   DxuiToolbar::Kind::Command,  1 },
        { kDeleteAll,  L"Delete all",        nullptr,            L"Delete all breakpoints",                                            DxuiToolbar::Kind::Command,  1 },
        { kDisableAll, L"Disable all",       nullptr,            L"Disable all breakpoints, or enable them all when all are disabled", DxuiToolbar::Kind::Command,  1 },
        { kUndo,       L"Undo",              s_kGlyphUndo,       L"Undo the last change made in this pane",                            DxuiToolbar::Kind::Command,  2 },
        { kRedo,       L"Redo",              s_kGlyphRedo,       L"Redo the change just undone",                                       DxuiToolbar::Kind::Command,  2 },
        { kGoToSource, L"Go to source code", nullptr,            L"Go to the selected breakpoint's source line",                       DxuiToolbar::Kind::Command,  3 },
        { kGoToCode,   L"Go to disassembly", nullptr,            L"Go to the selected breakpoint in the disassembly",                  DxuiToolbar::Kind::Command,  3 },
        { kExport,     L"Export",            nullptr,            L"Save the breakpoints to a file",                                    DxuiToolbar::Kind::Command,  5 },
        { kImport,     L"Import",            nullptr,            L"Add the breakpoints saved in a file",                               DxuiToolbar::Kind::Command,  5 },
    };



    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointBarCommands::BreakpointBarCommands
//
////////////////////////////////////////////////////////////////////////////////

BreakpointBarCommands::BreakpointBarCommands (Handlers handlers)
{
    m_handlers = std::move (handlers);

    for (const Row & row : GetRows())
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
//  BreakpointBarCommands::BuildEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> BreakpointBarCommands::BuildEntries() const
{
    std::vector<DxuiToolbar::Entry>  entries;



    for (const Row & row : GetRows())
    {
        DxuiToolbar::Entry  entry;

        entry.command  = Find (row.id);
        entry.kind     = row.kind;
        entry.group    = row.group;
        entry.iconOnly = row.kind != DxuiToolbar::Kind::DropDown;

        switch (row.id)
        {
        case kDeleteAll:  entry.vectorIcon = &DebuggerIcons::s_kDeleteAll;  break;
        case kDisableAll: entry.vectorIcon = &DebuggerIcons::s_kDisableAll; break;
        case kGoToSource: entry.vectorIcon = &DebuggerIcons::s_kGoToSource; break;
        case kGoToCode:   entry.vectorIcon = &DebuggerIcons::s_kGoToCode;   break;
        case kExport:     entry.vectorIcon = &DebuggerIcons::s_kExport;     break;
        case kImport:     entry.vectorIcon = &DebuggerIcons::s_kImport;     break;
        default:          break;
        }

        //  Delete all's cross is the icon's accent part.
        if (row.id == kDeleteAll && m_handlers.crossArgb)
        {
            entry.getAccent = [this] { return m_handlers.crossArgb(); };
        }

        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointBarCommands::Find
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> BreakpointBarCommands::Find (int id) const
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





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointBarCommands::GetIds
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> BreakpointBarCommands::GetIds()
{
    std::vector<int>  ids;



    for (const Row & row : GetRows())
    {
        ids.push_back (row.id);
    }

    return ids;
}

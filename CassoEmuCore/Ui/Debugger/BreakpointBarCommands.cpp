#include "Pch.h"

#include "Ui/Debugger/BreakpointBarCommands.h"
#include "Core/UnicodeSymbols.h"





//  Chosen from a rendered sheet of Segoe MDL2 Assets.
static constexpr const wchar_t *  s_kGlyphNew        = s_kpszMdl2Add;   // plus
static constexpr const wchar_t *  s_kGlyphDelete     = s_kpszMdl2Clear;   // a cross
static constexpr const wchar_t *  s_kGlyphDeleteAll  = s_kpszMdl2Broom;   // broom
static constexpr const wchar_t *  s_kGlyphEnableAll  = s_kpszMdl2CheckboxComposite;   // a checked box
static constexpr const wchar_t *  s_kGlyphDisableAll = s_kpszMdl2Checkbox;   // an empty box
static constexpr const wchar_t *  s_kGlyphUndo       = s_kpszMdl2Undo;   // arrow curling back to the left
static constexpr const wchar_t *  s_kGlyphRedo       = s_kpszMdl2Redo;   // arrow curling over to the right
static constexpr const wchar_t *  s_kGlyphSource     = s_kpszMdl2Page;   // a page
static constexpr const wchar_t *  s_kGlyphCode       = s_kpszMdl2List;   // a list of lines
static constexpr const wchar_t *  s_kGlyphColumns    = s_kpszMdl2ViewAll;   // a grid
static constexpr const wchar_t *  s_kGlyphExport     = s_kpszMdl2Export;   // arrow out of a bar
static constexpr const wchar_t *  s_kGlyphImport     = s_kpszMdl2Import;   // arrow into a bar





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointBarCommands::GetRows
//
//  Adding and removing, then turning on and off, then undo, then where a
//  breakpoint is, then the pane's columns, then the file.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<BreakpointBarCommands::Row> & BreakpointBarCommands::GetRows()
{
    static const std::vector<Row>  rows =
    {
        { kNew,        L"New",               s_kGlyphNew,        L"New breakpoint",                                   DxuiToolbar::Kind::DropDown, 0 },
        { kDelete,     L"Delete",            s_kGlyphDelete,     L"Delete the selected breakpoints",                  DxuiToolbar::Kind::Command,  0 },
        { kDeleteAll,  L"Delete all",        s_kGlyphDeleteAll,  L"Delete all breakpoints",                           DxuiToolbar::Kind::Command,  0 },
        { kEnableAll,  L"Enable all",        s_kGlyphEnableAll,  L"Enable all breakpoints",                           DxuiToolbar::Kind::Command,  1 },
        { kDisableAll, L"Disable all",       s_kGlyphDisableAll, L"Disable all breakpoints",                          DxuiToolbar::Kind::Command,  1 },
        { kUndo,       L"Undo",              s_kGlyphUndo,       L"Undo the last change made in this pane",           DxuiToolbar::Kind::Command,  2 },
        { kRedo,       L"Redo",              s_kGlyphRedo,       L"Redo the change just undone",                      DxuiToolbar::Kind::Command,  2 },
        { kGoToSource, L"Go to source code", s_kGlyphSource,     L"Go to the selected breakpoint's source line",      DxuiToolbar::Kind::Command,  3 },
        { kGoToCode,   L"Go to disassembly", s_kGlyphCode,       L"Go to the selected breakpoint in the disassembly", DxuiToolbar::Kind::Command,  3 },
        { kColumns,    L"Show columns",      s_kGlyphColumns,    L"Choose the columns this pane shows",               DxuiToolbar::Kind::DropDown, 4 },
        { kExport,     L"Export",            s_kGlyphExport,     L"Save the breakpoints to a file",                   DxuiToolbar::Kind::Command,  5 },
        { kImport,     L"Import",            s_kGlyphImport,     L"Add the breakpoints saved in a file",              DxuiToolbar::Kind::Command,  5 },
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
        entry.iconOnly = true;

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






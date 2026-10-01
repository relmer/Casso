#include "Pch.h"

#include "Ui/Debugger/MemoryBarCommands.h"





static constexpr const wchar_t *  s_kGlyphRefresh   = L"";   // circular arrow
static constexpr const wchar_t *  s_kGlyphNewWindow = L"";   // plus





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBarCommands::GetRows
//
//  Where to look, then how to lay the bytes out, then another window.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<MemoryBarCommands::Row> & MemoryBarCommands::GetRows()
{
    static const std::vector<Row>  rows =
    {
        { kAddress,   L"Address",           nullptr,            L"Address: a hex address, a register, a symbol or an expression such as (3E),Y", DxuiToolbar::Kind::DropDown, 0, false },
        { kRefresh,   L"Refresh",           s_kGlyphRefresh,    L"Read this window's bytes again",                                               DxuiToolbar::Kind::Command,  0, true  },
        { kColumns,   L"Columns",           nullptr,            L"Values in each row, or as many as fit",                                        DxuiToolbar::Kind::DropDown, 1, false },
        { kGrouping,  L"Group by bytes",    nullptr,            L"Show each value as a byte, a word or a long",                                  DxuiToolbar::Kind::DropDown, 1, false },
        { kNewWindow, L"New memory window", s_kGlyphNewWindow,  L"New memory window",                                                            DxuiToolbar::Kind::Command,  2, true  },
    };



    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBarCommands::MemoryBarCommands
//
////////////////////////////////////////////////////////////////////////////////

MemoryBarCommands::MemoryBarCommands (Handlers handlers)
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

        command->labelText = [this, id, label = std::wstring (row.label)]
        {
            std::wstring  text = m_handlers.getLabel ? m_handlers.getLabel (id) : std::wstring();

            return text.empty() ? label : text;
        };

        m_commands.push_back (std::move (command));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBarCommands::BuildEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> MemoryBarCommands::BuildEntries (IDxuiToolbarCustomEntry * address) const
{
    std::vector<DxuiToolbar::Entry>  entries;



    for (const Row & row : GetRows())
    {
        DxuiToolbar::Entry  entry;

        entry.command  = Find (row.id);
        entry.kind     = row.kind;
        entry.group    = row.group;
        entry.iconOnly = row.iconOnly;

        if (row.id == kAddress)
        {
            entry.custom = address;
        }

        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBarCommands::Find
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> MemoryBarCommands::Find (int id) const
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
//  MemoryBarCommands::GetColumnChoices
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<int> & MemoryBarCommands::GetColumnChoices()
{
    static const std::vector<int>  choices = { 0, 1, 2, 4, 8, 16 };



    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBarCommands::GetColumnsLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryBarCommands::GetColumnsLabel (int columns)
{
    return (columns == 0) ? std::wstring (L"Auto") : std::to_wstring (columns);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBarCommands::GetGroupingChoices
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<int> & MemoryBarCommands::GetGroupingChoices()
{
    static const std::vector<int>  choices = { 1, 2, 4 };



    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBarCommands::GetGroupingName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryBarCommands::GetGroupingName (int grouping)
{
    constexpr int  kWord = 2;
    constexpr int  kLong = 4;



    switch (grouping)
    {
    case kWord: return L"Words";
    case kLong: return L"Longs";
    default:    return L"Bytes";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBarCommands::GetGroupingLabel
//
//  What the drop-down sets, with the choice in force: "Group by words".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryBarCommands::GetGroupingLabel (int grouping)
{
    std::wstring  name = GetGroupingName (grouping);



    name[0] = (wchar_t) towlower (name[0]);

    return L"Group by " + name;
}

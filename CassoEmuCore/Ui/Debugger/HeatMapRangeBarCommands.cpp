#include "Pch.h"

#include "Ui/Debugger/HeatMapRangeBarCommands.h"
#include "Core/UnicodeSymbols.h"





//  Chosen from a rendered sheet of Segoe MDL2 Assets.
static constexpr const wchar_t *  s_kGlyphNew    = s_kpszMdl2Add;      // plus
static constexpr const wchar_t *  s_kGlyphEdit   = s_kpszMdl2Rename;   // A in a text box
static constexpr const wchar_t *  s_kGlyphRemove = s_kpszMdl2Delete;   // trash can
static constexpr const wchar_t *  s_kGlyphUp     = s_kpszMdl2Up;       // arrow up
static constexpr const wchar_t *  s_kGlyphDown   = s_kpszMdl2Down;     // arrow down





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeBarCommands::GetRows
//
//  The set first, since every other entry acts within it; then the ranges.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<HeatMapRangeBarCommands::Row> & HeatMapRangeBarCommands::GetRows()
{
    static const std::vector<Row>  rows =
    {
        { kSet,       L"Set",        nullptr,        L"The set of ranges this pane edits",                       DxuiToolbar::Kind::DropDown, 0, false },
        { kNewSet,    L"New set",    nullptr,        L"Make a set, starting with every built-in range left out", DxuiToolbar::Kind::Command,  0, false },
        { kRenameSet, L"Rename set", nullptr,        L"Rename this set",                                         DxuiToolbar::Kind::Command,  0, false },
        { kDeleteSet, L"Delete set", nullptr,        L"Delete this set and its ranges",                          DxuiToolbar::Kind::Command,  0, false },
        { kNew,       L"New",        s_kGlyphNew,    L"New range, as $6000-$95FF or ZP_VARS..+$20",              DxuiToolbar::Kind::Command,  1, true  },
        { kEdit,      L"Edit",       s_kGlyphEdit,   L"Edit the selected range",                                 DxuiToolbar::Kind::Command,  1, true  },
        { kRemove,    L"Remove",     s_kGlyphRemove, L"Remove the selected range",                               DxuiToolbar::Kind::Command,  1, true  },
        { kUp,        L"Move up",    s_kGlyphUp,     L"Move the selected range up the map",                      DxuiToolbar::Kind::Command,  2, true  },
        { kDown,      L"Move down",  s_kGlyphDown,   L"Move the selected range down the map",                    DxuiToolbar::Kind::Command,  2, true  },
    };



    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeBarCommands::HeatMapRangeBarCommands
//
////////////////////////////////////////////////////////////////////////////////

HeatMapRangeBarCommands::HeatMapRangeBarCommands (Handlers handlers)
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
//  HeatMapRangeBarCommands::BuildEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> HeatMapRangeBarCommands::BuildEntries() const
{
    std::vector<DxuiToolbar::Entry>  entries;



    for (const Row & row : GetRows())
    {
        DxuiToolbar::Entry  entry;

        entry.command  = Find (row.id);
        entry.kind     = row.kind;
        entry.group    = row.group;
        entry.iconOnly = row.iconOnly;

        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeBarCommands::Find
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> HeatMapRangeBarCommands::Find (int id) const
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
//  HeatMapRangeBarCommands::GetSetLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapRangeBarCommands::GetSetLabel (const std::wstring & name)
{
    return name.empty() ? std::wstring (L"No sets") : L"Set: " + name;
}






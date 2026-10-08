#include "Pch.h"

#include "Ui/Debugger/HeatMapBarCommands.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands::GetRows
//
//  How the map counts, with Reset counts beside it while it counts totals,
//  how it mixes colors, and which writes and reads before written it leaves
//  out; then, in the map's own row of views, which ranges and which bank it
//  shows. The zoom is the map's own widget, in its bottom-right corner.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<HeatMapBarCommands::Row> & HeatMapBarCommands::GetRows()
{
    static const std::vector<Row>  rows =
    {
        { kMode,        L"Fading",                nullptr, L"Fade each address as its accesses age, or keep a total since the counts were reset", DxuiToolbar::Kind::DropDown, 0, false, false, false },
        { kResetCounts, L"Reset counts",          nullptr, L"Start the totals over",                                                             DxuiToolbar::Kind::Command,  0, false, false, false },
        { kBlend,       L"Blend",                 nullptr, L"Mix the colors of an address touched more than one way",                            DxuiToolbar::Kind::Toggle,   4, false, false, false },
        { kIgnoreSame,  L"Skip unchanged writes", nullptr, L"Ignore writes that don't change the value: show only the writes that stored a different value", DxuiToolbar::Kind::Toggle, 5, false, false, false },
        { kIgnoreSet,   L"Leave out: None",       nullptr, L"Leave a set's ranges out of the reads before written",                              DxuiToolbar::Kind::DropDown, 5, false, false, false },
        { kRangeSet,    L"All memory",            nullptr, L"Show all of memory, or only the ranges of a set",                                   DxuiToolbar::Kind::DropDown, 0, false, true,  true  },
        { kBank,        L"Bank",                  nullptr, L"Show what the CPU addresses, or one bank as it is stored",                          DxuiToolbar::Kind::DropDown, 0, false, true,  true  },
    };



    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands::HeatMapBarCommands
//
////////////////////////////////////////////////////////////////////////////////

HeatMapBarCommands::HeatMapBarCommands (Handlers handlers)
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

        command->isChecked = [this, id]
        {
            return m_handlers.isChecked ? m_handlers.isChecked (id) : false;
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
//  HeatMapBarCommands::BuildEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> HeatMapBarCommands::BuildEntries (bool isCumulative) const
{
    return BuildEntriesOf (false, isCumulative);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands::BuildViewRowEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> HeatMapBarCommands::BuildViewRowEntries() const
{
    return BuildEntriesOf (true, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands::BuildEntriesOf
//
//  The rows of the bar or of the view row; Reset counts only while the map
//  counts totals, as there is nothing to reset while it fades.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> HeatMapBarCommands::BuildEntriesOf (bool viewRow, bool isCumulative) const
{
    std::vector<DxuiToolbar::Entry>  entries;



    for (const Row & row : GetRows())
    {
        DxuiToolbar::Entry  entry;

        if (row.inViewRow != viewRow || (row.id == kResetCounts && !isCumulative))
        {
            continue;
        }

        entry.command  = Find (row.id);
        entry.kind     = row.kind;
        entry.group    = row.group;
        entry.iconOnly = row.iconOnly;
        entry.trailing = row.trailing;

        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands::Find
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> HeatMapBarCommands::Find (int id) const
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
//  HeatMapBarCommands::GetFadeChoiceLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapBarCommands::GetFadeChoiceLabel (int seconds)
{
    return std::format (L"{} s", seconds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands::GetFadeLabel
//
//  The mode in force with its fade time: "Fading: 10 s".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapBarCommands::GetFadeLabel (int seconds)
{
    return L"Fading: " + GetFadeChoiceLabel (seconds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands::GetBankEntryLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapBarCommands::GetBankEntryLabel (HeatMapOptions::Bank bank)
{
    return HeatMapOptions::GetBankLabel (bank);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands::GetIgnoreSetLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapBarCommands::GetIgnoreSetLabel (const std::wstring & set)
{
    return L"Leave out: " + (set.empty() ? std::wstring (L"None") : set);
}






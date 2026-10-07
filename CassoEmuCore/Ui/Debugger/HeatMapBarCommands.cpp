#include "Pch.h"

#include "Ui/Debugger/HeatMapBarCommands.h"
#include "Core/UnicodeSymbols.h"





static constexpr const wchar_t *  s_kGlyphZoomIn  = s_kpszMdl2ZoomIn;    // magnifier with a plus
static constexpr const wchar_t *  s_kGlyphZoomOut = s_kpszMdl2ZoomOut;   // magnifier with a minus





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBarCommands::GetRows
//
//  How the map counts, then the zoom at the far end.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<HeatMapBarCommands::Row> & HeatMapBarCommands::GetRows()
{
    static const std::vector<Row>  rows =
    {
        { kFading,      L"Fading",         nullptr,         L"Show how recently and how often each address was touched", DxuiToolbar::Kind::Toggle,   0, false, false },
        { kCumulative,  L"Cumulative",     nullptr,         L"Show each address's total since the counts were reset",    DxuiToolbar::Kind::Toggle,   0, false, false },
        { kFade,        L"Fade",           nullptr,         L"How long a single access stays on the map",                DxuiToolbar::Kind::DropDown, 1, false, false },
        { kResetCounts, L"Reset counts",   nullptr,         L"Start the totals over",                                    DxuiToolbar::Kind::Command,  1, false, false },
        { kRangeSet,    L"All memory",     nullptr,         L"Show all of memory, or only the ranges of a set",          DxuiToolbar::Kind::DropDown, 3, false, false },
        { kEditRanges,  L"Edit ranges...", nullptr,         L"Make and change the sets of ranges the map can show",      DxuiToolbar::Kind::Command,  3, false, false },
        { kZoomIn,      L"Zoom in",        s_kGlyphZoomIn,  L"Zoom in (Ctrl+wheel over the map)",                        DxuiToolbar::Kind::Command,  2, true,  true  },
        { kZoomOut,     L"Zoom out",       s_kGlyphZoomOut, L"Zoom out (Ctrl+wheel over the map)",                       DxuiToolbar::Kind::Command,  2, true,  true  },
        { kResetZoom,   L"Reset zoom",     nullptr,         L"Go back to the starting size, with $0000 at the top left", DxuiToolbar::Kind::Command,  2, false, true  },
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

std::vector<DxuiToolbar::Entry> HeatMapBarCommands::BuildEntries() const
{
    std::vector<DxuiToolbar::Entry>  entries;



    for (const Row & row : GetRows())
    {
        DxuiToolbar::Entry  entry;

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
//  What the drop-down sets, with the choice in force: "Fade: 10 s".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapBarCommands::GetFadeLabel (int seconds)
{
    return L"Fade: " + GetFadeChoiceLabel (seconds);
}

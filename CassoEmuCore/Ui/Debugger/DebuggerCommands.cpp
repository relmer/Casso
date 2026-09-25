#include "Pch.h"

#include "Ui/Debugger/DebuggerCommands.h"





//  Segoe MDL2 Assets, chosen off a rendered sheet at both sizes the strip
//  draws: no guessed codepoints, and nothing that turns to mush at the icon
//  size. The font has no step-over arc, so the mark that reads as "past this
//  one" stands in for it.
static constexpr const wchar_t *  s_kGlyphRun         = L"\uE768";   // play
static constexpr const wchar_t *  s_kGlyphPause       = L"\uE769";   // pause bars
static constexpr const wchar_t *  s_kGlyphStepInto    = L"\uE896";   // arrow down to a bar
static constexpr const wchar_t *  s_kGlyphStepOver    = L"\uE7A6";   // an arc up and over, to the right
static constexpr const wchar_t *  s_kGlyphStepOut     = L"\uE898";   // arrow up from a bar
static constexpr const wchar_t *  s_kGlyphRunToCursor = L"";         // MDL2 has no arrow into a bar, so the entry draws its own icon
static constexpr const wchar_t *  s_kGlyphShowNext    = L"\uE72A";   // a plain arrow to the right
static constexpr const wchar_t *  s_kGlyphTrace       = L"\uE81C";   // clock with a turning arrow
static constexpr const wchar_t *  s_kGlyphPanels      = L"\uE950";   // chip
static constexpr const wchar_t *  s_kGlyphKeys        = L"\uE765";   // keyboard
static constexpr const wchar_t *  s_kGlyphMode        = L"\uE943";   // braces





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::GetRows
//
//  Running and stopping first, then where the code pane looks, then the
//  choices that change what the window shows.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<DebuggerCommands::Row> & DebuggerCommands::GetRows()
{
    static const std::vector<Row>  rows =
    {
        { kRun,         L"Run",           s_kGlyphRun,         L"Run until something stops the machine",        DxuiToolbar::Kind::Command,  0, false },
        { kPause,       L"Pause",         s_kGlyphPause,       L"Stop the running machine",                     DxuiToolbar::Kind::Command,  0, false },
        { kStepInto,    L"Step into",     s_kGlyphStepInto,    L"Step one instruction, into a call",            DxuiToolbar::Kind::Command,  1, false },
        { kStepOver,    L"Step over",     s_kGlyphStepOver,    L"Step one instruction, over a call",            DxuiToolbar::Kind::Command,  1, false },
        { kStepOut,     L"Step out",      s_kGlyphStepOut,     L"Run to the return of the current call",        DxuiToolbar::Kind::Command,  1, false },
        { kRunToCursor, L"Run to cursor", s_kGlyphRunToCursor, L"Run until the selected line",                  DxuiToolbar::Kind::Command,  1, false },
        { kShowNext,    L"Show next",     s_kGlyphShowNext,    L"Bring the code panes to the next statement",           DxuiToolbar::Kind::Command,  2, false },
        { kTrace,       L"Trace",         s_kGlyphTrace,       L"Record every instruction the machine runs",    DxuiToolbar::Kind::Toggle,   2, true  },
        { kPanels,      L"Panels",        s_kGlyphPanels,      L"Open a panel for one of the machine's devices", DxuiToolbar::Kind::DropDown, 3, false },
        { kMode,        L"Dialect",       s_kGlyphMode,        L"The command dialect the console reads",        DxuiToolbar::Kind::DropDown, 3, false },
        { kKeyScheme,   L"Keys",          s_kGlyphKeys,        L"Which editor's keys drive the debugger",       DxuiToolbar::Kind::DropDown, 3, false },
    };



    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::DebuggerCommands
//
////////////////////////////////////////////////////////////////////////////////

DebuggerCommands::DebuggerCommands (Handlers handlers)
{
    m_handlers = std::move (handlers);

    for (const Row & row : GetRows())
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        int                           id      = row.id;

        command->id    = row.id;
        command->label = row.label;
        command->glyph = row.glyph;
        command->tip   = GetTip (row, L"");

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

        if (row.checkable)
        {
            command->isChecked = [this, id]
            {
                return m_handlers.isChecked ? m_handlers.isChecked (id) : false;
            };
        }

        m_commands.push_back (std::move (command));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::BuildEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> DebuggerCommands::BuildEntries() const
{
    std::vector<DxuiToolbar::Entry>  entries;



    for (const Row & row : GetRows())
    {
        DxuiToolbar::Entry  entry;

        entry.command = Find (row.id);
        entry.kind    = row.kind;
        entry.group   = row.group;

        if (row.id == kRunToCursor)
        {
            //  Drawn rather than a glyph, but through the strip's own icon
            //  path: hover, disabling and theme are decided once, for every
            //  entry, and handed here as the box's ink.
            entry.icon = [] (IDxuiPainter & painter, const DxuiToolbarIconBox & icon)
            {
                PaintRunToCursor (painter, icon, icon.ink);
            };
        }

        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintRunToCursor
//
//  Visual Studio's Run to Cursor: an arrow pointing right that ends at a
//  vertical bar, in strokes as thin as the icon font's.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintRunToCursor (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t ink)
{
    float  s      = icon.size;
    float  cy     = icon.top + icon.rowH * 0.5f;
    float  stroke = (std::max) (1.0f, s / 14.0f);
    float  tipX   = icon.x + s * 0.74f;
    float  barX   = icon.x + s * 0.92f;



    painter.DrawLine (icon.x + s * 0.02f, cy, tipX, cy, stroke, ink);
    painter.DrawLine (tipX, cy, tipX - s * 0.30f, cy - s * 0.30f, stroke, ink);
    painter.DrawLine (tipX, cy, tipX - s * 0.30f, cy + s * 0.30f, stroke, ink);
    painter.DrawLine (barX, cy - s * 0.42f, barX, cy + s * 0.42f, stroke, ink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::Find
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> DebuggerCommands::Find (int id) const
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
//  DebuggerCommands::GetTip
//
//  The entry's title with the key that runs it in the scheme in force, as
//  Visual Studio writes its toolbar tips, then what it does.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerCommands::GetTip (const Row & row, const std::wstring & accelerator)
{
    std::wstring  title = accelerator.empty() ? std::wstring (row.label) : std::format (L"{} ({})", row.label, accelerator);



    return title + L"\n" + row.tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::ApplyKeyScheme
//
//  The strip shows each command's key as the scheme in force binds it, so
//  changing the scheme relabels the tips rather than leaving them wrong.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::ApplyKeyScheme (DebuggerKeyScheme scheme)
{
    const DxuiKeyMap &  map = DebuggerKeySchemes::GetMap (scheme);



    for (size_t i = 0; i < m_commands.size() && i < GetRows().size(); i++)
    {
        DxuiCommand &  command = *m_commands[i];

        command.accelerator = map.GetChordText (command.id);
        command.tip         = GetTip (GetRows()[i], command.accelerator);
    }
}

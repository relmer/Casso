#include "Pch.h"

#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/DebuggerIcons.h"
#include "Core/UnicodeSymbols.h"





//  Segoe MDL2 Assets, chosen off a rendered sheet at both sizes the strip
//  draws: no guessed codepoints, and nothing that turns to mush at the icon
//  size. On the bar every command but Trace is drawn from DebuggerIcons
//  instead; the glyphs are for a menu.
static constexpr const wchar_t *  s_kGlyphRun         = L"\uE768";   // play
static constexpr const wchar_t *  s_kGlyphPause       = L"\uE769";   // pause bars
static constexpr const wchar_t *  s_kGlyphStepInto    = L"\uE896";   // arrow down to a bar
static constexpr const wchar_t *  s_kGlyphStepOver    = L"\uE7A6";   // an arc up and over, to the right
static constexpr const wchar_t *  s_kGlyphStepOut     = L"\uE898";   // arrow up from a bar
static constexpr const wchar_t *  s_kGlyphRunToCursor = L"";         // MDL2 has no arrow into a bar
static constexpr const wchar_t *  s_kGlyphShowNext    = L"\uE72A";   // a plain arrow to the right
static constexpr const wchar_t *  s_kGlyphTrace       = L"\uE81C";   // clock with a turning arrow
static constexpr const wchar_t *  s_kGlyphLoadFile    = L"";         // no glyph for a grid of 1s and 0s
static constexpr const wchar_t *  s_kGlyphDrawn       = L"";         // no glyphs for the step back and reverse commands





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::GetRows
//
//  Running and stopping first, then where the code pane looks and the trace.
//  The run and step buttons are icons alone, as Visual Studio's are; each tip
//  gives the command and its key. The choices that change what the window
//  shows live in the menu bar, and the dialect in the console's own bar.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<DebuggerCommands::Row> & DebuggerCommands::GetRows()
{
    static const std::vector<Row>  rows =
    {
        { kRunOrPause,      L"Run",              s_kGlyphRun,         L"Run until something stops the machine",                DxuiToolbar::Kind::Command, 0, false, true  },
        { kRun,             L"Run",              s_kGlyphRun,         L"Run until something stops the machine",                DxuiToolbar::Kind::Command, 0, false, true  },
        { kPause,           L"Pause",            s_kGlyphPause,       L"Stop the running machine",                             DxuiToolbar::Kind::Command, 0, false, true  },
        { kStepInto,        L"Step into",        s_kGlyphStepInto,    L"Step one instruction, into a call",                    DxuiToolbar::Kind::Command, 1, false, true  },
        { kStepOver,        L"Step over",        s_kGlyphStepOver,    L"Step one instruction, over a call",                    DxuiToolbar::Kind::Command, 1, false, true  },
        { kStepOut,         L"Step out",         s_kGlyphStepOut,     L"Run to the return of the current call",                DxuiToolbar::Kind::Command, 1, false, true  },
        { kRunToCursor,     L"Run to cursor",    s_kGlyphRunToCursor, L"Run until the selected line",                          DxuiToolbar::Kind::Command, 1, false, true  },
        { kStepBackInto,    L"Step back into",   s_kGlyphDrawn,       L"Go back one instruction, into a call",                 DxuiToolbar::Kind::Command, 2, false, true  },
        { kStepBackOver,    L"Step back over",   s_kGlyphDrawn,       L"Go back one instruction, over a call",                 DxuiToolbar::Kind::Command, 2, false, true  },
        { kStepBackOut,     L"Step back out",    s_kGlyphDrawn,       L"Go back to the call that entered the current routine", DxuiToolbar::Kind::Command, 2, false, true  },
        { kReverseContinue, L"Reverse continue", s_kGlyphDrawn,       L"Run backward to the latest breakpoint",                 DxuiToolbar::Kind::Command, 2, false, true  },
        { kShowNext,        L"Show next",        s_kGlyphShowNext,    L"Bring the code panes to the next statement",           DxuiToolbar::Kind::Command, 3, false, true  },
        { kTrace,           L"Trace",            s_kGlyphTrace,       L"Record every instruction the machine runs",            DxuiToolbar::Kind::Toggle,  3, true,  false },
        { kLoadFile,        L"Load file into memory", s_kGlyphLoadFile, L"Load a file into memory, as BLOAD does",            DxuiToolbar::Kind::Command, 4, false, true  },
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

        //  The bar's one button is Run while the machine is stopped and
        //  Pause while it runs, with that command's name and tip.
        if (id == kRunOrPause)
        {
            command->labelText = [this] { return Find (IsPausing() ? kPause : kRun)->label; };
            command->tipText   = [this] { return Find (IsPausing() ? kPause : kRun)->GetTipText(); };
        }

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

        //  Run and Pause are one button on the bar.
        if (row.id == kRun || row.id == kPause)
        {
            continue;
        }

        entry.command  = Find (row.id);
        entry.kind     = row.kind;
        entry.group    = row.group;
        entry.iconOnly = row.iconOnly;

        if (row.id != kTrace)
        {
            bool  isRun = (row.id == kRunOrPause || row.id == kReverseContinue);

            //  The bar's one button is drawn as Pause while the machine runs.
            if (row.id == kRunOrPause)
            {
                entry.getVectorIcon = [this] { return GetIcon (IsPausing() ? kPause : kRun); };
            }
            else
            {
                entry.vectorIcon = GetIcon (row.id);
            }

            //  The icon's accent parts are in the run or the step color.
            entry.getAccent = [this, isRun]
            {
                IconColors  colors = GetIconColors (m_handlers.isDark ? m_handlers.isDark() : true);

                return isRun ? colors.run : colors.step;
            };
        }

        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::GetIcon
//
//  Returns the vector icon of a command, or nullptr for a command drawn as a
//  glyph.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiVectorIcon * DebuggerCommands::GetIcon (int id)
{
    const DxuiVectorIcon  * icon = nullptr;



    switch (id)
    {
        case kRun:             icon = &DebuggerIcons::s_kRun;          break;
        case kPause:           icon = &DebuggerIcons::s_kPause;        break;
        case kStepInto:        icon = &DebuggerIcons::s_kStepInto;     break;
        case kStepOver:        icon = &DebuggerIcons::s_kStepOver;     break;
        case kStepOut:         icon = &DebuggerIcons::s_kStepOut;      break;
        case kRunToCursor:     icon = &DebuggerIcons::s_kRunToCursor;  break;
        case kStepBackInto:    icon = &DebuggerIcons::s_kStepBackInto; break;
        case kStepBackOver:    icon = &DebuggerIcons::s_kStepBackOver; break;
        case kStepBackOut:     icon = &DebuggerIcons::s_kStepBackOut;  break;
        case kReverseContinue: icon = &DebuggerIcons::s_kReverseRun;   break;
        case kShowNext:        icon = &DebuggerIcons::s_kShowNext;     break;
        case kLoadFile:        icon = &DebuggerIcons::s_kLoadFile;     break;
        default:                                                       break;
    }

    return icon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::IsPausing
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerCommands::IsPausing() const
{
    return m_handlers.isEnabled && m_handlers.isEnabled (kPause);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::GetIconColors
//
//  Visual Studio's green run triangle and blue step arrows. Each is lighter on
//  a dark ground and deeper on a light one, for contrast.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerCommands::IconColors DebuggerCommands::GetIconColors (bool isDark)
{
    IconColors  colors;



    colors.run  = isDark ? 0xFF6CCB5Fu : 0xFF1F883Du;
    colors.step = isDark ? 0xFF4FA8E8u : 0xFF1A6FC4u;

    return colors;
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

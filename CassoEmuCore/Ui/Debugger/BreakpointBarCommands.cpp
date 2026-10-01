#include "Pch.h"

#include "Ui/Debugger/BreakpointBarCommands.h"
#include "Core/UnicodeSymbols.h"





//  Chosen from a rendered sheet of Segoe MDL2 Assets. The other buttons'
//  icons are drawn, since the font has nothing like Visual Studio's.
static constexpr const wchar_t *  s_kGlyphDelete     = s_kpszMdl2Delete;   // trash can
static constexpr const wchar_t *  s_kGlyphUndo       = s_kpszMdl2Undo;     // arrow curling back to the left
static constexpr const wchar_t *  s_kGlyphRedo       = s_kpszMdl2Redo;     // arrow curling over to the right





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointBarCommands::GetRows
//
//  Visual Studio's order: adding and removing, then turning off, then undo,
//  then where a breakpoint is, then the pane's columns, then the file.
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
        { kColumns,    L"Show columns",      nullptr,            L"Choose the columns this pane shows",                                DxuiToolbar::Kind::DropDown, 4 },
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
        case kDeleteAll:
            entry.icon = [this] (IDxuiPainter & painter, const DxuiToolbarIconBox & icon)
            {
                PaintBalls (painter, icon, m_handlers.crossArgb ? m_handlers.crossArgb() : icon.ink);
            };
            break;

        case kDisableAll: entry.icon = [] (IDxuiPainter & painter, const DxuiToolbarIconBox & icon) { PaintBalls     (painter, icon, std::nullopt); }; break;
        case kGoToSource: entry.icon = [] (IDxuiPainter & painter, const DxuiToolbarIconBox & icon) { PaintGoTo      (painter, icon, false);        }; break;
        case kGoToCode:   entry.icon = [] (IDxuiPainter & painter, const DxuiToolbarIconBox & icon) { PaintGoTo      (painter, icon, true);         }; break;
        case kExport:     entry.icon = [] (IDxuiPainter & painter, const DxuiToolbarIconBox & icon) { PaintFileArrow (painter, icon, true);         }; break;
        case kImport:     entry.icon = [] (IDxuiPainter & painter, const DxuiToolbarIconBox & icon) { PaintFileArrow (painter, icon, false);        }; break;
        default:          break;
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





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointBarCommands::PaintBalls
//
//  Delete all's and Disable all's icon: two breakpoint marks, one behind the
//  other, and for Delete all a red cross over them.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointBarCommands::PaintBalls (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, std::optional<uint32_t> crossArgb)
{
    float     s     = icon.size;
    float     cy    = icon.top + icon.rowH * 0.5f;
    float     r     = s * 0.30f;
    float     t     = std::max (1.0f, s / 12.0f);
    uint32_t  faint = (icon.ink & 0x00FFFFFFu) | (((icon.ink >> 24) / 2) << 24);
    float     cx0   = icon.x + s * 0.20f;
    float     cy0   = cy - s * 0.30f;
    float     arm   = s * 0.13f;



    painter.FillCircle (icon.x + s * 0.66f, cy + s * 0.08f, r, faint);
    painter.FillCircle (icon.x + s * 0.44f, cy + s * 0.14f, r, icon.ink);

    if (crossArgb.has_value())
    {
        painter.DrawLine (cx0 - arm, cy0 - arm, cx0 + arm, cy0 + arm, t * 1.5f, *crossArgb);
        painter.DrawLine (cx0 - arm, cy0 + arm, cx0 + arm, cy0 - arm, t * 1.5f, *crossArgb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointBarCommands::PaintGoTo
//
//  Lines of text with an arrow into the first, for Go to source code; with a
//  bracket ahead of them, for Go to disassembly.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointBarCommands::PaintGoTo (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, bool bracket)
{
    float  s    = icon.size;
    float  cy   = icon.top + icon.rowH * 0.5f;
    float  t    = std::max (1.0f, s / 12.0f);
    float  left = icon.x + s * 0.42f;
    float  top  = cy - s * 0.32f;



    for (int i = 0; i < 3; i++)
    {
        float  y = top + i * s * 0.32f;

        painter.DrawLine (left, y, icon.x + s, y, t, icon.ink);
    }

    if (bracket)
    {
        float  x = icon.x + s * 0.12f;

        painter.DrawLine (x,            cy - s * 0.42f, x,            cy + s * 0.42f, t, icon.ink);
        painter.DrawLine (x - t * 0.5f, cy - s * 0.42f, x + s * 0.16f, cy - s * 0.42f, t, icon.ink);
        painter.DrawLine (x - t * 0.5f, cy + s * 0.42f, x + s * 0.16f, cy + s * 0.42f, t, icon.ink);
        return;
    }

    painter.DrawLine (icon.x, top, icon.x + s * 0.30f, top, t, icon.ink);
    PaintArrowHead   (painter, icon.x + s * 0.30f, top, s * 0.16f, true, t, icon.ink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointBarCommands::PaintFileArrow
//
//  An arrow and a bar on the right: Export's points left, away from the bar;
//  Import's points right, into it.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointBarCommands::PaintFileArrow (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, bool out)
{
    float  s    = icon.size;
    float  cy   = icon.top + icon.rowH * 0.5f;
    float  t    = std::max (1.0f, s / 12.0f);
    float  barX = icon.x + s * 0.92f;
    float  from = out ? icon.x + s * 0.80f : icon.x + s * 0.05f;
    float  to   = out ? icon.x + s * 0.08f : icon.x + s * 0.76f;



    painter.DrawLine (barX, cy - s * 0.36f, barX, cy + s * 0.36f, t, icon.ink);
    painter.DrawLine (from, cy, to, cy, t, icon.ink);
    PaintArrowHead   (painter, to, cy, s * 0.28f, !out, t, icon.ink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointBarCommands::PaintArrowHead
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointBarCommands::PaintArrowHead (IDxuiPainter & painter, float tipX, float tipY, float length, bool pointsRight, float thickness, uint32_t ink)
{
    float  back = pointsRight ? tipX - length : tipX + length;



    painter.DrawLine (back, tipY - length, tipX, tipY, thickness, ink);
    painter.DrawLine (back, tipY + length, tipX, tipY, thickness, ink);
}





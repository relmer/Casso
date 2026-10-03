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
static constexpr const wchar_t *  s_kGlyphDrawn       = L"";         // the step back and reverse entries draw their own icons





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
        { kRun,             L"Run",              s_kGlyphRun,         L"Run until something stops the machine",                DxuiToolbar::Kind::Command, 0, false, true  },
        { kPause,           L"Pause",            s_kGlyphPause,       L"Stop the running machine",                             DxuiToolbar::Kind::Command, 0, false, true  },
        { kStepInto,        L"Step into",        s_kGlyphStepInto,    L"Step one instruction, into a call",                    DxuiToolbar::Kind::Command, 1, false, true  },
        { kStepOver,        L"Step over",        s_kGlyphStepOver,    L"Step one instruction, over a call",                    DxuiToolbar::Kind::Command, 1, false, true  },
        { kStepOut,         L"Step out",         s_kGlyphStepOut,     L"Run to the return of the current call",                DxuiToolbar::Kind::Command, 1, false, true  },
        { kRunToCursor,     L"Run to cursor",    s_kGlyphRunToCursor, L"Run until the selected line",                          DxuiToolbar::Kind::Command, 1, false, true  },
        { kStepBackInto,    L"Step back into",   s_kGlyphDrawn,       L"Go back one instruction, into a call",                 DxuiToolbar::Kind::Command, 2, false, true  },
        { kStepBackOver,    L"Step back over",   s_kGlyphDrawn,       L"Go back one instruction, over a call",                 DxuiToolbar::Kind::Command, 2, false, true  },
        { kStepBackOut,     L"Step back out",    s_kGlyphDrawn,       L"Go back to the call that entered the current routine", DxuiToolbar::Kind::Command, 2, false, true  },
        { kReverseContinue, L"Reverse continue", s_kGlyphDrawn,       L"Run backward to the latest breakpoint or watchpoint",  DxuiToolbar::Kind::Command, 2, false, true  },
        { kShowNext,        L"Show next",        s_kGlyphShowNext,    L"Bring the code panes to the next statement",           DxuiToolbar::Kind::Command, 3, false, true  },
        { kTrace,           L"Trace",            s_kGlyphTrace,       L"Record every instruction the machine runs",            DxuiToolbar::Kind::Toggle,  3, true,  false },
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

        entry.command  = Find (row.id);
        entry.kind     = row.kind;
        entry.group    = row.group;
        entry.iconOnly = row.iconOnly;

        if (row.id != kTrace)
        {
            //  Drawn rather than a glyph, but through the strip's own icon
            //  path: hover and disabling are decided once, for every entry,
            //  and handed here as the box's ink and enabled state.
            int  id = row.id;

            entry.icon = [this, id] (IDxuiPainter & painter, const DxuiToolbarIconBox & icon)
            {
                PaintIcon (id, painter, icon, GetIconColors (m_handlers.isDark ? m_handlers.isDark() : true));
            };
        }

        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::GetIconColors
//
//  Visual Studio's green play triangle and blue step arrows, each lighter on
//  a dark ground and deeper on a light one so it keeps its contrast.
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
//  DebuggerCommands::PaintIcon
//
//  One entry's vector icon, in Visual Studio's shapes. Colored parts take the
//  entry's dimmed ink while it is disabled, as Visual Studio grays them.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintIcon (int id, IDxuiPainter & painter, const DxuiToolbarIconBox & icon, const IconColors & colors)
{
    uint32_t  run  = icon.enabled ? colors.run  : icon.ink;
    uint32_t  step = icon.enabled ? colors.step : icon.ink;



    switch (id)
    {
    case kRun:             PaintRun          (painter, icon, run);               break;
    case kPause:           PaintPause        (painter, icon, icon.ink);          break;
    case kStepInto:        PaintStepInto     (painter, icon, step);              break;
    case kStepOver:        PaintStepOver     (painter, icon, step);              break;
    case kStepOut:         PaintStepOut      (painter, icon, step);              break;
    case kRunToCursor:     PaintRunToCursor  (painter, icon, step, icon.ink);    break;
    case kShowNext:        PaintShowNext     (painter, icon, icon.ink);          break;
    case kStepBackInto:    PaintStepBackInto (painter, icon, step);              break;
    case kStepBackOver:    PaintStepBackOver (painter, icon, step);              break;
    case kStepBackOut:     PaintStepBackOut  (painter, icon, step);              break;
    case kReverseContinue: PaintReverseRun   (painter, icon, run);               break;
    default:                                                                     break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintRun
//
//  A filled triangle pointing right.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintRun (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    float  s  = icon.size;
    float  cy = icon.top + icon.rowH * 0.5f;
    float  l  = icon.x + s * 0.18f;
    float  r  = icon.x + s * 0.90f;



    painter.FillConvexQuad (l, cy - s * 0.42f, r, cy, r, cy, l, cy + s * 0.42f, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintPause
//
//  Two upright rounded bars.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintPause (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    float  s  = icon.size;
    float  cy = icon.top + icon.rowH * 0.5f;
    float  w  = s * 0.24f;
    float  h  = s * 0.80f;



    painter.FillRoundedRect (icon.x + s * 0.16f, cy - h * 0.5f, w, h, w * 0.5f, color);
    painter.FillRoundedRect (icon.x + s * 0.60f, cy - h * 0.5f, w, h, w * 0.5f, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintStepDot
//
//  The statement the step lands beside: a dot under the icon's middle.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintStepDot (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    float  s  = icon.size;
    float  cy = icon.top + icon.rowH * 0.5f;



    painter.FillCircle (icon.x + s * 0.5f, cy + s * 0.36f, s * 0.11f, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintArrowHead
//
//  Two strokes back from a tip, along the unit direction (dirX, dirY) the
//  arrow points.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintArrowHead (IDxuiPainter & painter, float tipX, float tipY, float dirX, float dirY, float length, float stroke, uint32_t color)
{
    float  backX = -dirX * length;
    float  backY = -dirY * length;



    painter.DrawLine (tipX, tipY, tipX + backX - backY, tipY + backY + backX, stroke, color);
    painter.DrawLine (tipX, tipY, tipX + backX + backY, tipY + backY - backX, stroke, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintStepInto
//
//  An arrow pointing down at the dot.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintStepInto (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    float  s      = icon.size;
    float  cx     = icon.x + s * 0.5f;
    float  cy     = icon.top + icon.rowH * 0.5f;
    float  stroke = (std::max) (1.5f, s / 7.5f);
    float  tipY   = cy + s * 0.12f;



    painter.DrawLine (cx, cy - s * 0.46f, cx, tipY, stroke, color);
    PaintArrowHead   (painter, cx, tipY, 0.0f, 1.0f, s * 0.24f, stroke, color);
    PaintStepDot     (painter, icon, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintStepOut
//
//  An arrow pointing up, away from the dot.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintStepOut (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    float  s      = icon.size;
    float  cx     = icon.x + s * 0.5f;
    float  cy     = icon.top + icon.rowH * 0.5f;
    float  stroke = (std::max) (1.5f, s / 7.5f);
    float  tipY   = cy - s * 0.46f;



    painter.DrawLine (cx, cy + s * 0.12f, cx, tipY, stroke, color);
    PaintArrowHead   (painter, cx, tipY, 0.0f, -1.0f, s * 0.24f, stroke, color);
    PaintStepDot     (painter, icon, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintStepOver
//
//  An arc from the left up over the dot, ending in an arrow pointing down on
//  the right.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintStepOver (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    static constexpr int    kSegments = 10;
    static constexpr float  kPi       = 3.14159265f;



    float  s      = icon.size;
    float  cx     = icon.x + s * 0.5f;
    float  cy     = icon.top + icon.rowH * 0.5f;
    float  stroke = (std::max) (1.5f, s / 7.5f);
    float  rx     = s * 0.36f;
    float  ry     = s * 0.42f;
    float  baseY  = cy - s * 0.04f;
    float  tipY   = cy + s * 0.20f;
    float  prevX  = cx - rx;
    float  prevY  = baseY;



    for (int i = 1; i <= kSegments; i++)
    {
        float  angle = kPi - kPi * (float) i / (float) kSegments;
        float  x     = cx + rx * std::cos (angle);
        float  y     = baseY - ry * std::sin (angle);

        painter.DrawLine (prevX, prevY, x, y, stroke, color);

        prevX = x;
        prevY = y;
    }

    painter.DrawLine (cx + rx, baseY, cx + rx, tipY, stroke, color);
    PaintArrowHead   (painter, cx + rx, tipY, 0.0f, 1.0f, s * 0.22f, stroke, color);
    PaintStepDot     (painter, icon, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintRunToCursor
//
//  Visual Studio's Run to Cursor: an arrow pointing right that ends at a
//  vertical bar.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintRunToCursor (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t arrow, uint32_t bar)
{
    float  s      = icon.size;
    float  cy     = icon.top + icon.rowH * 0.5f;
    float  stroke = (std::max) (1.5f, s / 7.5f);
    float  tipX   = icon.x + s * 0.72f;
    float  barX   = icon.x + s * 0.92f;



    painter.DrawLine (icon.x + s * 0.02f, cy, tipX, cy, stroke, arrow);
    PaintArrowHead   (painter, tipX, cy, 1.0f, 0.0f, s * 0.30f, stroke, arrow);
    painter.DrawLine (barX, cy - s * 0.42f, barX, cy + s * 0.42f, stroke, bar);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintShowNext
//
//  A plain arrow pointing right.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintShowNext (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    float  s      = icon.size;
    float  cy     = icon.top + icon.rowH * 0.5f;
    float  stroke = (std::max) (1.5f, s / 7.5f);
    float  tipX   = icon.x + s * 0.92f;



    painter.DrawLine (icon.x + s * 0.08f, cy, tipX, cy, stroke, color);
    PaintArrowHead   (painter, tipX, cy, 1.0f, 0.0f, s * 0.34f, stroke, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintBackDot
//
//  The statement a step back lands beside, as Visual Studio draws it: a dot
//  at the icon's top left.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintBackDot (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    float  s  = icon.size;
    float  cy = icon.top + icon.rowH * 0.5f;



    painter.FillCircle (icon.x + s * 0.20f, cy - s * 0.30f, s * 0.11f, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintStepBackInto
//
//  An arrow from the bottom right up to the dot at the top left.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintStepBackInto (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    constexpr float  kDiagonal = 0.70710678f;
    float            s         = icon.size;
    float            cy        = icon.top + icon.rowH * 0.5f;
    float            stroke    = (std::max) (1.5f, s / 7.5f);
    float            tipX      = icon.x + s * 0.40f;
    float            tipY      = cy - s * 0.10f;



    painter.DrawLine (icon.x + s * 0.88f, cy + s * 0.38f, tipX, tipY, stroke, color);
    PaintArrowHead   (painter, tipX, tipY, -kDiagonal, -kDiagonal, s * 0.24f, stroke, color);
    PaintBackDot     (painter, icon, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintStepBackOut
//
//  An arrow from beside the dot at the top left down to the bottom right.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintStepBackOut (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    constexpr float  kDiagonal = 0.70710678f;
    float            s         = icon.size;
    float            cy        = icon.top + icon.rowH * 0.5f;
    float            stroke    = (std::max) (1.5f, s / 7.5f);
    float            tipX      = icon.x + s * 0.88f;
    float            tipY      = cy + s * 0.38f;



    painter.DrawLine (icon.x + s * 0.40f, cy - s * 0.10f, tipX, tipY, stroke, color);
    PaintArrowHead   (painter, tipX, tipY, kDiagonal, kDiagonal, s * 0.24f, stroke, color);
    PaintBackDot     (painter, icon, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintStepBackOver
//
//  Step over mirrored: an arc from the right up over the dot, ending in an
//  arrow pointing down on the left.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintStepBackOver (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    static constexpr int    kSegments = 10;
    static constexpr float  kPi       = 3.14159265f;



    float  s      = icon.size;
    float  cx     = icon.x + s * 0.5f;
    float  cy     = icon.top + icon.rowH * 0.5f;
    float  stroke = (std::max) (1.5f, s / 7.5f);
    float  rx     = s * 0.36f;
    float  ry     = s * 0.42f;
    float  baseY  = cy - s * 0.04f;
    float  tipY   = cy + s * 0.20f;
    float  prevX  = cx + rx;
    float  prevY  = baseY;



    for (int i = 1; i <= kSegments; i++)
    {
        float  angle = kPi * (float) i / (float) kSegments;
        float  x     = cx + rx * std::cos (angle);
        float  y     = baseY - ry * std::sin (angle);

        painter.DrawLine (prevX, prevY, x, y, stroke, color);

        prevX = x;
        prevY = y;
    }

    painter.DrawLine (cx - rx, baseY, cx - rx, tipY, stroke, color);
    PaintArrowHead   (painter, cx - rx, tipY, 0.0f, 1.0f, s * 0.22f, stroke, color);
    PaintStepDot     (painter, icon, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommands::PaintReverseRun
//
//  Run mirrored: a filled triangle pointing left.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerCommands::PaintReverseRun (IDxuiPainter & painter, const DxuiToolbarIconBox & icon, uint32_t color)
{
    float  s  = icon.size;
    float  cy = icon.top + icon.rowH * 0.5f;
    float  l  = icon.x + s * 0.10f;
    float  r  = icon.x + s * 0.82f;



    painter.FillConvexQuad (r, cy - s * 0.42f, l, cy, l, cy, r, cy + s * 0.42f, color);
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

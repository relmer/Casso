#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/DroppedFiles.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/Source/SourcePathList.h"
#include "Config/WindowPlacementProfile.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/FlagsDialog.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Debugger/SymbolDescriptions.h"
#include "Cassque/CassquePromptDialog.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/Panes/TracePaneController.h"
#include "Ui/Debugger/MemoryBarCommands.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Ui/Debugger/MemoryAddressEntry.h"
#include "Ui/Debugger/Panes/BasicProgramController.h"
#include "Ui/Debugger/Panes/BreakpointsPane.h"
#include "Ui/Debugger/Panes/CodeEditCommands.h"
#include "Ui/Debugger/Panes/ConsolePanes.h"
#include "Ui/Debugger/ViewMenuRows.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Ui/Debugger/Panes/FindWidgets.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/ToolPanes.h"
#include "Ui/Debugger/Panes/HeatMapPane.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/Panes/MemoryWindows.h"
#include "Ui/Debugger/Panes/OperandTipController.h"
#include "Ui/Debugger/Panes/UndoListPanes.h"
#include "Ui/Debugger/Panes/WatchesPane.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/Panes/FlameGraphView.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/ProfileTimeline.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/ColorKeyButton.h"
#include "Ui/Debugger/HeatMapBarCommands.h"
#include "Ui/Debugger/HeatMapRangeBarCommands.h"
#include "Ui/Debugger/ToolbarLabelEntry.h"
#include "Ui/Debugger/ToolbarCheckEntry.h"
#include "Ui/Debugger/UndoBarCommands.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyTheme
//
//  The window's own theme, or the emulator's for the empty name. The system
//  themes take the accent from Windows each time one is chosen, as Casso
//  Explorer's do, and then the debugger's own tooltip and pane colors. A
//  test gives the system colors in place of Windows', so nothing is read
//  from the machine. The Theme drop-down is rebuilt for the check it holds.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyTheme (const std::string & name)
{
    const DxuiWindowsThemeColors::SystemColors &  system = m_systemColorsForTest.has_value() ? *m_systemColorsForTest
                                                                                            : DxuiWindowsThemeColors::Instance().GetSystemColors();



    m_themeName = name;

    m_lightTheme.ApplySystemColors (system);
    m_darkTheme.ApplySystemColors  (system);

    DebuggerThemes::ApplyOwnColors (m_lightTheme, m_darkTheme);

    if (m_emulatorTheme != nullptr)
    {
        m_theme = &DebuggerThemes::Choose (name, *m_emulatorTheme, m_lightTheme, m_darkTheme, m_ownTheme);
    }

    SetTheme            (m_theme);
    m_focusMgr.SetTheme (m_theme);

    if (m_theme != nullptr)
    {
        m_tooltip.SetTheme (*m_theme);
    }

    m_floating->ApplyTheme (m_theme);

    if (m_barHost.GetFloatWindow() != nullptr)
    {
        m_barHost.GetFloatWindow()->SetTheme (m_theme);
    }

    if (m_timelineHost.GetFloatWindow() != nullptr)
    {
        m_timelineHost.GetFloatWindow()->SetTheme (m_theme);
    }

    if (m_commandBar != nullptr)
    {
        SetWindowMenus();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetColorPalette
//
//  Every color the legend shows, as the panes draw them in the theme in
//  force.
//
////////////////////////////////////////////////////////////////////////////////

ColorLegend::Palette DebuggerWindow::GetColorPalette() const
{
    ColorLegend::Palette  palette;



    palette.text       = GetTextColors();
    palette.background = (m_theme != nullptr) ? m_theme->ContentBackground()  : 0xFF000000;
    palette.muted      = (m_theme != nullptr) ? m_theme->ForegroundMuted()    : 0xFF808080;
    palette.disabled   = (m_theme != nullptr) ? m_theme->ForegroundDisabled() : 0xFF808080;
    palette.accent     = (m_theme != nullptr) ? m_theme->Accent()             : 0xFF3C8CE6;
    palette.flash      = (m_theme != nullptr) ? DiskHeadView::GetFlashColor (*m_theme) : 0xFFFFFFFF;
    palette.pcMarker   = GetPcMarkerArgb();
    palette.breakpoint = GetBreakpointArgb();
    palette.meterEmpty = DebuggerStatusText::kEmptyArgb;
    palette.meterFull  = DebuggerStatusText::kFullArgb;

    return palette;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsDarkTheme
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsDarkTheme() const
{
    uint32_t  bg    = (m_theme != nullptr) ? m_theme->ContentBackground() : 0xFF000000;
    uint32_t  luma  = ((bg >> 16) & 0xFF) * 299 + ((bg >> 8) & 0xFF) * 587 + (bg & 0xFF) * 114;



    return luma < 128 * 1000;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBreakpointArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetBreakpointArgb() const
{
    //  A warm red that holds its own against a blue ground, as Visual Studio
    //  Code's breakpoint red does; a pure red goes purple beside blue.
    return IsDarkTheme() ? 0xFFF4524D : 0xFFD1242F;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBreakpointIcon
//
//  The breakpoint's dot, filled when enabled and a ring when not. Rebuilt
//  when the theme changes the color.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DebuggerWindow::GetBreakpointIcon (bool enabled)
{
    uint32_t  argb = GetBreakpointArgb();



    if (argb != m_breakpointIconArgb || m_breakpointIcons[0] == nullptr)
    {
        m_breakpointIconArgb = argb;
        m_breakpointIcons[0] = GutterGlyph::MakeDot (argb, false);
        m_breakpointIcons[1] = GutterGlyph::MakeDot (argb, true);
    }

    return m_breakpointIcons[enabled ? 1 : 0];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHoverBreakpointIcon
//
//  The gray dot under the pointer in the breakpoint column, in the theme's
//  muted text color, where a click would set a breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DebuggerWindow::GetHoverBreakpointIcon()
{
    uint32_t  argb = (m_theme != nullptr) ? m_theme->ForegroundMuted() : 0xFF808080;



    if (argb != m_hoverBreakpointArgb || m_hoverBreakpointIcon == nullptr)
    {
        m_hoverBreakpointArgb = argb;
        m_hoverBreakpointIcon = GutterGlyph::MakeDot (argb, true);
    }

    return m_hoverBreakpointIcon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPcIcon
//
//  The PC's arrow in the glyph margin, over the line's dot when it has one,
//  as Visual Studio draws its arrow over a breakpoint. A dot not seen before
//  gets its own image, and a few are kept: the enabled and disabled dots,
//  the gray one under the pointer, and the arrow alone; one more means a
//  dot from an earlier theme, so the old ones are dropped.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DebuggerWindow::GetPcIcon (const std::shared_ptr<const DxuiIconImage> & dot)
{
    constexpr size_t                      kMaxPcIcons = 4;
    uint32_t                              argb        = GetPcMarkerArgb();
    std::shared_ptr<const DxuiIconImage>  icon;



    if (argb != m_pcIconArgb)
    {
        m_pcIconArgb = argb;
        m_pcIcons.clear();
    }

    for (const PcIcon & each : m_pcIcons)
    {
        icon = (each.dot == dot) ? each.icon : icon;
    }

    if (icon == nullptr)
    {
        if (m_pcIcons.size() >= kMaxPcIcons)
        {
            m_pcIcons.clear();
        }

        icon = GutterGlyph::MakeMarked (dot, GutterGlyph::MakeArrow (argb));

        m_pcIcons.push_back ({ dot, icon });
    }

    return icon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPcMarkerArgb
//
//  Visual Studio's yellow, which reads on either background.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetPcMarkerArgb() const
{
    return DisassemblyCells::GetPcMarkerArgb ((m_theme != nullptr) ? m_theme->ContentBackground() : 0xFF000000);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPcRowArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetPcRowArgb() const
{
    return GetTextColors().pcRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetNavigatedRowArgb
//
//  Visual Studio's green for a frame a call stack brought into view.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetNavigatedRowArgb() const
{
    return GetTextColors().navigatedRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTargetRowArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetTargetRowArgb() const
{
    return GetTextColors().targetRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTextColors
//
//  Every text color the panes draw, made readable against the theme's
//  content background, with every color of the theme's own: its ROM, I/O and
//  memory map colors among them.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerTextColors::Set DebuggerWindow::GetTextColors() const
{
    if (m_theme != nullptr)
    {
        return m_textColors.GetFor (*m_theme);
    }

    return DebuggerTextColors::Make (0xFF000000, 0xFFFFFFFF, 0xFFC0C0C0, 0u);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetSyntaxColors
//
//  Visual Studio's code colors for the theme's darkness; see DebuggerTextColors.
//
////////////////////////////////////////////////////////////////////////////////

SourceSyntax::Colors DebuggerWindow::GetSyntaxColors() const
{
    return GetTextColors().syntax;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetResultArgb
//
//  The theme's color for an operand's result; a theme that gives none gets a
//  cyan for its darkness.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetResultArgb() const
{
    return GetTextColors().result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetAnnotationArgb
//
//  The comment green of Visual Studio's editor.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetAnnotationArgb() const
{
    return GetTextColors().annotation;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetChangedArgb
//
//  Visual Studio's red for a value that changed since the last stop.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetChangedArgb() const
{
    return GetTextColors().changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHeatPalette
//
//  Code in the disassembly's instruction color, reads in the annotation
//  green and writes in the changed red, from the theme's cold gray, over the
//  page the panes are drawn on. The PC is outlined in the disassembly's PC
//  yellow and a breakpoint in its breakpoint red, and a symbol tints toward
//  the symbol color; the rest of the overlays take the theme's heat colors.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapView::Palette DebuggerWindow::GetHeatPalette() const
{
    DebuggerTextColors::Set  colors  = GetTextColors();
    HeatMapView::Palette     palette;



    palette.background    = (m_theme != nullptr) ? m_theme->ContentBackground() : 0xFF000000;
    palette.cold          = colors.heatCold;
    palette.execute       = colors.syntax.mnemonic;
    palette.read          = colors.annotation;
    palette.write         = colors.changed;
    palette.pc            = GetPcMarkerArgb();
    palette.stack         = colors.heatStack;
    palette.breakpoint    = GetBreakpointArgb();
    palette.readWatch     = colors.heatReadWatch;
    palette.writeWatch    = colors.heatWriteWatch;
    palette.symbol        = colors.syntax.symbol;
    palette.selfModifying = colors.heatSelfModifying;
    palette.unwritten     = colors.heatUnwritten;
    palette.changed       = colors.heatChanged;

    return palette;
}

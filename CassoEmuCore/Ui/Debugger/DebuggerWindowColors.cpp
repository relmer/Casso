#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "AssetBootstrap.h"
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
#include "CassoExplorer/CassoExplorerPromptDialog.h"
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
//  DebuggerWindow::ApplyTypography
//
//  Attached to the window, so its floating windows, menus and tips draw in
//  it too.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyTypography()
{
    HRESULT  hr = S_OK;



    if (m_look.GetTheme() == m_emulatorTheme)
    {
        m_typographyInForce = &DxuiTextRenderer::GetDefaultTypography();
        DxuiTextRenderer::AttachTypography (GetHwnd(), nullptr);
        return;
    }

    if (m_look.GetTheme() == &m_ownTheme)
    {
        hr = AssetBootstrap::ApplyThemeTypography (m_hInstance, m_ownTheme, m_typography);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
    else
    {
        DxuiTextRenderer::ClearBodyFace             (m_typography);
        DxuiTextRenderer::SetTextGlow               (m_typography, 0, 0.0f);
        DxuiTextRenderer::SetMonochromeGlyphsForced (m_typography, false);
    }

    m_typographyInForce = &m_typography;
    DxuiTextRenderer::AttachTypography (GetHwnd(), &m_typography);
}





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
//  The window's text is drawn in its theme's typography: the emulator's,
//  which is the process default, when it follows the emulator, and the
//  window's otherwise. The system themes draw in the system face.
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
        m_look.SetTheme (&DebuggerThemes::Choose (name, *m_emulatorTheme, m_lightTheme, m_darkTheme, m_ownTheme));
    }

    ApplyTypography();

    //  A theme's face can be wider than the last one's, so the columns are
    //  fitted again in it, or the widest text would be cut.
    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        for (DxuiListView * list : pane->GetLists())
        {
            list->ResetAutoFit();
        }
    }

    SetTheme            (m_look.GetTheme());
    m_focusMgr.SetTheme (m_look.GetTheme());

    if (m_look.GetTheme() != nullptr)
    {
        m_tooltip.SetTheme (*m_look.GetTheme());
    }

    m_floating->ApplyTheme (m_look.GetTheme());

    if (m_barHost.GetFloatWindow() != nullptr)
    {
        m_barHost.GetFloatWindow()->SetTheme (m_look.GetTheme());
    }

    if (m_timelineHost.GetFloatWindow() != nullptr)
    {
        m_timelineHost.GetFloatWindow()->SetTheme (m_look.GetTheme());
    }

    if (m_commandBar != nullptr)
    {
        SetWindowMenus();
    }

    //  The code views, the source documents, the heat map and the memory
    //  windows take their colors at a snapshot, and no snapshot comes when a
    //  theme is chosen while the machine is stopped: their rows had the last
    //  theme's colors.
    if (m_heatMapPane != nullptr)
    {
        m_heatMapPane->ApplyPalette();
    }

    if (m_memoryGroup != nullptr)
    {
        m_memoryGroup->ApplyColors();
    }

    if (m_snapshot != nullptr && m_code != nullptr && m_sources != nullptr)
    {
        m_code->Apply();
        m_sources->Apply();
    }

    Invalidate();
}






#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/ProjectSettingsDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetProjectFileSpec
//
//  The file picker's filter for debug projects.
//
////////////////////////////////////////////////////////////////////////////////

static FileDialogSpec GetProjectFileSpec()
{
    return { { { L"Casso debug projects", L"*.cassoproj" }, { L"All files", L"*.*" } }, L"cassoproj" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveProjectAs
//
//  A copy of the open project in a file the user picks, which becomes the
//  project's file (FR-170).
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveProjectAs()
{
    HRESULT                hr     = S_OK;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    hr = m_host->GetHostDialogs().PickFileToSave (GetHwnd(), GetProjectFileSpec(), chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    m_host->RunDebuggerCommandInMode (std::format ("PROJECT SAVEAS \"{}\"", TextEncoding::WideToNarrow (chosen.wstring())), CommandMode::AppleWin);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenProject
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenProject()
{
    HRESULT                hr     = S_OK;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    hr = m_host->GetHostDialogs().PickFileToOpen (GetHwnd(), GetProjectFileSpec(), chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    m_host->RunDebuggerCommandInMode (std::format ("PROJECT OPEN \"{}\"", TextEncoding::WideToNarrow (chosen.wstring())), CommandMode::AppleWin);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EditProjectSettings
//
//  The open project's startup commands (FR-171), on a dialog of rows until
//  the editable grid exists.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EditProjectSettings()
{
    DebugProject                                           project;
    std::optional<ProjectSettingsDialog::StartupCommands>  commands;



    if (m_host == nullptr || !m_host->TryGetDebuggerProject (project))
    {
        return;
    }

    commands = ProjectSettingsDialog::RunModal (GetHwnd(), m_theme, project);

    if (commands.has_value())
    {
        m_host->SetDebuggerStartupCommands (*commands);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ReloadProjectState
//
//  Another project opened and its strings are the host's now: the panes
//  laid out as it has them, its disassembly options, its heat map ranges,
//  and its open views opened.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ReloadProjectState()
{
    std::wstring    savedText = ReadSavedLayout();
    DxuiPaneLayout  restored  = DebuggerLayout::Restore (savedText);



    restored.PlaceOnMonitors (GetMonitors());
    m_dockSite->SetPaneLayout (restored);
    m_syncFloats = true;

    m_codeOptions = DisassemblyOptions::FromText (m_host->GetDebuggerDisassemblyOptions());

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews && m_snapshot != nullptr; view++)
    {
        ApplyCodeView (view);
    }

    m_isHeatRangesOpened = !m_host->GetDebuggerHeatMapRanges().empty();
    m_heatRanges         = HeatMapRangeSets::FromText (m_host->GetDebuggerHeatMapRanges());
    (void) m_heatRanges.TryAddDefaultSet();
    m_heatRangeSet       = (m_heatRanges.FindSet (m_heatRangeSet) != nullptr || m_heatRanges.sets.empty()) ? m_heatRangeSet : m_heatRanges.sets.front().name;

    ApplyHeatRanges();
    SetHeatRangeMenus();

    m_openViewsRestored = false;
    Invalidate();
}

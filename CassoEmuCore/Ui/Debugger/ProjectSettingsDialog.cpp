#include "Pch.h"

#include "Ui/Debugger/ProjectSettingsDialog.h"
#include "Debugger/Source/SourcePathList.h"



//  The mode list, in CommandMode's order.
static constexpr const wchar_t *  s_kModeLabels[] = { L"AppleWin", L"Monitor", L"GSSquared", L"WinDbg", L"Casso" };





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void ProjectSettingsPanel::Init (DxuiLabel & scope, DxuiLabel & file, DxuiLabel & heading, std::span<DxuiComboBox> modes, std::span<DxuiTextInput> lines)
{
    m_scope   = &scope;
    m_file    = &file;
    m_heading = &heading;
    m_modes   = modes;
    m_lines   = lines;

    Adopt (scope);
    Adopt (file);
    Adopt (heading);

    for (size_t i = 0; i < m_modes.size(); i++)
    {
        Adopt (m_modes[i]);
        Adopt (m_lines[i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ProjectSettingsPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  row   = scaler.ToPx (kRowDip);
    int  gap   = scaler.ToPx (kGapDip);
    int  split = boundsDip.left + scaler.ToPx (kModeDip);
    int  y     = boundsDip.top;



    SetBounds (boundsDip);

    for (DxuiLabel * label : { m_scope, m_file, m_heading })
    {
        label->Layout (RECT { boundsDip.left, y, boundsDip.right, y + row }, scaler);
        y += row;
    }

    for (size_t i = 0; i < m_modes.size(); i++)
    {
        m_modes[i].Layout (RECT { boundsDip.left, y, split - gap, y + row }, scaler);
        m_lines[i].Layout (RECT { split, y, boundsDip.right, y + row }, scaler);
        y += row + gap;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::GetCommands
//
////////////////////////////////////////////////////////////////////////////////

ProjectSettingsDialog::StartupCommands ProjectSettingsDialog::GetCommands (const std::array<CommandMode, kRows> & modes, const std::array<std::string, kRows> & lines)
{
    StartupCommands  commands;



    for (size_t i = 0; i < kRows; i++)
    {
        size_t  first = lines[i].find_first_not_of (" \t");
        size_t  last  = lines[i].find_last_not_of (" \t");



        if (first == std::string::npos)
        {
            continue;
        }

        commands.push_back ({ lines[i].substr (first, last - first + 1), modes[i] });
    }

    return commands;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::OnCreate
//
//  OK reads the rows before the dialog ends.
//
////////////////////////////////////////////////////////////////////////////////

void ProjectSettingsDialog::OnCreate()
{
    DxuiButton  * ok = nullptr;



    for (DxuiLabel * label : { &m_scope, &m_file, &m_heading })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_scope.SetText   (L"Debug project for " + SourcePathList::Utf8ToWide (m_project.scope.label));
    m_file.SetText    (m_project.filePath);
    m_heading.SetText (L"Startup commands, run in order when the project opens:");

    for (size_t i = 0; i < kRows; i++)
    {
        bool  isUsed = i < m_project.startupCommands.size();



        m_modes[i].SetItems     (std::vector<std::wstring> (std::begin (s_kModeLabels), std::end (s_kModeLabels)));
        m_modes[i].SetSelected  (isUsed ? (int) m_project.startupCommands[i].mode : (int) CommandMode::AppleWin);
        m_modes[i].SetPopupHost (GetPopupHost());

        m_lines[i].SetTheme        (m_theme);
        m_lines[i].SetHwnd         (GetHwnd());
        m_lines[i].SetTextRenderer (GetTextRenderer());
        m_lines[i].SetText         (isUsed ? SourcePathList::Utf8ToWide (m_project.startupCommands[i].text) : std::wstring());
    }

    m_body = CreateDialogContent<ProjectSettingsPanel>();
    m_body->Init (m_scope, m_file, m_heading, m_modes, m_lines);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        std::array<CommandMode, kRows>  modes;
        std::array<std::string, kRows>  lines;



        for (size_t i = 0; i < kRows; i++)
        {
            modes[i] = (CommandMode) m_modes[i].GetSelectedIndex();
            lines[i] = SourcePathList::WideToUtf8 (m_lines[i].GetText());
        }

        m_result = GetCommands (modes, lines);
        EndDialog (IDOK);
    });

    SetInitialFocus (&m_lines[0]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::RunModal
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ProjectSettingsDialog::StartupCommands> ProjectSettingsDialog::RunModal (HWND owner, const IDxuiTheme * theme, const DebugProject & project)
{
    HRESULT                   hr     = S_OK;
    ProjectSettingsDialog     dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme   = theme;
    dialog.m_project = project;

    params.title                    = L"Project settings";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 560, 470 };
    params.minSizeDip               = { 480, 470 };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return std::nullopt;
    }

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

    return dialog.m_result;
}

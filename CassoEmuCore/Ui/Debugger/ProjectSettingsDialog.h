#pragma once

#include "Debugger/Project/DebugProject.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsPanel
//
//  The project's scope and file at the top, then a row a startup command:
//  the mode it runs in beside its text.
//
////////////////////////////////////////////////////////////////////////////////

class ProjectSettingsPanel : public DxuiPanel
{
public:
    void  Init   (DxuiLabel & scope, DxuiLabel & file, DxuiLabel & heading, std::span<DxuiComboBox> modes, std::span<DxuiTextInput> lines);
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    static constexpr int  kRowDip  = 30;
    static constexpr int  kGapDip  = 6;
    static constexpr int  kModeDip = 120;

private:
    DxuiLabel                  * m_scope   = nullptr;
    DxuiLabel                  * m_file    = nullptr;
    DxuiLabel                  * m_heading = nullptr;
    std::span<DxuiComboBox>      m_modes;
    std::span<DxuiTextInput>     m_lines;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog
//
//  Edits the open debug project's startup commands, which run in order,
//  each in its mode, when the project opens (FR-171). A row left empty is
//  dropped.
//
////////////////////////////////////////////////////////////////////////////////

class ProjectSettingsDialog : public DxuiDialogWindow
{
public:
    static constexpr size_t  kRows = 8;

    using StartupCommands = std::vector<DebugProject::StartupCommand>;

    //  The rows' commands in order, the empty ones left out.
    static StartupCommands  GetCommands (const std::array<CommandMode, kRows> & modes, const std::array<std::string, kRows> & lines);

    //  Runs the dialog modally over `owner`; the commands when OK.
    static std::optional<StartupCommands>  RunModal (HWND owner, const IDxuiTheme * theme, const DebugProject & project);

protected:
    void  OnCreate() override;

private:
    const IDxuiTheme                         * m_theme   = nullptr;
    DebugProject                               m_project;
    std::optional<StartupCommands>             m_result;
    DxuiLabel                                  m_scope;
    DxuiLabel                                  m_file;
    DxuiLabel                                  m_heading;
    std::array<DxuiComboBox,  kRows>           m_modes;
    std::array<DxuiTextInput, kRows>           m_lines;
    ProjectSettingsPanel                     * m_body    = nullptr;
};

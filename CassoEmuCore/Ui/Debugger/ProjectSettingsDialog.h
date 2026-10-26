#pragma once

#include "Debugger/Project/DebugProject.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsPanel
//
//  The project's scope and file at the top, then the startup commands on an
//  editable grid, each command's text beside its mode, and a line under the
//  grid with the reason an edit failed. The grid takes coordinates relative
//  to itself, so the panel moves each mouse event into them.
//
////////////////////////////////////////////////////////////////////////////////

class ProjectSettingsPanel : public DxuiPanel
{
public:
    using PressFn = std::function<void (const DxuiMouseEvent & ev, int row)>;

    void     Init              (DxuiLabel & scope, DxuiLabel & file, DxuiLabel & heading, DxuiEditableGrid & grid, DxuiLabel & error);
    void     SetOnGridPressed  (PressFn fn) { m_pressed = std::move (fn); }
    void     Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

    static constexpr int  kRowDip = 30;
    static constexpr int  kGapDip = 6;

private:
    DxuiLabel         * m_scope   = nullptr;
    DxuiLabel         * m_file    = nullptr;
    DxuiLabel         * m_heading = nullptr;
    DxuiEditableGrid  * m_grid    = nullptr;
    DxuiLabel         * m_error   = nullptr;
    PressFn             m_pressed;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog
//
//  Edits the open debug project's startup commands, which run in order,
//  each in its mode, when the project opens. A command is added on the
//  grid's last row, edited in place, dragged to another place by its
//  gripper and deleted with Delete or its row's menu.
//
////////////////////////////////////////////////////////////////////////////////

class ProjectSettingsDialog : public DxuiDialogWindow
{
public:
    using StartupCommands = std::vector<DebugProject::StartupCommand>;

    enum Column { kCommandColumn, kModeColumn };

    //  A mode by its label, in any case; an empty one is AppleWin.
    static bool  TryParseMode   (const std::wstring & text, CommandMode & mode);

    //  A command from a row's text and mode, its text trimmed; false, with
    //  the reason, for no text or no such mode.
    static bool  TryMakeCommand (const std::wstring & line, const std::wstring & mode, DebugProject::StartupCommand & command, std::wstring & error);

    //  Runs the dialog modally over `owner`; the commands when OK.
    static std::optional<StartupCommands>  RunModal (HWND owner, const IDxuiTheme * theme, const DebugProject & project);

protected:
    void  OnCreate() override;

    //  The commands OK took, none until OK closes the dialog.
    const std::optional<StartupCommands> &  GetResult() const { return m_result; }

private:
    void  ConfigureGrid  ();
    void  ApplyRows      ();
    bool  CommitCell     (int row, int column, const std::wstring & text, std::wstring & outError);
    bool  AddCommand     (const std::vector<std::wstring> & texts, std::wstring & outError);
    void  MoveCommand    (int from, int to);
    void  ShowRowMenu    (const DxuiMouseEvent & ev, int row);

    const IDxuiTheme              * m_theme   = nullptr;
    DebugProject                    m_project;
    StartupCommands                 m_commands;
    std::optional<StartupCommands>  m_result;
    DxuiLabel                       m_scope;
    DxuiLabel                       m_file;
    DxuiLabel                       m_heading;
    DxuiEditableGrid                m_grid;
    DxuiLabel                       m_error;
    std::shared_ptr<DxuiCommand>    m_deleteCommand;
    ProjectSettingsPanel          * m_body    = nullptr;
};

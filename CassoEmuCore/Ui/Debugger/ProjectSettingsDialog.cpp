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

void ProjectSettingsPanel::Init (DxuiLabel & scope, DxuiLabel & file, DxuiLabel & heading, DxuiEditableGrid & grid, DxuiLabel & error)
{
    m_scope   = &scope;
    m_file    = &file;
    m_heading = &heading;
    m_grid    = &grid;
    m_error   = &error;

    Adopt (scope);
    Adopt (file);
    Adopt (heading);
    Adopt (grid);
    Adopt (error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsPanel::Layout
//
//  The labels a row each, the error line at the bottom, and the grid
//  between them.
//
////////////////////////////////////////////////////////////////////////////////

void ProjectSettingsPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  row = scaler.ToPx (kRowDip);
    int  gap = scaler.ToPx (kGapDip);
    int  y   = boundsDip.top;



    SetBounds (boundsDip);

    for (DxuiLabel * label : { m_scope, m_file, m_heading })
    {
        label->Layout (RECT { boundsDip.left, y, boundsDip.right, y + row }, scaler);
        y += row;
    }

    m_grid->Layout  (RECT { boundsDip.left, y, boundsDip.right, boundsDip.bottom - row - gap }, scaler);
    m_error->Layout (RECT { boundsDip.left, boundsDip.bottom - row, boundsDip.right, boundsDip.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsPanel::OnMouse
//
//  Only the grid takes the pointer: over it, or anywhere while it drags or
//  edits, so a press elsewhere commits its edit. A press over it is passed
//  on first, with the row under it, for the focus and a row's menu.
//
////////////////////////////////////////////////////////////////////////////////

bool ProjectSettingsPanel::OnMouse (const DxuiMouseEvent & ev)
{
    RECT            bounds = m_grid->GetBounds();
    DxuiMouseEvent  local  = ev;
    bool            isOver = PtInRect (&bounds, ev.positionDip) != FALSE;



    local.positionDip = { ev.positionDip.x - bounds.left, ev.positionDip.y - bounds.top };

    if (!isOver && !m_grid->IsInteracting() && !m_grid->IsEditing())
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && isOver && m_pressed)
    {
        m_pressed (ev, m_grid->HitTestRow (local.positionDip.x, local.positionDip.y));
    }

    return m_grid->OnMouse (local) || isOver;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsPanel::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ProjectSettingsPanel::GetCursorForPoint (POINT clientPx) const
{
    RECT  bounds = m_grid->GetBounds();



    if (!PtInRect (&bounds, clientPx) && !m_grid->IsDraggingRow())
    {
        return nullptr;
    }

    return m_grid->GetCursorForPoint (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::TryParseMode
//
////////////////////////////////////////////////////////////////////////////////

bool ProjectSettingsDialog::TryParseMode (const std::wstring & text, CommandMode & mode)
{
    size_t  first = text.find_first_not_of (L" \t");
    size_t  last  = text.find_last_not_of (L" \t");



    if (first == std::wstring::npos)
    {
        mode = CommandMode::AppleWin;
        return true;
    }

    for (size_t i = 0; i < std::size (s_kModeLabels); i++)
    {
        if (_wcsicmp (text.substr (first, last - first + 1).c_str(), s_kModeLabels[i]) == 0)
        {
            mode = (CommandMode) i;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::TryMakeCommand
//
////////////////////////////////////////////////////////////////////////////////

bool ProjectSettingsDialog::TryMakeCommand (const std::wstring & line, const std::wstring & mode, DebugProject::StartupCommand & command, std::wstring & error)
{
    size_t  first = line.find_first_not_of (L" \t");
    size_t  last  = line.find_last_not_of (L" \t");



    if (first == std::wstring::npos)
    {
        error = L"A startup command has no text.";
        return false;
    }

    if (!TryParseMode (mode, command.mode))
    {
        error = L"The modes are AppleWin, Monitor, GSSquared, WinDbg and Casso.";
        return false;
    }

    command.text = SourcePathList::WideToUtf8 (line.substr (first, last - first + 1));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::OnCreate
//
//  OK takes the commands as the grid has them.
//
////////////////////////////////////////////////////////////////////////////////

void ProjectSettingsDialog::OnCreate()
{
    DxuiButton  * ok = nullptr;



    for (DxuiLabel * label : { &m_scope, &m_file, &m_heading, &m_error })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_error.SetTextRole (DxuiTextRole::Error);

    m_scope.SetText   (L"Debug project for " + SourcePathList::Utf8ToWide (m_project.scope.label));
    m_file.SetText    (m_project.filePath);
    m_heading.SetText (L"Startup commands, run in order when the project opens:");

    m_commands = m_project.startupCommands;
    ConfigureGrid();

    m_body = CreateDialogContent<ProjectSettingsPanel>();
    m_body->Init (m_scope, m_file, m_heading, m_grid, m_error);
    m_body->SetOnGridPressed ([this] (const DxuiMouseEvent & ev, int row)
    {
        FocusControl (&m_grid);

        if (ev.button == DxuiMouseButton::Right)
        {
            ShowRowMenu (ev, row);
        }
    });

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    //  An edit open when OK is pressed is committed first. When that fails
    //  the dialog stays open with the edit's error.
    ok->SetOnClick ([this]()
    {
        if (!m_grid.EndEdit (true))
        {
            return;
        }

        m_result = m_commands;
        EndDialog (IDOK);
    });

    SetInitialFocus (&m_grid);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::ConfigureGrid
//
////////////////////////////////////////////////////////////////////////////////

void ProjectSettingsDialog::ConfigureGrid()
{
    constexpr int  kCommandDip = 300;
    constexpr int  kModeDip    = 110;



    m_grid.SetTheme       (m_theme);
    m_grid.SetShowHeader  (true);
    m_grid.SetEditorHwnd  (GetHwnd());
    m_grid.SetGridColumns ({ { { L"Command", kCommandDip }, true, true }, { { L"Mode", kModeDip } } });
    m_grid.SetAddRowText  (L"Add startup command");

    m_grid.SetOnCommitCell ([this] (int row, int column, const std::wstring & text, std::wstring & outError) { return CommitCell (row, column, text, outError); });
    m_grid.SetOnAddRow     ([this] (const std::vector<std::wstring> & texts, std::wstring & outError) { return AddCommand (texts, outError); });
    m_grid.SetOnMoveRow    ([this] (int from, int to) { MoveCommand (from, to); });
    m_grid.SetOnError      ([this] (const std::wstring & text) { m_error.SetText (text); Invalidate(); });
    m_grid.SetOnDeleteRow  ([this] (int row)
    {
        m_commands.erase (m_commands.begin() + row);
        ApplyRows();
    });

    ApplyRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::ApplyRows
//
////////////////////////////////////////////////////////////////////////////////

void ProjectSettingsDialog::ApplyRows()
{
    std::vector<DxuiEditableGrid::GridRow>  rows;



    for (const DebugProject::StartupCommand & command : m_commands)
    {
        rows.push_back ({ { { SourcePathList::Utf8ToWide (command.text) }, { s_kModeLabels[(size_t) command.mode] } } });
    }

    m_grid.SetGridRows (std::move (rows));
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::CommitCell
//
////////////////////////////////////////////////////////////////////////////////

bool ProjectSettingsDialog::CommitCell (int row, int column, const std::wstring & text, std::wstring & outError)
{
    DebugProject::StartupCommand  & command = m_commands[(size_t) row];
    DebugProject::StartupCommand    made;



    if (!TryMakeCommand ((column == kCommandColumn) ? text : SourcePathList::Utf8ToWide (command.text),
                         (column == kModeColumn)    ? text : std::wstring (s_kModeLabels[(size_t) command.mode]), made, outError))
    {
        return false;
    }

    command = made;
    ApplyRows();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::AddCommand
//
////////////////////////////////////////////////////////////////////////////////

bool ProjectSettingsDialog::AddCommand (const std::vector<std::wstring> & texts, std::wstring & outError)
{
    DebugProject::StartupCommand  command;



    if (!TryMakeCommand (texts[kCommandColumn], texts[kModeColumn], command, outError))
    {
        return false;
    }

    m_commands.push_back (command);
    ApplyRows();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::MoveCommand
//
////////////////////////////////////////////////////////////////////////////////

void ProjectSettingsDialog::MoveCommand (int from, int to)
{
    auto  first = m_commands.begin();



    if (from < to)
    {
        std::rotate (first + from, first + from + 1, first + to + 1);
    }
    else
    {
        std::rotate (first + to, first + from, first + from + 1);
    }

    ApplyRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProjectSettingsDialog::ShowRowMenu
//
//  Delete, on a command's row.
//
////////////////////////////////////////////////////////////////////////////////

void ProjectSettingsDialog::ShowRowMenu (const DxuiMouseEvent & ev, int row)
{
    if (!m_grid.CanDeleteRow (row) || GetPopupHost() == nullptr)
    {
        return;
    }

    m_grid.SetSelectedRow (row);

    m_deleteCommand           = std::make_shared<DxuiCommand>();
    m_deleteCommand->label    = L"Delete";
    m_deleteCommand->dispatch = [this, row] { m_grid.DeleteRow (row); };

    DxuiContextMenu::Show (*GetPopupHost(), ev.positionDip.x, ev.positionDip.y, { DxuiPopupMenuItem::ForCommand (m_deleteCommand) });
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

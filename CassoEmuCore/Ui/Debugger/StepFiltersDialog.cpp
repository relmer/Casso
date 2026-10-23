#include "Pch.h"

#include "Ui/Debugger/StepFiltersDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StepFiltersPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void StepFiltersPanel::Init (DxuiLabel & heading, DxuiEditableGrid & grid, DxuiLabel & error)
{
    m_heading = &heading;
    m_grid    = &grid;
    m_error   = &error;

    Adopt (heading);
    Adopt (grid);
    Adopt (error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFiltersPanel::Layout
//
//  The heading a row, the error line at the bottom, and the grid between
//  them.
//
////////////////////////////////////////////////////////////////////////////////

void StepFiltersPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  row = scaler.ToPx (kRowDip);
    int  gap = scaler.ToPx (kGapDip);



    SetBounds (boundsDip);

    m_heading->Layout (RECT { boundsDip.left, boundsDip.top, boundsDip.right, boundsDip.top + row }, scaler);
    m_grid->Layout    (RECT { boundsDip.left, boundsDip.top + row, boundsDip.right, boundsDip.bottom - row - gap }, scaler);
    m_error->Layout   (RECT { boundsDip.left, boundsDip.bottom - row, boundsDip.right, boundsDip.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFiltersPanel::OnMouse
//
//  Only the grid takes the pointer: over it, or anywhere while it edits, so
//  a press elsewhere commits its edit. A press over it is passed on first,
//  with the row under it, for the focus and a row's menu.
//
////////////////////////////////////////////////////////////////////////////////

bool StepFiltersPanel::OnMouse (const DxuiMouseEvent & ev)
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
//  StepFiltersPanel::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR StepFiltersPanel::GetCursorForPoint (POINT clientPx) const
{
    RECT  bounds = m_grid->GetBounds();



    if (!PtInRect (&bounds, clientPx))
    {
        return nullptr;
    }

    return m_grid->GetCursorForPoint (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top });
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFiltersDialog::TryParseRange
//
////////////////////////////////////////////////////////////////////////////////

bool StepFiltersDialog::TryParseRange (const std::wstring & text, Word & first, Word & last)
{
    static const std::wregex  s_kRange (LR"(^\$?([0-9A-Fa-f]{1,4})(\.\$?([0-9A-Fa-f]{1,4}))?$)");
    std::wsmatch              match;



    if (!std::regex_match (text, match, s_kRange))
    {
        return false;
    }

    first = (Word) std::stoul (match[1].str(), nullptr, 16);
    last  = match[3].matched ? (Word) std::stoul (match[3].str(), nullptr, 16) : first;

    return last >= first;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFiltersDialog::OnCreate
//
//  OK takes the filters as the grid has them.
//
////////////////////////////////////////////////////////////////////////////////

void StepFiltersDialog::OnCreate()
{
    DxuiButton  * ok = nullptr;



    for (DxuiLabel * label : { &m_heading, &m_error })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_error.SetTextRole (DxuiTextRole::Error);
    m_heading.SetText   (L"Routines a step goes over, by symbol, address or range:");

    ConfigureGrid();

    m_body = CreateDialogContent<StepFiltersPanel>();
    m_body->Init (m_heading, m_grid, m_error);
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

        m_result = m_filters;
        EndDialog (IDOK);
    });

    SetInitialFocus (&m_grid);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFiltersDialog::ConfigureGrid
//
////////////////////////////////////////////////////////////////////////////////

void StepFiltersDialog::ConfigureGrid()
{
    constexpr int  kFilterDip = 380;



    m_grid.SetTheme       (m_theme);
    m_grid.SetShowHeader  (true);
    m_grid.SetEditorHwnd  (GetHwnd());
    m_grid.SetGridColumns ({ { { L"Filter", kFilterDip }, true, false } });
    m_grid.SetAddRowText  (L"Add step filter");

    m_grid.SetOnCommitCell ([this] (int row, int, const std::wstring & text, std::wstring & outError)
    {
        std::wstring  taken;



        if (!TryTake (text, taken, outError))
        {
            return false;
        }

        m_filters[(size_t) row] = taken;
        ApplyRows();
        return true;
    });

    m_grid.SetOnAddRow ([this] (const std::vector<std::wstring> & texts, std::wstring & outError)
    {
        std::wstring  taken;



        if (!TryTake (texts[0], taken, outError))
        {
            return false;
        }

        m_filters.push_back (taken);
        ApplyRows();
        return true;
    });

    m_grid.SetOnError     ([this] (const std::wstring & text) { m_error.SetText (text); Invalidate(); });
    m_grid.SetOnDeleteRow ([this] (int row)
    {
        m_filters.erase (m_filters.begin() + row);
        ApplyRows();
    });

    ApplyRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFiltersDialog::ApplyRows
//
////////////////////////////////////////////////////////////////////////////////

void StepFiltersDialog::ApplyRows()
{
    std::vector<DxuiEditableGrid::GridRow>  rows;



    for (const std::wstring & filter : m_filters)
    {
        rows.push_back ({ { { filter } } });
    }

    m_grid.SetGridRows (std::move (rows));
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFiltersDialog::TryTake
//
//  The text trimmed: a range as it is, a name only where the check finds a
//  symbol by it.
//
////////////////////////////////////////////////////////////////////////////////

bool StepFiltersDialog::TryTake (const std::wstring & text, std::wstring & taken, std::wstring & error) const
{
    size_t  first = text.find_first_not_of (L" \t");
    size_t  last  = text.find_last_not_of (L" \t");
    Word    from  = 0;
    Word    to    = 0;



    if (first == std::wstring::npos)
    {
        error = L"A step filter is a symbol's name, an address or a range.";
        return false;
    }

    taken = text.substr (first, last - first + 1);

    return TryParseRange (taken, from, to) || (m_check != nullptr && m_check (taken, error));
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFiltersDialog::ShowRowMenu
//
//  Delete, on a filter's row.
//
////////////////////////////////////////////////////////////////////////////////

void StepFiltersDialog::ShowRowMenu (const DxuiMouseEvent & ev, int row)
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
//  StepFiltersDialog::RunModal
//
////////////////////////////////////////////////////////////////////////////////

std::optional<StepFiltersDialog::Filters> StepFiltersDialog::RunModal (HWND owner, const IDxuiTheme * theme, const Filters & filters, CheckFn check)
{
    HRESULT                   hr     = S_OK;
    StepFiltersDialog         dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme   = theme;
    dialog.m_filters = filters;
    dialog.m_check   = std::move (check);

    params.title                    = L"Step filters";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 480, 420 };
    params.minSizeDip               = { 420, 420 };
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

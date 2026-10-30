#include "Pch.h"

#include "CassoExplorer/ChooseDetailsDialog.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsPanel::Init (DxuiLabel & prompt, DxuiListView & list, std::vector<DxuiButton *> buttons)
{
    m_prompt  = &prompt;
    m_list    = &list;
    m_buttons = std::move (buttons);

    Adopt (prompt);
    Adopt (list);

    for (DxuiButton * button : m_buttons)
    {
        Adopt (*button);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  gap        = scaler.ToPx (kGapDip);
    int  listTop    = boundsPx.top + scaler.ToPx (kPromptDip) + gap;
    int  listRight  = boundsPx.right - scaler.ToPx (kButtonWidthDip) - gap;
    int  buttonTop  = listTop;



    SetBounds (boundsPx);

    m_prompt->Layout (RECT { boundsPx.left, boundsPx.top, boundsPx.right, listTop - gap }, scaler);
    m_list->Layout   (RECT { boundsPx.left, listTop, listRight, (LONG) boundsPx.bottom }, scaler);

    for (DxuiButton * button : m_buttons)
    {
        button->Layout (RECT { listRight + gap, buttonTop, boundsPx.right, buttonTop + scaler.ToPx (kButtonDip) }, scaler);
        buttonTop += scaler.ToPx (kButtonDip) + gap / 2;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsPanel::OnMouse
//
//  The list has the pointer while it drags, wherever the pointer goes.
//
////////////////////////////////////////////////////////////////////////////////

bool ChooseDetailsPanel::OnMouse (const DxuiMouseEvent & ev)
{
    DxuiMouseEvent  local  = ev;
    RECT            bounds = m_list->GetBounds();
    POINT           point  = { ev.positionDip.x, ev.positionDip.y };
    IDxuiControl  * taker  = nullptr;



    local.positionDip = { ev.positionDip.x - bounds.left, ev.positionDip.y - bounds.top };

    if (m_list->IsInteracting() || PtInRect (&bounds, point))
    {
        taker = m_list->OnMouse (local) ? m_list : nullptr;
    }
    else
    {
        //  A move outside clears the list's hover.
        local.kind = DxuiMouseEventKind::Leave;
        m_list->OnMouse (local);

        for (IDxuiControl * control : std::initializer_list<IDxuiControl *> { m_buttons[0], m_buttons[1], m_buttons[2], m_buttons[3] })
        {
            if (taker == nullptr && control->IsEnabled() && control->OnMouse (ev))
            {
                taker = control;
            }
        }
    }

    if (taker != nullptr && ev.kind == DxuiMouseEventKind::Down && m_onPressed)
    {
        m_onPressed (taker);
    }

    return taker != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsPanel::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ChooseDetailsPanel::GetCursorForPoint (POINT clientPx) const
{
    RECT  bounds = m_list->GetBounds();



    if (PtInRect (&bounds, clientPx))
    {
        return m_list->GetCursorForPoint (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top });
    }

    return DxuiPanel::GetCursorForPoint (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsDialog::Run
//
////////////////////////////////////////////////////////////////////////////////

bool ChooseDetailsDialog::Run (HWND owner, const IDxuiTheme * theme, ChooseDetailsModel & model)
{
    HRESULT                   hr     = S_OK;
    ChooseDetailsDialog       dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_model = &model;
    dialog.m_theme = theme;

    params.title                    = L"Choose details";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 480, 560 };
    params.minSizeDip               = { 400, 360 };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return false;
    }

    dialog.SetTheme (theme);

    return dialog.ShowModalDialog (IDOK) == IDOK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsDialog::OnCreate()
{
    ExplorerTextStyles::Apply (m_prompt, ExplorerTextStyles::kFormLabel);
    m_prompt.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_prompt.SetText      (L"Select the details you want to display for the items in this folder.");

    m_list.SetDpi                (GetDpi());
    m_list.SetTheme              (m_theme);
    m_list.SetOwnerWindow        (GetHwnd());
    m_list.SetShowHeader         (false);
    m_list.SetColumns            ({ DxuiListView::Column { L"", 0, true } });
    m_list.SetKeyboardColumnNav  (true);
    m_list.SetAlwaysShowSelection (true);
    m_list.SetMultiSelect         (true);
    m_list.SetOnCheckToggled     ([this] (int row) { m_model->Toggle ((size_t) row); FillList(); });
    m_list.SetOnSelectionChanged ([this] (int row) { m_selected = row; UpdateButtons(); });


    m_up.SetOnClick   ([this]() { m_model->MoveUp   ((size_t) m_selected); FillList(); SelectEntry ((size_t) m_selected - 1); KeepFocus (m_up); });
    m_down.SetOnClick ([this]() { m_model->MoveDown ((size_t) m_selected); FillList(); SelectEntry ((size_t) m_selected + 1); KeepFocus (m_down); });
    m_show.SetOnClick ([this]() { SetChecked (true);  KeepFocus (m_show); });
    m_hide.SetOnClick ([this]() { SetChecked (false); KeepFocus (m_hide); });

    m_body = CreateDialogContent<ChooseDetailsPanel>();
    m_body->Init (m_prompt, m_list, { &m_up, &m_down, &m_show, &m_hide });
    m_body->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });

    AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    FillList();
    SelectEntry (0);
    SetInitialFocus (&m_list);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsDialog::FillList
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsDialog::FillList()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    std::vector<int>                              selected = m_list.GetSelectedRows();



    for (const ChooseDetailsModel::Entry & entry : m_model->GetEntries())
    {
        DxuiListView::Cell  cell { entry.title };

        cell.check = entry.fixed ? DxuiListView::Cell::Check::OnFixed : (entry.checked ? DxuiListView::Cell::Check::On : DxuiListView::Cell::Check::Off);

        rows.push_back ({ std::move (cell) });
    }

    m_list.SetRows (std::move (rows));

    m_list.SetSelectedRows (selected, m_selected);

    UpdateButtons();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsDialog::SelectEntry
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsDialog::SelectEntry (size_t entry)
{
    if (entry >= m_model->GetEntries().size())
    {
        return;
    }

    m_selected = (int) entry;
    m_list.SetSelectedRow (m_selected);
    m_list.EnsureVisible  (m_selected);

    UpdateButtons();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsDialog::UpdateButtons
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsDialog::UpdateButtons()
{
    const std::vector<ChooseDetailsModel::Entry> &  entries  = m_model->GetEntries();
    bool                                            selected = m_selected >= 0 && (size_t) m_selected < entries.size() && m_list.GetSelectedRows().size() == 1;



    m_up.SetEnabled   (selected && m_model->CanMoveUp   ((size_t) m_selected));
    m_down.SetEnabled (selected && m_model->CanMoveDown ((size_t) m_selected));
    m_show.SetEnabled (std::any_of (m_list.GetSelectedRows().begin(), m_list.GetSelectedRows().end(), [&entries] (int row) { return row >= 0 && (size_t) row < entries.size() && !entries[(size_t) row].fixed && !entries[(size_t) row].checked; }));
    m_hide.SetEnabled (std::any_of (m_list.GetSelectedRows().begin(), m_list.GetSelectedRows().end(), [&entries] (int row) { return row >= 0 && (size_t) row < entries.size() && !entries[(size_t) row].fixed && entries[(size_t) row].checked; }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsDialog::SetChecked
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsDialog::SetChecked (bool checked)
{
    for (int row : m_list.GetSelectedRows())
    {
        if (row >= 0 && (size_t) row < m_model->GetEntries().size() && m_model->GetEntries()[(size_t) row].checked != checked)
        {
            m_model->Toggle ((size_t) row);
        }
    }

    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsDialog::KeepFocus
//
//  Focus moves to the list when a button's click disabled it.
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsDialog::KeepFocus (DxuiButton & button)
{
    if (!button.IsEnabled())
    {
        FocusControl (&m_list);
    }
}

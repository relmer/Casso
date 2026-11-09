#include "Pch.h"

#include "CassoLinkActivityDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityList::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkActivityList::Init (DxuiListView * list)
{
    m_list = list;

    Adopt (*list);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityList::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkActivityList::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsPx);
    m_list->Layout (boundsPx, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityList::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkActivityList::OnMouse (const DxuiMouseEvent & ev)
{
    DxuiMouseEvent  local  = ev;
    RECT            bounds = m_list->GetBounds();



    local.positionDip = { ev.positionDip.x - bounds.left, ev.positionDip.y - bounds.top };

    return m_list->OnMouse (local);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityList::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR CassoLinkActivityList::GetCursorForPoint (POINT clientPx) const
{
    RECT  bounds = m_list->GetBounds();



    return m_list->GetCursorForPoint (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityDialog::FormatTime
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkActivityDialog::FormatTime (const SYSTEMTIME & time)
{
    return std::format (L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityDialog::BuildRows
//
//  An operation starts when it begins to run and ends with its entry; any
//  other entry starts when it is logged, with no end.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> CassoLinkActivityDialog::BuildRows (const std::vector<CassoLinkConnections::Activity> & log)
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;



    for (auto entry = log.rbegin(); entry != log.rend(); entry++)
    {
        rows.push_back ({ DxuiListView::Cell { FormatTime (entry->operation ? entry->started : entry->time) },
                          DxuiListView::Cell { entry->operation ? FormatTime (entry->time) : std::wstring() },
                          DxuiListView::Cell { entry->connection },
                          DxuiListView::Cell { entry->text } });
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityDialog::Show
//
//  The list is declared ahead of the dialog, whose content has a reference
//  to it, so the dialog is destroyed first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkActivityDialog::Show (HWND owner, const IDxuiTheme * theme, CassoLinkActivityFn getLog)
{
    HRESULT                                      hr      = S_OK;
    std::vector<CassoLinkConnections::Activity>  log     = getLog();
    DxuiListView                                 list;
    std::unique_ptr<CassoLinkActivityList>       content = std::make_unique<CassoLinkActivityList>();
    CassoLinkActivityPicker                      dialog;
    DxuiWindow::CreateParams                     params;
    UINT                                         dpi     = (owner != nullptr) ? GetDpiForWindow (owner) : GetDpiForSystem();



    list.SetDpi                     (dpi);
    list.SetTheme                   (theme);
    list.SetShowHeader              (true);
    list.SetColumns                 ({ { L"Started",    0, false, DxuiTextHAlign::Left },
                                       { L"Ended",      0, false, DxuiTextHAlign::Left },
                                       { L"Connection", 0, false, DxuiTextHAlign::Left },
                                       { L"Activity",   0, false, DxuiTextHAlign::Left } });
    list.SetHorizontalScrollEnabled (true);
    list.SetPreciseAutoFit          (true);
    list.SetMultiSelect             (true);
    list.SetRows                    (BuildRows (log));

    content->Init (&list);
    dialog.Watch  (&list, std::move (getLog), log);
    dialog.ConfigurePicker (std::move (content), &list, { DialogButton { L"Close", 0, true, true } }, 0);

    params.title                    = L"CassoLink activity";
    params.hInstance                = (owner != nullptr) ? (HINSTANCE) GetWindowLongPtrW (owner, GWLP_HINSTANCE) : GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kWidthDip, kHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme     (theme);
    list.SetOwnerWindow (dialog.GetHwnd());

    dialog.ShowModalDialog (dialog.GetDefaultCommandId());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityPicker::Watch
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkActivityPicker::Watch (DxuiListView * list, CassoLinkActivityFn getLog, const std::vector<CassoLinkConnections::Activity> & listed)
{
    m_list   = list;
    m_getLog = std::move (getLog);
    m_newest = listed.empty() ? std::wstring() : GetKey (listed.back());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityPicker::GetKey
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkActivityPicker::GetKey (const CassoLinkConnections::Activity & entry)
{
    return CassoLinkActivityDialog::FormatTime (entry.time) + L"\t" + entry.connection + L"\t" + entry.text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityPicker::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkActivityPicker::OnCreate()
{
    PickerDialog::OnCreate();

    SetDialogTickIntervalMs (kTickMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityPicker::OnDialogTick
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkActivityPicker::OnDialogTick()
{
    std::vector<CassoLinkConnections::Activity>  log;
    std::vector<int>                             selected;
    int                                          added  = 0;
    int                                          top    = 0;
    int                                          anchor = 0;



    log = (m_list != nullptr && m_getLog) ? m_getLog() : std::vector<CassoLinkConnections::Activity>();

    if (m_list != nullptr && !log.empty() && GetKey (log.back()) != m_newest)
    {
        while (added < (int) log.size() && GetKey (log[log.size() - 1 - (size_t) added]) != m_newest)
        {
            added++;
        }

        top      = m_list->GetTopRow();
        anchor   = m_list->GetSelectedRow();
        selected = m_list->GetSelectedRows();
        m_newest = GetKey (log.back());

        for (int & row : selected)
        {
            row += added;
        }

        std::erase_if (selected, [&] (int row) { return row >= (int) log.size(); });

        m_list->SetRows         (CassoLinkActivityDialog::BuildRows (log));
        m_list->SetSelectedRows (std::move (selected), (anchor < 0 || anchor + added >= (int) log.size()) ? -1 : anchor + added);

        if (top > 0)
        {
            m_list->SetTopRow (top + added);
        }

        Invalidate();
    }
}
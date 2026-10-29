#include "Pch.h"

#include "CassoLinkActivityDialog.h"
#include "Ui/PickerDialog.h"





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

HRESULT CassoLinkActivityDialog::Show (HWND owner, const IDxuiTheme * theme, const std::vector<CassoLinkConnections::Activity> & log)
{
    HRESULT                                 hr      = S_OK;
    DxuiListView                            list;
    std::unique_ptr<CassoLinkActivityList>  content = std::make_unique<CassoLinkActivityList>();
    PickerDialog                            dialog;
    DxuiWindow::CreateParams                params;
    UINT                                    dpi     = (owner != nullptr) ? GetDpiForWindow (owner) : GetDpiForSystem();



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

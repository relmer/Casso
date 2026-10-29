#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkConnections.h"


class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityList
//
//  The activity dialog's content: the list, filling the dialog. Mouse
//  points are translated to the list's coordinates.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkActivityList : public DxuiPanel
{
public:
    void     Init              (DxuiListView * list);
    void     Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

private:
    DxuiListView  * m_list = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityDialog
//
//  The CassoLink activity log as a list, newest first: when each entry
//  started, when an operation ended, its connection, and what happened.
//  Ctrl+C copies the selected rows, a tab between cells.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkActivityDialog
{
public:
    static constexpr int  kWidthDip     = 900;
    static constexpr int  kHeightDip    = 520;
    static constexpr int  kMinWidthDip  = 480;
    static constexpr int  kMinHeightDip = 240;

    static std::vector<std::vector<DxuiListView::Cell>>  BuildRows  (const std::vector<CassoLinkConnections::Activity> & log);
    static std::wstring                                  FormatTime (const SYSTEMTIME & time);
    static HRESULT                                       Show       (HWND owner, const IDxuiTheme * theme, const std::vector<CassoLinkConnections::Activity> & log);
};

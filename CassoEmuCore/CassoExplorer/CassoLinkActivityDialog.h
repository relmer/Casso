#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "Ui/PickerDialog.h"


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





using CassoLinkActivityFn = std::function<std::vector<CassoLinkConnections::Activity> ()>;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkActivityPicker
//
//  The activity dialog's window. The log is read again twice a second, and the
//  entries added since the newest one listed go at the top. A list scrolled
//  down from the top, and its selection, stay on the same entries. Once the
//  log is full its oldest entries are dropped, so the newest entry is compared
//  rather than the count.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkActivityPicker : public PickerDialog
{
public:
    static constexpr UINT  kTickMs = 500;

    void  Watch (DxuiListView * list, CassoLinkActivityFn getLog, const std::vector<CassoLinkConnections::Activity> & listed);

    static std::wstring  GetKey (const CassoLinkConnections::Activity & entry);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    DxuiListView         * m_list = nullptr;
    CassoLinkActivityFn    m_getLog;
    std::wstring           m_newest;     // the newest entry listed, by GetKey
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
    static HRESULT                                       Show       (HWND owner, const IDxuiTheme * theme, CassoLinkActivityFn getLog);
};

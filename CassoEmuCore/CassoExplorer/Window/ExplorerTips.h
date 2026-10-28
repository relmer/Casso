#pragma once

#include "Pch.h"


class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips
//
//  The hover tooltip: whose it is, and the tree's, the list's, the status
//  bar's and a cut-off tree name's tips.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerTips
{
public:
    explicit ExplorerTips (CassoExplorerWindow & window);

    static constexpr size_t  kActivityTipLines     = 10;
    static constexpr float   kActivityTipWidthDip  = 720.0f;   // a log line, time and link and all, on one line

    //  Which part of the window the hover tooltip belongs to.
    enum class TipOwner { None, Toolbar, Tree, List, Status, Menu };

    void  ShowHoverTip        (TipOwner owner, const RECT & anchor, const std::wstring & text,
                               float maxWidthDip = DxuiTooltip::kDefaultMaxTextWidthDip);
    void  HideHoverTip        (TipOwner owner);
    void  UpdateStatusTip     (const DxuiMouseEvent & ev, POINT point);
    void  UpdateListTip       (const DxuiMouseEvent & ev, POINT point);
    void  RefreshListTip      ();

private:
    friend class CassoExplorerWindow;

    CassoExplorerWindow  & m_window;

    //  The text of the CassoLink line's tip, for as long as the tip is up.
    std::wstring  m_activityTip;
    POINT         m_listTipPoint  = {};   // where the pointer last was over the list
    bool          m_listTipActive = false;
    TipOwner      m_tipOwner      = TipOwner::None;
};

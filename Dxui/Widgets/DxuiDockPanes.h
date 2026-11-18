#pragma once

#include "Pch.h"
#include "Widgets/DxuiTabGroup.h"


class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPane
//
//  A pane a dock site places: its title and control, and the indicator,
//  leading mark, tip and title-bar room its tab has wherever the pane is
//  docked.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiDockPane
{
    std::wstring                 title;
    IDxuiControl               * content       = nullptr;
    bool                         indicator     = false;
    DxuiTabGroup::LeadingMark    leadMark;
    std::wstring                 tip;
    int                          titleExtraDip = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanes
//
//  A dock site's panes by name. The setters return false for a pane that
//  was never added; SetLeadingMark and SetTitleExtra also return false when
//  the value is the one the pane has.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockPanes
{
public:
    using Panes = std::map<std::wstring, DxuiDockPane>;

    void                  Add    (const std::wstring & pane, const std::wstring & title, IDxuiControl * content) { m_panes[pane] = DxuiDockPane { title, content }; }
    const DxuiDockPane  * Find   (const std::wstring & pane) const;
    const Panes         & GetAll () const { return m_panes; }

    bool  SetTitle       (const std::wstring & pane, const std::wstring & title);
    bool  SetIndicator   (const std::wstring & pane, bool on);
    bool  SetLeadingMark (const std::wstring & pane, const DxuiTabGroup::LeadingMark & mark);
    bool  SetTabTip      (const std::wstring & pane, const std::wstring & tip);
    bool  SetTitleExtra  (const std::wstring & pane, int widthDip);

    std::wstring    GetPaneOf    (const IDxuiControl * content) const;
    IDxuiControl  * GetContent   (const std::wstring & pane) const;
    std::wstring    GetTitle     (const std::wstring & pane) const;
    std::wstring    GetTip       (const std::wstring & pane) const;
    bool            HasIndicator (const std::wstring & pane) const;

private:
    Panes  m_panes;
};

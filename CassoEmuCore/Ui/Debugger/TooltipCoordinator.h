#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/Panes/OperandTipController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ITooltipSources
//
//  What the tooltip coordinator reads from the window: the tip service of the
//  window the event being routed came from, whether a menu is open, and the
//  tips of what the window draws outside the panes. Every point is in that
//  window's client pixels.
//
////////////////////////////////////////////////////////////////////////////////

class ITooltipSources
{
public:
    virtual ~ITooltipSources() = default;

    virtual DxuiTipService &                                     GetRoutedTips     () = 0;
    virtual bool                                                 IsAnyMenuOpen     () const = 0;
    virtual const std::vector<std::unique_ptr<IDebuggerPane>> &  GetTipPanes       () const = 0;
    virtual OperandTipController &                               GetOperandTips    () = 0;

    //  The command bar's entry and the find widgets' buttons under a point,
    //  or null.
    virtual const wchar_t *  GetCommandBarTip (POINT clientPx, RECT & anchor) const = 0;
    virtual const wchar_t *  GetFindTip       (POINT clientPx, RECT & anchor) const = 0;

    //  A disassembly view's branch arrow and symbol, the status bar's
    //  graphics, and a tab or a title-bar button of the routed window's dock
    //  site.
    virtual bool  TryGetBranchTip  (POINT clientPx, RECT & anchor, std::wstring & text) const = 0;
    virtual bool  TryGetSymbolTip  (POINT clientPx, RECT & anchor, std::wstring & text) const = 0;
    virtual bool  TryGetGraphicTip (POINT clientPx, RECT & anchor, std::wstring & text) const = 0;
    virtual bool  TryGetChromeTip  (POINT clientPx, DxuiTip & tip) const = 0;

    //  The lists in the routed window.
    virtual std::vector<DxuiListView *>  GetTipLists () const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator
//
//  The one tip at the pointer, from the first source that has one: the
//  bars, then what a pane draws, a branch arrow, a symbol, a color that
//  means something, a tab and a title-bar button, displayed by the routed
//  window's tip service. No tip opens while a menu is open, so no tip ever
//  covers its rows.
//
////////////////////////////////////////////////////////////////////////////////

class TooltipCoordinator
{
public:
    explicit TooltipCoordinator (ITooltipSources & sources) : m_sources (sources) {}

    void  Update (POINT clientPx);

    //  The tip under the point, and whether it indicates what a color means,
    //  which goes under the operand tip when one is up.
    bool  TryGetTip     (POINT clientPx, DxuiTip & tip, bool & isColorTip) const;

    //  The tip of the list cell or heading under the point. Past the last
    //  column there is none.
    bool  TryGetCellTip (POINT clientPx, DxuiTip & tip) const;

private:
    bool  TryGetBarTip  (POINT clientPx, DxuiTip & tip) const;
    bool  TryGetPaneTip (POINT clientPx, DxuiTip & tip) const;

    ITooltipSources  & m_sources;
};

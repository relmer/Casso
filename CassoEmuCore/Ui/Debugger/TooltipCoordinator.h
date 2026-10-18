#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/Panes/OperandTipController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ITooltipSources
//
//  What the tooltip coordinator reads from the window: the tooltip of the
//  window the event being routed came from, whether a menu is open, and the
//  tips of what the window draws outside the panes. Every point is in that
//  window's client pixels.
//
////////////////////////////////////////////////////////////////////////////////

class ITooltipSources
{
public:
    virtual ~ITooltipSources() = default;

    virtual DxuiTooltip &                                        GetRoutedTooltip  () = 0;
    virtual bool                                                 IsAnyMenuOpen     () const = 0;
    virtual const std::vector<std::unique_ptr<IDebuggerPane>> &  GetTipPanes       () const = 0;
    virtual OperandTipController &                               GetOperandTips    () = 0;

    //  The command bar's entry and the find widgets' buttons under a point,
    //  or null.
    virtual const wchar_t *  GetCommandBarTip (POINT clientPx, RECT & anchor) const = 0;
    virtual const wchar_t *  GetFindTip       (POINT clientPx, RECT & anchor) const = 0;

    //  A disassembly view's branch arrow and symbol, the device panels' and
    //  status bar's graphics, and a tab or a title-bar button.
    virtual bool          TryGetBranchTip  (POINT clientPx, RECT & anchor, std::wstring & text) const = 0;
    virtual bool          TryGetSymbolTip  (POINT clientPx, RECT & anchor, std::wstring & text) const = 0;
    virtual bool          TryGetGraphicTip (POINT clientPx, RECT & anchor, std::wstring & text) const = 0;
    virtual std::wstring  GetChromeTipAt   (POINT clientPx, RECT & anchor) const = 0;

    //  The lists in the routed window, and the column of one under an x in
    //  its own pixels.
    virtual std::vector<DxuiListView *>  GetTipLists     () const = 0;
    virtual int                          GetListColumnAt (const DxuiListView * list, int xPx) const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator
//
//  The one tip at the pointer, from the first source that has one: the
//  bars, then what a pane draws, a branch arrow, a symbol, a color that
//  means something, a tab and a title-bar button. While a menu is open there
//  is no tip, so none ever covers its rows.
//
////////////////////////////////////////////////////////////////////////////////

class TooltipCoordinator
{
public:
    explicit TooltipCoordinator (ITooltipSources & sources) : m_sources (sources) {}

    void  Update (POINT clientPx);

    //  The tip of the list cell under the point, which indicates what its
    //  color means, or the tip of the heading under it. Past the last
    //  column there is none.
    bool  TryGetCellTip (POINT clientPx, RECT & anchor, std::wstring & text) const;

private:
    bool  TryShowBarTip  (POINT clientPx, DxuiTooltip & tip, int64_t now);
    bool  TryShowPaneTip (POINT clientPx, DxuiTooltip & tip, int64_t now, bool & isPending);

    static RECT  GetPointerAnchor (POINT clientPx) { return RECT { clientPx.x, clientPx.y, clientPx.x + 1, clientPx.y + 1 }; }

    ITooltipSources  & m_sources;
};

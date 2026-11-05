#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/Panes/OperandTipController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ITooltipSources
//
//  What the tooltip coordinator reads from the window: the tip service of the
//  window the event being routed came from, whether a menu is open, the
//  operand tips, and the tip of the control under a point in that window.
//  Every point is in that window's client pixels.
//
////////////////////////////////////////////////////////////////////////////////

class ITooltipSources
{
public:
    virtual ~ITooltipSources() = default;

    virtual DxuiTipService &        GetRoutedTips   () = 0;
    virtual bool                    IsAnyMenuOpen   () const = 0;
    virtual OperandTipController &  GetOperandTips  () = 0;

    //  The tip of the one control the routed window's hit test finds under
    //  the point: the front control there, in the order the window paints.
    virtual bool                    TryGetRoutedTip (POINT clientPx, DxuiTip & tip) const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator
//
//  The one tip at the pointer: the tip of the control the window's hit test
//  finds there, displayed by the routed window's tip service. A tip that is
//  what a color means goes under the operand tip when one is up. No tip opens
//  while a menu is open, so no tip ever covers its rows.
//
////////////////////////////////////////////////////////////////////////////////

class TooltipCoordinator
{
public:
    explicit TooltipCoordinator (ITooltipSources & sources) : m_sources (sources) {}

    void  Update (POINT clientPx);

    //  The tip under the point, and whether it is what a color means.
    bool  TryGetTip (POINT clientPx, DxuiTip & tip, bool & isColorTip) const;

private:
    ITooltipSources  & m_sources;
};

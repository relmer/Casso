#include "Pch.h"

#include "Ui/Debugger/TooltipCoordinator.h"

#include "Ui/Debugger/ColorLegend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator::Update
//
//  A color tip put below the operand tip is passed to OperandTipController,
//  so that the two are hidden together.
//
////////////////////////////////////////////////////////////////////////////////

void TooltipCoordinator::Update (POINT clientPx)
{
    int64_t                now         = (int64_t) GetTickCount64();
    DxuiTipService       & tips        = m_sources.GetRoutedTips();
    OperandTipController & operandTips = m_sources.GetOperandTips();
    DxuiTip                tip;
    bool                   isColorTip  = false;



    operandTips.SetColorTip (nullptr);
    tips.SetSuppressed (m_sources.IsAnyMenuOpen());

    if (tips.IsSuppressed())
    {
        return;
    }

    if (!TryGetTip (clientPx, tip, isColorTip))
    {
        tips.Hide (now);
        return;
    }

    //  Under the operand tip when there is one, so the two stack.
    if (isColorTip)
    {
        operandTips.SetColorTip (&tips.GetTooltip());

        if (operandTips.HasTip())
        {
            tip.anchor    = operandTips.GetColorTipAnchor (tip.anchor);
            tip.placement = DxuiTip::Placement::AtAnchor;
        }
    }

    tips.Show (tip, clientPx, now);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator::TryGetTip
//
//  A symbol's tip is code, so it reads in the fixed-width face, and adds what
//  the color of the cell under it means. A tab's or title-bar button's tip
//  is a sentence, so it reads in the body face.
//
////////////////////////////////////////////////////////////////////////////////

bool TooltipCoordinator::TryGetTip (POINT clientPx, DxuiTip & tip, bool & isColorTip) const
{
    DxuiTip       colorTip;
    std::wstring  text;



    isColorTip = false;
    tip        = DxuiTip();

    if (TryGetBarTip (clientPx, tip) || TryGetPaneTip (clientPx, tip))
    {
        return true;
    }

    tip = DxuiTip();

    if (m_sources.TryGetBranchTip (clientPx, tip.anchor, tip.text))
    {
        return true;
    }

    if (m_sources.TryGetSymbolTip (clientPx, tip.anchor, text))
    {
        (void) TryGetCellTip (clientPx, colorTip);

        tip.text         = ColorLegend::JoinLines ({ text, colorTip.text });
        tip.isMonospace  = true;
        tip.gradient     = std::move (colorTip.gradient);
        tip.gradientMark = colorTip.gradientMark;
        tip.lineScales   = std::move (colorTip.lineScales);
        return true;
    }

    //  Over anything else drawn in a color that means something, what it
    //  means.
    if (TryGetCellTip (clientPx, tip))
    {
        isColorTip = true;
        return true;
    }

    tip = DxuiTip();

    if (m_sources.TryGetGraphicTip (clientPx, tip.anchor, tip.text))
    {
        isColorTip = true;
        return true;
    }

    tip = DxuiTip();
    return m_sources.TryGetChromeTip (clientPx, tip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator::TryGetBarTip
//
//  The command bar's entries, what each does and its key in the scheme in
//  force; then each pane's bar, then the find widgets'.
//
////////////////////////////////////////////////////////////////////////////////

bool TooltipCoordinator::TryGetBarTip (POINT clientPx, DxuiTip & tip) const
{
    const wchar_t  * barTip = m_sources.GetCommandBarTip (clientPx, tip.anchor);



    for (const std::unique_ptr<IDebuggerPane> & pane : m_sources.GetTipPanes())
    {
        if (barTip == nullptr || *barTip == L'\0')
        {
            barTip = pane->GetBarTip (clientPx, tip.anchor);
        }
    }

    if (barTip == nullptr || *barTip == L'\0')
    {
        barTip = m_sources.GetFindTip (clientPx, tip.anchor);
    }

    if (barTip == nullptr || *barTip == L'\0')
    {
        return false;
    }

    tip.text = barTip;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator::TryGetPaneTip
//
//  Over what a pane draws, such as the heat map or the profiler's timeline:
//  a control's tip, one at once following the pointer, or a pending one
//  while what it lists is read, after which a tip already up stays up.
//
////////////////////////////////////////////////////////////////////////////////

bool TooltipCoordinator::TryGetPaneTip (POINT clientPx, DxuiTip & tip) const
{
    for (const std::unique_ptr<IDebuggerPane> & pane : m_sources.GetTipPanes())
    {
        DxuiTip  paneTip;



        if (pane->TryGetTip (clientPx, paneTip))
        {
            tip = std::move (paneTip);
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator::TryGetCellTip
//
//  The front list under the point. A list takes points relative to itself,
//  so the point is moved there and the tip's anchor moved back.
//
////////////////////////////////////////////////////////////////////////////////

bool TooltipCoordinator::TryGetCellTip (POINT clientPx, DxuiTip & tip) const
{
    DxuiListView  * list   = nullptr;
    RECT            bounds = {};



    for (DxuiListView * each : m_sources.GetTipLists())
    {
        if (each != nullptr && each->IsVisible() && DxuiDockSite::Contains (each->GetBounds(), clientPx))
        {
            list = each;
        }
    }

    if (list == nullptr)
    {
        return false;
    }

    bounds = list->GetBounds();

    if (!list->TryGetTip (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top }, tip))
    {
        return false;
    }

    OffsetRect (&tip.anchor, bounds.left, bounds.top);
    return true;
}

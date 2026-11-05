#include "Pch.h"

#include "Ui/Debugger/TooltipCoordinator.h"





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
////////////////////////////////////////////////////////////////////////////////

bool TooltipCoordinator::TryGetTip (POINT clientPx, DxuiTip & tip, bool & isColorTip) const
{
    tip        = DxuiTip();
    isColorTip = false;

    if (!m_sources.TryGetRoutedTip (clientPx, tip))
    {
        return false;
    }

    isColorTip = tip.isColorKey;
    return true;
}

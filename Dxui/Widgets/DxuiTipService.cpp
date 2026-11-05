#include "Pch.h"

#include "Widgets/DxuiTipService.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiTooltip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::FindTip
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTipService::FindTip (const IDxuiControl & root, POINT pointPx, DxuiTip & tip)
{
    return root.IsVisible() && root.TryGetTip (pointPx, tip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::Update
//
//  Display the tip of the control under the pointer, or hide the tooltip
//  when no control there has a tip.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTipService::Update (const IDxuiControl & root, POINT pointPx, int64_t nowMs)
{
    DxuiTip  tip;



    if (FindTip (root, pointPx, tip))
    {
        Show (tip, pointPx, nowMs);
    }
    else
    {
        Hide (nowMs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::Show
//
//  A repeat of the tip that is up, or of the tip whose open delay is
//  running, changes nothing: DxuiTooltip compares the text, anchor and look
//  with those of the tip it has. A tip that follows the pointer is placed
//  against the pointer's pixel, not its anchor. A timed tip is opened once,
//  so a move does not push its closing time back.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTipService::Show (const DxuiTip & tip, POINT pointPx, int64_t nowMs)
{
    RECT  pointer   = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    bool  isTimedUp = false;



    if (m_isSuppressed)
    {
        m_tooltip.HideImmediate();
        return;
    }

    if (tip.isPending)
    {
        return;
    }

    if (tip.text.empty())
    {
        Hide (nowMs);
        return;
    }

    if (m_isPressHidden && IsPressedTip (tip))
    {
        return;
    }

    m_isPressHidden = false;
    m_lastText      = tip.text;

    ApplyLook (tip);

    if (tip.placement == DxuiTip::Placement::FollowPointer)
    {
        m_tooltip.RequestShowNow (pointer, tip.text, nowMs);
        return;
    }

    if (tip.isImmediate)
    {
        ShowImmediate (tip, pointer, nowMs);
        return;
    }

    if (tip.lifetime == DxuiTip::Lifetime::Timed)
    {
        isTimedUp = m_tooltip.IsVisible() && m_tooltip.GetText() == tip.text;

        if (!isTimedUp)
        {
            m_tooltip.ShowTimed (tip.anchor, tip.text, nowMs, tip.timedMs);
        }

        return;
    }

    m_tooltip.RequestShow (tip.anchor, tip.text, nowMs, tip.placement == DxuiTip::Placement::AtPointer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::ShowImmediate
//
//  A tip that opens at once is placed by the pointer when its text first
//  comes up, and isn't moved while the pointer stays over the same text.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTipService::ShowImmediate (const DxuiTip & tip, const RECT & pointer, int64_t nowMs)
{
    if (m_tooltip.IsVisible() && m_tooltip.GetText() == tip.text)
    {
        return;
    }

    m_tooltip.RequestShowNow (pointer, tip.text, nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::Hide
//
//  No tip under the pointer: the tooltip closes after its close delay, and a
//  tip hidden by a press may open again.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTipService::Hide (int64_t nowMs)
{
    m_isPressHidden = false;

    m_lastText.clear();
    m_tooltip.RequestHide (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::OnPress
//
//  As the system's tooltips do, a press hides the tip at once, and the tip
//  under the pointer at the press does not open again until the pointer is
//  over another tip or none.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTipService::OnPress()
{
    m_isPressHidden = true;
    m_pressedText   = m_lastText;

    m_tooltip.HideImmediate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::OnLeave
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTipService::OnLeave (int64_t nowMs)
{
    Hide (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::SetSuppressed
//
//  A tip requested before a press opened a menu would otherwise open over
//  the menu once its delay ran out.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTipService::SetSuppressed (bool isSuppressed)
{
    m_isSuppressed = isSuppressed;

    if (isSuppressed)
    {
        m_tooltip.HideImmediate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::Tick
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTipService::Tick (int64_t nowMs)
{
    if (!WantsTick())
    {
        return false;
    }

    m_tooltip.Tick (nowMs);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::WantsTick
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTipService::WantsTick() const
{
    return !m_isSuppressed && m_tooltip.WantsTick();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::ApplyLook
//
//  Every part of the look is set for every tip, so nothing of one tip's look
//  is drawn on the next: a toolbar button's tip after a tip with swatches was
//  once indented by the swatches' width.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTipService::ApplyLook (const DxuiTip & tip)
{
    m_tooltip.SetMonospace    (tip.isMonospace);
    m_tooltip.SetHasLifetime  (tip.lifetime == DxuiTip::Lifetime::ReadTime);
    m_tooltip.SetLineColors   (tip.lineColors);
    m_tooltip.SetLineSwatches (tip.lineSwatches);
    m_tooltip.SetLineScales   (tip.lineScales);
    m_tooltip.SetGradient     (tip.gradient, tip.gradientMark);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipService::IsPressedTip
//
//  Tips are compared by text, since the anchor of a tip placed by the
//  pointer moves with the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTipService::IsPressedTip (const DxuiTip & tip) const
{
    return tip.text == m_pressedText;
}

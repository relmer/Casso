#include "Pch.h"

#include "Widgets/DxuiActionBanner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiActionBanner::SetActions
//
//  Replaces the whole set of actions.
//
//  REPLACED RATHER THAN ADDED TO. A standing report absorbs further changes
//  instead of stacking, so setting its actions again must leave it with one
//  Restart button and not three.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiActionBanner::SetActions (const std::vector<std::wstring> & labels)
{
    size_t  i = 0;



    m_actions.clear();

    for (i = 0; i < labels.size(); i++)
    {
        auto     button = std::make_unique<DxuiButton> (labels[i]);
        size_t   index  = i;

        button->SetVariant (DxuiButton::Variant::Default);
        button->SetOnClick ([this, index] ()
        {
            if (m_onAction)
            {
                m_onAction (index);
            }
        });

        m_actions.push_back (std::move (button));
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiActionBanner::GetAction
//
////////////////////////////////////////////////////////////////////////////////

DxuiButton * DxuiActionBanner::GetAction (size_t index)
{
    DxuiButton *  button = nullptr;



    if (index < m_actions.size())
    {
        button = m_actions[index].get();
    }

    return button;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiActionBanner::GetActionColumnPx
//
//  How much of the width the actions take, gap to the text included.
//
//  ZERO WHEN THERE ARE NO ACTIONS, so a banner with none lays its text out
//  exactly as a plain DxuiInfoBanner would.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiActionBanner::GetActionColumnPx (const DxuiDpiScaler & scaler) const
{
    float   width = 0.0f;
    size_t  count = m_actions.size();



    if (count > 0)
    {
        width = scaler.ToPxf (s_kActionWidthDip) * (float) count
              + scaler.ToPxf (s_kActionGapDip)   * (float) count;
    }

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiActionBanner::GetPreferredHeightPx
//
//  The height this banner needs at a given width.
//
//  THE ACTION COLUMN COMES OUT OF THE WIDTH BEFORE THE TEXT WRAPS. Measuring
//  the text across the full width and then putting a button over it is how a
//  notice ends up with its message underneath its own action.
//
//  NEVER SHORTER THAN A BUTTON, because a report whose action is clipped offers
//  nothing at all.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiActionBanner::GetPreferredHeightPx (float widthPx, const DxuiDpiScaler & scaler) const
{
    float  actionsPx = GetActionColumnPx (scaler);
    float  textWidth = widthPx - actionsPx;
    float  textH     = 0.0f;
    float  actionH   = 0.0f;



    if (textWidth < 1.0f)
    {
        textWidth = 1.0f;
    }

    textH   = m_banner.GetPreferredHeightPx (textWidth, scaler);
    actionH = scaler.ToPxf (s_kActionHeightDip) + scaler.ToPxf (s_kEdgePadDip) * 2.0f;

    return (textH > actionH) ? textH : actionH;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiActionBanner::Layout
//
//  The banner fills the bounds; the actions sit against its trailing edge,
//  vertically centered, in the column reserved out of the width.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiActionBanner::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    float  actionW = scaler.ToPxf (s_kActionWidthDip);
    float  actionH = scaler.ToPxf (s_kActionHeightDip);
    float  gap     = scaler.ToPxf (s_kActionGapDip);
    float  edge    = scaler.ToPxf (s_kEdgePadDip);
    float  right   = (float) boundsDip.right - edge;
    float  midY    = (float) (boundsDip.top + boundsDip.bottom) * 0.5f;
    size_t i       = 0;



    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    m_banner.Layout (boundsDip, scaler);

    //  Laid out from the trailing edge back, so the first action is the one
    //  furthest from the edge and the order on screen reads left to right the
    //  way it was given.
    for (i = m_actions.size(); i > 0; i--)
    {
        RECT   slot = {};
        float  left = right - actionW;

        slot.left   = (LONG) left;
        slot.right  = (LONG) right;
        slot.top    = (LONG) (midY - actionH * 0.5f);
        slot.bottom = (LONG) (midY + actionH * 0.5f);

        m_actions[i - 1]->Layout (slot, scaler);

        right = left - gap;
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiActionBanner::OnMouse
//
//  Offered to each action in turn. The banner itself takes nothing: it is a
//  notice, and the only clickable things on it are its buttons.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiActionBanner::OnMouse (const DxuiMouseEvent & ev)
{
    bool    handled = false;
    size_t  i       = 0;



    for (i = 0; i < m_actions.size() && !handled; i++)
    {
        handled = m_actions[i]->OnMouse (ev);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiActionBanner::Paint
//
//  The notice first, then its actions over it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiActionBanner::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text,
                              const IDxuiTheme & theme)
{
    size_t  i = 0;



    m_banner.Paint (painter, text, theme);

    for (i = 0; i < m_actions.size(); i++)
    {
        m_actions[i]->Paint (painter, text, theme);
    }

    return;
}

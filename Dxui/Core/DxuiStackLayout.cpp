#include "Pch.h"

#include "Core/DxuiStackLayout.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStackLayout
//
////////////////////////////////////////////////////////////////////////////////

DxuiStackLayout::DxuiStackLayout (Orientation orientation,
                                  float       spacingDip,
                                  Align       crossAxisAlign)
    : m_orientation     (orientation),
      m_spacingDip      (spacingDip),
      m_crossAxisAlign  (crossAxisAlign)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWeight
//
//  weight == 0 keeps the child at its natural size (its existing
//  Bounds()'s main-axis extent). weight > 0 makes the child a flex
//  participant in the remaining-space distribution.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStackLayout::SetWeight (IDxuiControl * child, int weight)
{
    if (child == nullptr)
    {
        return;
    }

    if (weight <= 0)
    {
        m_weights.erase (child);
    }
    else
    {
        m_weights[child] = weight;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Arrange
//
//  Algorithm:
//    1. Compute the natural main-axis size for each non-flex child
//       from its current Bounds().
//    2. Compute the leftover space along the main axis after
//       subtracting natural sizes and inter-child spacing.
//    3. Distribute the leftover proportionally to flex children's
//       weights.
//    4. Walk children left-to-right (or top-to-bottom), assigning
//       bounds with the requested cross-axis alignment.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStackLayout::Arrange (
    const RECT                          & boundsDip,
    const DxuiDpiScaler                 & /*scaler*/,
    std::span<IDxuiControl * const>       children)
{
    bool   horizontal     = (m_orientation == Orientation::Horizontal);
    LONG   bandX          = boundsDip.left;
    LONG   bandY          = boundsDip.top;
    LONG   bandWidth      = boundsDip.right  - boundsDip.left;
    LONG   bandHeight     = boundsDip.bottom - boundsDip.top;
    LONG   mainExtent     = horizontal ? bandWidth  : bandHeight;
    LONG   crossExtent    = horizontal ? bandHeight : bandWidth;
    LONG   childCount     = (LONG) children.size();
    LONG   spacingTotal   = (childCount > 1) ? (LONG) (m_spacingDip * (float) (childCount - 1)) : 0;
    LONG   naturalSum     = 0;
    int    weightSum      = 0;
    LONG   cursor         = horizontal ? bandX : bandY;
    LONG   flexRemaining  = 0;
    int    weightsApplied = 0;


    if (children.empty())
    {
        return;
    }

    for (IDxuiControl * child : children)
    {
        auto  it     = m_weights.find (child);
        int   weight = (it == m_weights.end()) ? 0 : it->second;
        RECT  curBounds = child->Bounds();

        if (weight > 0)
        {
            weightSum += weight;
        }
        else
        {
            naturalSum += horizontal ? (curBounds.right - curBounds.left)
                                     : (curBounds.bottom - curBounds.top);
        }
    }

    flexRemaining = mainExtent - spacingTotal - naturalSum;
    if (flexRemaining < 0)
    {
        flexRemaining = 0;
    }

    for (size_t i = 0; i < children.size(); ++i)
    {
        IDxuiControl *  child     = children[i];
        auto            it        = m_weights.find (child);
        int             weight    = (it == m_weights.end()) ? 0 : it->second;
        RECT            curBounds = child->Bounds();
        LONG            mainSize  = horizontal ? (curBounds.right - curBounds.left)
                                               : (curBounds.bottom - curBounds.top);
        LONG            crossSize = horizontal ? (curBounds.bottom - curBounds.top)
                                               : (curBounds.right  - curBounds.left);
        LONG            crossOffset = 0;
        RECT            newBounds = {};

        if (weight > 0 && weightSum > 0)
        {
            mainSize = (LONG) ((int64_t) flexRemaining * (int64_t) weight / (int64_t) weightSum);
            // Last weighted child mops up rounding remainder.
            weightsApplied += weight;
            if (weightsApplied == weightSum)
            {
                LONG  consumed = cursor - (horizontal ? bandX : bandY);
                LONG  needTail = mainExtent - consumed;

                if (i + 1 == children.size())
                {
                    mainSize = needTail;
                }
            }
        }

        switch (m_crossAxisAlign)
        {
        case Align::Start:
            crossOffset = 0;
            break;
        case Align::Center:
            crossOffset = (crossExtent - crossSize) / 2;
            break;
        case Align::End:
            crossOffset = crossExtent - crossSize;
            break;
        case Align::Stretch:
            crossOffset = 0;
            crossSize   = crossExtent;
            break;
        }

        if (horizontal)
        {
            newBounds.left   = cursor;
            newBounds.top    = bandY + crossOffset;
            newBounds.right  = cursor + mainSize;
            newBounds.bottom = newBounds.top + crossSize;
        }
        else
        {
            newBounds.left   = bandX + crossOffset;
            newBounds.top    = cursor;
            newBounds.right  = newBounds.left + crossSize;
            newBounds.bottom = cursor + mainSize;
        }

        child->SetBounds (newBounds);

        cursor += mainSize;
        if (i + 1 < children.size())
        {
            cursor += (LONG) m_spacingDip;
        }
    }
}

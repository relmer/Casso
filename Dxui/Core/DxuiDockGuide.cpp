#include "Pch.h"

#include "Core/DxuiDockGuide.h"
#include "Render/DxuiCoverageRaster.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetSizePx
//
//  A guide is square: an inset, then one pitch for each button across it.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiDockGuide::GetSizePx (DxuiDockGuideKind kind, const DxuiDpiScaler & scaler)
{
    int  side = scaler.ToPx (kInsetDip + GetSlots (kind) * kPitchDip);



    return SIZE { side, side };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetOrigin
//
//  The top left of a guide centered on a point.
//
////////////////////////////////////////////////////////////////////////////////

POINT DxuiDockGuide::GetOrigin (DxuiDockGuideKind kind, POINT centerPx, const DxuiDpiScaler & scaler)
{
    SIZE  size = GetSizePx (kind, scaler);



    return POINT { centerPx.x - size.cx / 2, centerPx.y - size.cy / 2 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetOriginOfButton
//
//  The top left of the guide whose `button` lies at `buttonPx`, the inverse
//  of GetButtonRect.
//
////////////////////////////////////////////////////////////////////////////////

POINT DxuiDockGuide::GetOriginOfButton (DxuiDockGuideKind kind, DxuiDockGuideButton button, const RECT & buttonPx, const DxuiDpiScaler & scaler)
{
    POINT  dip = {};



    if (!TryGetButtonDip (kind, button, dip))
    {
        return POINT { buttonPx.left, buttonPx.top };
    }

    return POINT { buttonPx.left - scaler.ToPx ((int) dip.x), buttonPx.top - scaler.ToPx ((int) dip.y) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetButtonRect
//
//  Where a button of a guide at `originPx` lies, or an empty rect for a
//  button that kind of guide does not have.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockGuide::GetButtonRect (DxuiDockGuideKind kind, DxuiDockGuideButton button, POINT originPx, const DxuiDpiScaler & scaler)
{
    POINT  dip = {};



    if (!TryGetButtonDip (kind, button, dip))
    {
        return RECT {};
    }

    return RECT { originPx.x + scaler.ToPx ((int) dip.x),              originPx.y + scaler.ToPx ((int) dip.y),
                  originPx.x + scaler.ToPx ((int) dip.x + kButtonDip), originPx.y + scaler.ToPx ((int) dip.y + kButtonDip) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::IsInside
//
//  Whether a pixel's center lies inside the guide's outline.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockGuide::IsInside (DxuiDockGuideKind kind, POINT originPx, POINT point, const DxuiDpiScaler & scaler)
{
    constexpr float  kPixelMid = 0.5f;



    return DxuiCoverageRaster::IsInsidePolygon (GetOutline (kind, scaler),
                                                (float) (point.x - originPx.x) + kPixelMid,
                                                (float) (point.y - originPx.y) + kPixelMid);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetButtons
//
//  A small cross has the center and four dock buttons, a large one the split
//  buttons too, and an edge guide the one dock button for its edge.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDockGuideButton> DxuiDockGuide::GetButtons (DxuiDockGuideKind kind, DxuiDockSide edge)
{
    switch (kind)
    {
    case DxuiDockGuideKind::Edge:
        return { GetDockButton (edge) };

    case DxuiDockGuideKind::LargeCross:
        return { DxuiDockGuideButton::Center,
                 DxuiDockGuideButton::DockTop,  DxuiDockGuideButton::DockBottom,  DxuiDockGuideButton::DockLeft,  DxuiDockGuideButton::DockRight,
                 DxuiDockGuideButton::SplitTop, DxuiDockGuideButton::SplitBottom, DxuiDockGuideButton::SplitLeft, DxuiDockGuideButton::SplitRight };

    default:
        return { DxuiDockGuideButton::Center,
                 DxuiDockGuideButton::DockTop,  DxuiDockGuideButton::DockBottom,  DxuiDockGuideButton::DockLeft,  DxuiDockGuideButton::DockRight };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetDockButton
//
////////////////////////////////////////////////////////////////////////////////

DxuiDockGuideButton DxuiDockGuide::GetDockButton (DxuiDockSide side)
{
    switch (side)
    {
    case DxuiDockSide::Top:    return DxuiDockGuideButton::DockTop;
    case DxuiDockSide::Bottom: return DxuiDockGuideButton::DockBottom;
    case DxuiDockSide::Right:  return DxuiDockGuideButton::DockRight;
    default:                   return DxuiDockGuideButton::DockLeft;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetSplitButton
//
////////////////////////////////////////////////////////////////////////////////

DxuiDockGuideButton DxuiDockGuide::GetSplitButton (DxuiDockSide side)
{
    switch (side)
    {
    case DxuiDockSide::Top:    return DxuiDockGuideButton::SplitTop;
    case DxuiDockSide::Bottom: return DxuiDockGuideButton::SplitBottom;
    case DxuiDockSide::Right:  return DxuiDockGuideButton::SplitRight;
    default:                   return DxuiDockGuideButton::SplitLeft;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::Render
//
//  The cross's fill and its border ring, then the buttons. The fill runs
//  under the ring, so the ring's inner edge blends into the fill as Visual
//  Studio's does.
//
//  Every button but the one under the pointer is composed at full strength
//  on a layer of its own, which is then laid over the cross at kRestAlpha,
//  so each part of it fades together; the button under the pointer is drawn
//  last, opaque. An edge guide is composed whole and then faded whole unless
//  its button is under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

DxuiIconImage DxuiDockGuide::Render (DxuiDockGuideKind kind, DxuiDockSide edge, int hoveredButton, const DxuiDockGuideColors & colors, const DxuiDpiScaler & scaler)
{
    constexpr float                   kChannelMax = 255.0f;
    float                             rest        = (float) kRestAlpha / kChannelMax;
    SIZE                              size        = GetSizePx (kind, scaler);
    DxuiIconImage                     image       = DxuiCoverageRaster::MakeImage (size.cx, size.cy);
    DxuiIconImage                     layer;
    std::vector<DxuiPointF>           outline     = GetOutline (kind, scaler);
    std::vector<DxuiPointF>           inner       = DxuiCoverageRaster::InsetPolygon (outline, scaler.ToPxf (kBorderDip));
    std::vector<DxuiDockGuideButton>  buttons     = GetButtons (kind, edge);
    std::vector<DxuiDockGuideButton>  lit;
    std::vector<DxuiDockGuideButton>  resting;



    for (DxuiDockGuideButton button : buttons)
    {
        ((int) button == hoveredButton ? lit : resting).push_back (button);
    }

    DxuiCoverageRaster::FillPolygon     (image, outline, colors.fill);
    DxuiCoverageRaster::FillPolygonRing (image, outline, inner, colors.border);

    if (kind == DxuiDockGuideKind::Edge)
    {
        PaintButtons (image, kind, buttons, colors, scaler);

        if (lit.empty())
        {
            DxuiCoverageRaster::FadeImage (image, rest);
        }

        return image;
    }

    layer = DxuiCoverageRaster::MakeImage (size.cx, size.cy);

    PaintButtons                  (layer, kind, resting, colors, scaler);
    DxuiCoverageRaster::DrawImage (image, layer, rest);
    PaintButtons                  (image, kind, lit, colors, scaler);

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetOutline
//
//  An edge guide's box, or a cross: two arms as wide as a button and its
//  insets, each inside corner cut by a 45-degree chamfer. The arms' edges
//  land on whole pixels, and each chamfer runs at 45 degrees between them.
//  Clockwise from the top arm's left end.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPointF> DxuiDockGuide::GetOutline (DxuiDockGuideKind kind, const DxuiDpiScaler & scaler)
{
    int    slots = GetSlots (kind);
    float  size  = (float) scaler.ToPx (kInsetDip + slots * kPitchDip);
    float  low   = (float) scaler.ToPx (slots / 2 * kPitchDip);
    float  high  = (float) scaler.ToPx (slots / 2 * kPitchDip + kArmDip);
    float  leg   = scaler.ToPxf ((kind == DxuiDockGuideKind::LargeCross) ? kLargeChamferDip : kSmallChamferDip);



    if (kind == DxuiDockGuideKind::Edge)
    {
        return { DxuiPointF { 0.0f, 0.0f }, DxuiPointF { size, 0.0f }, DxuiPointF { size, size }, DxuiPointF { 0.0f, size } };
    }

    return { DxuiPointF { low,        0.0f       }, DxuiPointF { high,       0.0f       },
             DxuiPointF { high,       low - leg  }, DxuiPointF { high + leg, low        },
             DxuiPointF { size,       low        }, DxuiPointF { size,       high       },
             DxuiPointF { high + leg, high       }, DxuiPointF { high,       high + leg },
             DxuiPointF { high,       size       }, DxuiPointF { low,        size       },
             DxuiPointF { low,        high + leg }, DxuiPointF { low - leg,  high       },
             DxuiPointF { 0.0f,       high       }, DxuiPointF { 0.0f,       low        },
             DxuiPointF { low - leg,  low        }, DxuiPointF { low,        low - leg  } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetGlyphRect
//
//  The picture on a button, of the window with the part the pane would take:
//  the whole of it for the center and the split buttons, and the half on its
//  side for a dock button.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockGuide::GetGlyphRect (DxuiDockGuideKind kind, DxuiDockGuideButton button, const DxuiDpiScaler & scaler)
{
    POINT  dip   = {};
    RECT   glyph = GetGlyphDip (button);



    if (!TryGetButtonDip (kind, button, dip))
    {
        return RECT {};
    }

    return RECT { scaler.ToPx ((int) (dip.x + glyph.left)),  scaler.ToPx ((int) (dip.y + glyph.top)),
                  scaler.ToPx ((int) (dip.x + glyph.right)), scaler.ToPx ((int) (dip.y + glyph.bottom)) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetBandPx
//
//  How deep the solid title band across the top of every picture is.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiDockGuide::GetBandPx (const DxuiDpiScaler & scaler)
{
    return scaler.ToPxf (kBandDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetArrow
//
//  A dock button's triangle, its apex toward the picture: the apex, then
//  the two ends of its base. Empty for any other button.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPointF> DxuiDockGuide::GetArrow (DxuiDockGuideKind kind, DxuiDockGuideButton button, const DxuiDpiScaler & scaler)
{
    constexpr size_t             kCorners = 3;
    constexpr int                kMid     = kButtonDip / 2;
    constexpr int                kHalf    = kArrowHalfBaseDip;
    constexpr int                kApex    = kArrowApexDip;
    constexpr int                kBase    = kArrowBaseDip;
    std::array<POINT, kCorners>  corners  = {};
    std::vector<DxuiPointF>      arrow;
    POINT                        dip      = {};



    switch (button)
    {
    case DxuiDockGuideButton::DockTop:
        corners = { POINT { kMid, kApex }, POINT { kMid - kHalf, kBase }, POINT { kMid + kHalf, kBase } };
        break;

    case DxuiDockGuideButton::DockBottom:
        corners = { POINT { kMid, kButtonDip - kApex }, POINT { kMid - kHalf, kButtonDip - kBase }, POINT { kMid + kHalf, kButtonDip - kBase } };
        break;

    case DxuiDockGuideButton::DockLeft:
        corners = { POINT { kApex, kMid }, POINT { kBase, kMid - kHalf }, POINT { kBase, kMid + kHalf } };
        break;

    case DxuiDockGuideButton::DockRight:
        corners = { POINT { kButtonDip - kApex, kMid }, POINT { kButtonDip - kBase, kMid - kHalf }, POINT { kButtonDip - kBase, kMid + kHalf } };
        break;

    default:
        return arrow;
    }

    if (!TryGetButtonDip (kind, button, dip))
    {
        return arrow;
    }

    for (const POINT & corner : corners)
    {
        arrow.push_back (DxuiPointF { (float) scaler.ToPx ((int) (dip.x + corner.x)), (float) scaler.ToPx ((int) (dip.y + corner.y)) });
    }

    return arrow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetDots
//
//  A split button's dotted line, where the new tab group would meet the one
//  it splits: one dot every other DIP across the picture, just above or left
//  of its middle for a top or left split and just below or right of it for a
//  bottom or right one. Empty for any other button.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiCoverageRect> DxuiDockGuide::GetDots (DxuiDockGuideKind kind, DxuiDockGuideButton button, const DxuiDpiScaler & scaler)
{
    RECT                           glyph  = GetGlyphRect (kind, button, scaler);
    float                          dot    = scaler.ToPxf ((float) kDotDip);
    float                          left   = (float) glyph.left;
    float                          top    = (float) glyph.top;
    float                          across = 0.0f;
    bool                           isRow  = true;
    std::vector<DxuiCoverageRect>  dots;



    switch (button)
    {
    case DxuiDockGuideButton::SplitTop:    across = scaler.ToPxf ((float) kDotNearDip); isRow = true;  break;
    case DxuiDockGuideButton::SplitBottom: across = scaler.ToPxf ((float) kDotFarDip);  isRow = true;  break;
    case DxuiDockGuideButton::SplitLeft:   across = scaler.ToPxf ((float) kDotNearDip); isRow = false; break;
    case DxuiDockGuideButton::SplitRight:  across = scaler.ToPxf ((float) kDotFarDip);  isRow = false; break;
    default:                                                                                           return dots;
    }

    if (glyph.right <= glyph.left)
    {
        return dots;
    }

    for (int along = 0; along < kGlyphDip; along += kDotPitchDip)
    {
        float  start = scaler.ToPxf ((float) along);

        dots.push_back (isRow ? DxuiCoverageRect { left + start,  top + across, left + start + dot,  top + across + dot }
                              : DxuiCoverageRect { left + across, top + start,  left + across + dot, top + start + dot  });
    }

    return dots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetSlots
//
//  How many buttons a guide holds across its middle.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDockGuide::GetSlots (DxuiDockGuideKind kind)
{
    switch (kind)
    {
    case DxuiDockGuideKind::LargeCross: return kLargeSlots;
    case DxuiDockGuideKind::Edge:       return kEdgeSlots;
    default:                            return kSmallSlots;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::TryGetButtonDip
//
//  A button's top left in DIP from the guide's: the center button in the
//  middle slot, each dock button in the end slot on its side, and each
//  split button in the slot between. Only a large cross has split buttons,
//  and an edge guide has a dock button and nothing else.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockGuide::TryGetButtonDip (DxuiDockGuideKind kind, DxuiDockGuideButton button, POINT & dip)
{
    int   last    = GetSlots (kind) - 1;
    int   middle  = last / 2;
    int   column  = middle;
    int   row     = middle;
    bool  isDock  = false;
    bool  isSplit = false;
    bool  isValid = false;



    switch (button)
    {
    case DxuiDockGuideButton::DockTop:     row    = 0;          isDock  = true; break;
    case DxuiDockGuideButton::DockBottom:  row    = last;       isDock  = true; break;
    case DxuiDockGuideButton::DockLeft:    column = 0;          isDock  = true; break;
    case DxuiDockGuideButton::DockRight:   column = last;       isDock  = true; break;
    case DxuiDockGuideButton::SplitTop:    row    = middle - 1; isSplit = true; break;
    case DxuiDockGuideButton::SplitBottom: row    = middle + 1; isSplit = true; break;
    case DxuiDockGuideButton::SplitLeft:   column = middle - 1; isSplit = true; break;
    case DxuiDockGuideButton::SplitRight:  column = middle + 1; isSplit = true; break;
    default:                                                                    break;
    }

    switch (kind)
    {
    case DxuiDockGuideKind::Edge:       isValid = isDock;   break;
    case DxuiDockGuideKind::LargeCross: isValid = true;     break;
    default:                            isValid = !isSplit; break;
    }

    dip = POINT { kInsetDip + column * kPitchDip, kInsetDip + row * kPitchDip };

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetGlyphDip
//
//  Where a button's picture lies, in DIP from the button's top left.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockGuide::GetGlyphDip (DxuiDockGuideButton button)
{
    constexpr long  kNear = kInsetDip;
    constexpr long  kMid  = kInsetDip + kGlyphHalfDip;
    constexpr long  kFar  = kInsetDip + kGlyphDip;



    switch (button)
    {
    case DxuiDockGuideButton::DockTop:    return RECT { kNear, kNear, kFar, kMid };
    case DxuiDockGuideButton::DockBottom: return RECT { kNear, kMid,  kFar, kFar };
    case DxuiDockGuideButton::DockLeft:   return RECT { kNear, kNear, kMid, kFar };
    case DxuiDockGuideButton::DockRight:  return RECT { kMid,  kNear, kFar, kFar };
    default:                              return RECT { kNear, kNear, kFar, kFar };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetButtonBox
//
////////////////////////////////////////////////////////////////////////////////

DxuiCoverageRect DxuiDockGuide::GetButtonBox (DxuiDockGuideKind kind, DxuiDockGuideButton button, const DxuiDpiScaler & scaler)
{
    RECT   rect   = GetButtonRect (kind, button, POINT {}, scaler);
    float  radius = scaler.ToPxf (kButtonRadiusDip);



    return DxuiCoverageRect { (float) rect.left, (float) rect.top, (float) rect.right, (float) rect.bottom, radius, radius };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::GetBorderHole
//
//  The inside of a one-pixel border around a rounded rectangle: the
//  rectangle one pixel in, its corners following the outer ones.
//
////////////////////////////////////////////////////////////////////////////////

DxuiCoverageRect DxuiDockGuide::GetBorderHole (const DxuiCoverageRect & rect)
{
    constexpr float  kLinePx = 1.0f;



    return DxuiCoverageRect { rect.left  + kLinePx, rect.top    + kLinePx,
                              rect.right - kLinePx, rect.bottom - kLinePx,
                              std::max (0.0f, rect.topRadius - kLinePx), std::max (0.0f, rect.bottomRadius - kLinePx) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::PaintGlyph
//
//  A button's picture: a frame one pixel wide, square at the top and rounded
//  at the bottom, whose top is the solid title band.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGuide::PaintGlyph (DxuiIconImage & image, DxuiDockGuideKind kind, DxuiDockGuideButton button, uint32_t argb, const DxuiDpiScaler & scaler)
{
    constexpr float   kLinePx = 1.0f;
    RECT              glyph   = GetGlyphRect (kind, button, scaler);
    float             radius  = scaler.ToPxf (kGlyphRadiusDip);
    DxuiCoverageRect  outer   = { (float) glyph.left, (float) glyph.top, (float) glyph.right, (float) glyph.bottom, 0.0f, radius };
    DxuiCoverageRect  hole    = { outer.left  + kLinePx, outer.top    + GetBandPx (scaler),
                                  outer.right - kLinePx, outer.bottom - kLinePx,
                                  0.0f,                  std::max (0.0f, radius - kLinePx) };



    if (glyph.right <= glyph.left)
    {
        return;
    }

    DxuiCoverageRaster::FillRoundedRing (image, outer, hole, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide::PaintButtons
//
//  In Visual Studio's order: the buttons' fills, then their borders, the
//  pictures of where the pane would go, the dots of a split, and the arrows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockGuide::PaintButtons (
    DxuiIconImage                           & image,
    DxuiDockGuideKind                         kind,
    const std::vector<DxuiDockGuideButton>  & buttons,
    const DxuiDockGuideColors               & colors,
    const DxuiDpiScaler                     & scaler)
{
    for (DxuiDockGuideButton button : buttons)
    {
        DxuiCoverageRaster::FillRoundedRect (image, GetButtonBox (kind, button, scaler), colors.buttonFill);
    }

    for (DxuiDockGuideButton button : buttons)
    {
        DxuiCoverageRect  box = GetButtonBox (kind, button, scaler);

        DxuiCoverageRaster::FillRoundedRing (image, box, GetBorderHole (box), colors.buttonBorder);
    }

    for (DxuiDockGuideButton button : buttons)
    {
        PaintGlyph (image, kind, button, colors.glyph, scaler);
    }

    for (DxuiDockGuideButton button : buttons)
    {
        DxuiCoverageRaster::FillRects (image, GetDots (kind, button, scaler), colors.glyph);
    }

    for (DxuiDockGuideButton button : buttons)
    {
        DxuiCoverageRaster::FillPolygon (image, GetArrow (kind, button, scaler), colors.arrow);
    }
}






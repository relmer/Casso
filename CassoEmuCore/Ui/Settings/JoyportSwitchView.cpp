#include "Pch.h"

#include "Ui/Settings/JoyportSwitchView.h"

#include "Render/IDxuiPainter.h"
#include "Theme/IDxuiTheme.h"





// The stick is drawn in its own colors rather than the theme's: it is a
// picture of a black plastic joystick with orange markings and a red button,
// and those are what make it recognizable on any theme.
static constexpr uint32_t  s_kBodyEdgeColor   = 0xFF3A3A3A;
static constexpr uint32_t  s_kBodyColor       = 0xFF1B1B1B;
static constexpr uint32_t  s_kRingPlateColor  = 0xFF0C0C0C;
static constexpr uint32_t  s_kMarkingColor    = 0xFFE8892A;
static constexpr uint32_t  s_kLitColor        = 0xFFFFF4C8;
static constexpr uint32_t  s_kBootColor       = 0xFF141414;
static constexpr uint32_t  s_kBootRidgeColor  = 0xFF2E2E2E;
static constexpr uint32_t  s_kShaftColor      = 0xFF222222;
static constexpr uint32_t  s_kShaftRimColor   = 0xFF404040;
static constexpr uint32_t  s_kFireRimColor    = 0xFF6E170C;
static constexpr uint32_t  s_kFireColor       = 0xFFC9301C;
static constexpr uint32_t  s_kFireLitColor    = 0xFFFF6A48;

static constexpr const wchar_t *  s_kpszMarkingFont = L"Segoe UI";

// Proportions, as fractions of the square's side, read off the real stick.
// The ring is centered on the base, so its margin to each edge is the same;
// the fire button is a little smaller than the stick and sits in the top-left
// corner between the ring and the base's rounded corner.
static constexpr float  s_kCornerRadius   = 0.09f;
static constexpr float  s_kBevel          = 0.025f;
static constexpr float  s_kRingCenter     = 0.5f;
static constexpr float  s_kRingOuter      = 0.37f;
static constexpr float  s_kRingInner      = 0.352f;
static constexpr float  s_kRingPlate      = 0.42f;
static constexpr float  s_kFireSurround   = 0.025f;
static constexpr float  s_kPlateFillet    = 0.03f;
static constexpr float  s_kTopHeight      = 0.058f;
static constexpr float  s_kFireCenter     = 0.155f;
static constexpr float  s_kFireCap        = 0.82f;
static constexpr float  s_kFireScale      = 0.9f;
static constexpr float  s_kBootRadius     = 0.25f;
static constexpr float  s_kShaftRadius    = 0.09f;
static constexpr float  s_kShaftTravel    = 0.06f;
static constexpr float  s_kRidgeThickness = 0.008f;

// The stick's top is a hexagon with rounded corners, each corner's radius a
// fraction of the hexagon's.
static constexpr float  s_kStickRounding  = 0.25f;
static constexpr int    s_kHexagonSides   = 6;

// Each quadrant of the ring, from one cardinal to the next, in degrees: half a
// gap, a marker piece, a gap, seven dashes with a gap between each, a gap,
// the next marker's piece, and half a gap -- 1.5 + 10.5 + 3 + 60 + 3 + 10.5 +
// 1.5 = 90.
static constexpr float  s_kGapDeg         = 3.0f;
static constexpr float  s_kDashDeg        = 6.0f;
static constexpr float  s_kMarkerDeg      = 10.5f;

// How far a marker piece's triangle reaches outside the ring, in ring
// thicknesses.
static constexpr float  s_kMarkerDepth    = 1.75f;

static constexpr float  s_kDegRight       = 0.0f;
static constexpr float  s_kDegDown        = 90.0f;
static constexpr float  s_kDegLeft        = 180.0f;
static constexpr float  s_kDegUp          = 270.0f;
static constexpr int    s_kCornerSteps    = 6;
static constexpr int    s_kQuadrantCount  = 4;
static constexpr float  s_kArcStepDeg     = 5.0f;
static constexpr float  s_kFullTurnDeg    = 360.0f;
static constexpr float  s_kHalfTurnDeg    = 180.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void JoyportSwitchView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_scaler.SetDpi (scaler.GetDpi());
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildArt
//
//  Every shape of the stick for a square at (left, top) of the given side.
//  Angles run clockwise from the right, as screen coordinates do: 0 right,
//  90 down, 180 left, 270 up. Each quadrant of the ring holds seven dashes,
//  with a marker piece at either end; at up, TOP takes the place of the two
//  pieces.
//
////////////////////////////////////////////////////////////////////////////////

JoyportStickArt JoyportSwitchView::BuildArt (float left, float top, float side)
{
    JoyportStickArt  art;
    float            firstDash = s_kGapDeg * 0.5f + s_kMarkerDeg + s_kGapDeg;
    float            fromDeg   = 0.0f;
    int              quadrant  = 0;
    int              dash      = 0;



    art.body        = BuildRoundedSquare (left, top, side, side * s_kCornerRadius);
    art.bodyTop     = BuildRoundedSquare (left + side * s_kBevel, top + side * s_kBevel,
                                          side * (1.0f - 2.0f * s_kBevel), side * (s_kCornerRadius - s_kBevel));
    art.ringCenter  = { left + side * s_kRingCenter, top + side * s_kRingCenter };
    art.ringOuter   = side * s_kRingOuter;
    art.ringInner   = side * s_kRingInner;
    art.topCenter   = PointAt (art.ringCenter, (art.ringInner + art.ringOuter) * 0.5f, s_kDegUp);
    art.topHeight   = side * s_kTopHeight;
    art.fireCenter  = { left + side * s_kFireCenter, top + side * s_kFireCenter };
    art.fireRadius  = side * s_kShaftRadius * s_kFireScale;
    art.bootRadius  = side * s_kBootRadius;
    art.shaftRadius = side * s_kShaftRadius;
    art.shaftTravel = side * s_kShaftTravel;

    // The plate's margin inside the dashes is half its margin outside them.
    art.plate       = BuildPlate (art.ringCenter, side * s_kRingPlate, art.fireCenter, art.fireRadius + side * s_kFireSurround,
                                  side * s_kPlateFillet);
    art.plateInner  = art.ringInner - (side * s_kRingPlate - art.ringOuter) * 0.5f;

    for (quadrant = 0; quadrant < s_kQuadrantCount; quadrant++)
    {
        for (dash = 0; dash < kDashesPerQuadrant; dash++)
        {
            fromDeg = s_kDegDown * (float) quadrant + firstDash + (s_kDashDeg + s_kGapDeg) * (float) dash;

            art.dashes.push_back (BuildDash (art.ringCenter, art.ringInner, art.ringOuter, fromDeg, fromDeg + s_kDashDeg));
        }
    }

    for (auto [direction, degrees] : { std::pair { JoystickSwitch::Right, s_kDegRight },
                                       std::pair { JoystickSwitch::Down,  s_kDegDown  },
                                       std::pair { JoystickSwitch::Left,  s_kDegLeft  } })
    {
        JoyportStickMarker  marker;

        marker.direction = direction;
        marker.pieces[0] = BuildMarkerPiece (art.ringCenter, art.ringInner, art.ringOuter, degrees, -1.0f);
        marker.pieces[1] = BuildMarkerPiece (art.ringCenter, art.ringInner, art.ringOuter, degrees,  1.0f);
        marker.center    = PointAt (art.ringCenter, art.ringInner, degrees);
        art.markers.push_back (std::move (marker));
    }

    return art;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildRoundedSquare
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPointF> JoyportSwitchView::BuildRoundedSquare (float left, float top, float side, float radius)
{
    std::vector<DxuiPointF>  points;
    float                    right  = left + side;
    float                    bottom = top + side;
    int                      step   = 0;



    // Each corner's arc, clockwise from the top-right.
    const std::array<std::pair<DxuiPointF, float>, 4>  corners =
    {{
        { { right - radius, top    + radius }, s_kDegUp    },
        { { right - radius, bottom - radius }, s_kDegRight },
        { { left  + radius, bottom - radius }, s_kDegDown  },
        { { left  + radius, top    + radius }, s_kDegLeft  },
    }};

    for (const auto & [center, startDeg] : corners)
    {
        for (step = 0; step <= s_kCornerSteps; step++)
        {
            points.push_back (PointAt (center, radius, startDeg + s_kDegDown * (float) step / (float) s_kCornerSteps));
        }
    }

    return points;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildPlate
//
//  The dark plate the ring is painted on, with the lobe that wraps the fire
//  button: the ring's circle and the button's joined by two fillet arcs, each
//  tangent to both circles, so the plate flows out to the button the way the
//  molding does. A fillet's center lies filletRadius outside both circles.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPointF> JoyportSwitchView::BuildPlate (DxuiPointF ringCenter, float ringRadius,
                                                       DxuiPointF fireCenter, float fireRadius, float filletRadius)
{
    std::vector<DxuiPointF>  points;
    float                    dx       = fireCenter.x - ringCenter.x;
    float                    dy       = fireCenter.y - ringCenter.y;
    float                    distance = std::sqrt (dx * dx + dy * dy);
    float                    toRing   = ringRadius + filletRadius;
    float                    toFire   = fireRadius + filletRadius;
    float                    along    = (toRing * toRing - toFire * toFire + distance * distance) / (2.0f * distance);
    float                    across   = std::sqrt (std::max (0.0f, toRing * toRing - along * along));
    DxuiPointF               unit     = { dx / distance, dy / distance };
    DxuiPointF               fillet0  = { ringCenter.x + unit.x * along - unit.y * across, ringCenter.y + unit.y * along + unit.x * across };
    DxuiPointF               fillet1  = { ringCenter.x + unit.x * along + unit.y * across, ringCenter.y + unit.y * along - unit.x * across };



    AppendArc (points, ringCenter, ringRadius,   AngleOf (ringCenter, fillet0), AngleOf (ringCenter, fillet1), true);
    AppendArc (points, fillet1,    filletRadius, AngleOf (fillet1, ringCenter), AngleOf (fillet1, fireCenter), false);
    AppendArc (points, fireCenter, fireRadius,   AngleOf (fireCenter, fillet1), AngleOf (fireCenter, fillet0), true);
    AppendArc (points, fillet0,    filletRadius, AngleOf (fillet0, fireCenter), AngleOf (fillet0, ringCenter), false);

    return points;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendArc
//
//  The arc of a circle from one angle to another, the long way round or the
//  short, with a point every few degrees.
//
////////////////////////////////////////////////////////////////////////////////

void JoyportSwitchView::AppendArc (std::vector<DxuiPointF> & points, DxuiPointF center, float radius, float fromDeg, float toDeg, bool isLongWay)
{
    float  sweep = std::fmod (toDeg - fromDeg, s_kFullTurnDeg);
    int    steps = 0;
    int    step  = 0;



    if (sweep > s_kHalfTurnDeg)
    {
        sweep -= s_kFullTurnDeg;
    }
    else if (sweep <= -s_kHalfTurnDeg)
    {
        sweep += s_kFullTurnDeg;
    }

    if (isLongWay)
    {
        sweep += (sweep > 0.0f) ? -s_kFullTurnDeg : s_kFullTurnDeg;
    }

    steps = std::max (2, (int) std::ceil (std::abs (sweep) / s_kArcStepDeg));

    for (step = 0; step <= steps; step++)
    {
        points.push_back (PointAt (center, radius, fromDeg + sweep * (float) step / (float) steps));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AngleOf
//
////////////////////////////////////////////////////////////////////////////////

float JoyportSwitchView::AngleOf (DxuiPointF center, DxuiPointF point)
{
    return std::atan2 (point.y - center.y, point.x - center.x) * s_kHalfTurnDeg / std::numbers::pi_v<float>;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildRoundedHexagon
//
//  A regular hexagon of the given radius, center to corner, with a corner at
//  the right, and each corner rounded by an arc of the given radius. An arc's
//  center lies on its corner's spoke, cornerRadius / sin 60 in from the
//  corner, and the arc spans the 60 degrees between the two edges' normals.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPointF> JoyportSwitchView::BuildRoundedHexagon (DxuiPointF center, float radius, float cornerRadius)
{
    constexpr float          kSideDeg = 360.0f / (float) s_kHexagonSides;
    std::vector<DxuiPointF>  points;
    float                    inset    = cornerRadius / std::sin (kSideDeg * std::numbers::pi_v<float> / 180.0f);
    int                      corner   = 0;
    int                      step     = 0;



    for (corner = 0; corner < s_kHexagonSides; corner++)
    {
        float       cornerDeg = kSideDeg * (float) corner;
        DxuiPointF  arcCenter = PointAt (center, radius - inset, cornerDeg);

        for (step = 0; step <= s_kCornerSteps; step++)
        {
            points.push_back (PointAt (arcCenter, cornerRadius, cornerDeg - kSideDeg * 0.5f + kSideDeg * (float) step / (float) s_kCornerSteps));
        }
    }

    return points;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildDash
//
//  One segment of the ring between two angles: the outer edge out, then the
//  inner edge back, each through its middle so the curve reads as a curve.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPointF> JoyportSwitchView::BuildDash (DxuiPointF center, float inner, float outer, float fromDeg, float toDeg)
{
    float  midDeg = (fromDeg + toDeg) * 0.5f;



    return
    {
        PointAt (center, outer, fromDeg),
        PointAt (center, outer, midDeg),
        PointAt (center, outer, toDeg),
        PointAt (center, inner, toDeg),
        PointAt (center, inner, midDeg),
        PointAt (center, inner, fromDeg),
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildMarkerPiece
//
//  Half of a cardinal marker, on the side of the cardinal `sign` gives: a
//  dash, longer than the ring's others, extended by a right triangle. One
//  leg lies along the dash's outer edge and the other runs straight outward
//  from the dash's end nearest the cardinal; the hypotenuse meets the outer
//  edge at its midpoint. So the piece points out of the ring, reaching
//  farthest beside the cardinal and tapering to a plain dash away from it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPointF> JoyportSwitchView::BuildMarkerPiece (DxuiPointF center, float inner, float outer, float cardinalDeg, float sign)
{
    float  depth   = (outer - inner) * s_kMarkerDepth;
    float  nearDeg = cardinalDeg + sign * s_kGapDeg * 0.5f;
    float  farDeg  = nearDeg + sign * s_kMarkerDeg;
    float  midDeg  = nearDeg + sign * s_kMarkerDeg * 0.5f;



    return
    {
        PointAt (center, inner,         nearDeg),
        PointAt (center, inner,         midDeg),
        PointAt (center, inner,         farDeg),
        PointAt (center, outer,         farDeg),
        PointAt (center, outer,         midDeg),
        PointAt (center, outer + depth, nearDeg),
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  PointAt
//
////////////////////////////////////////////////////////////////////////////////

DxuiPointF JoyportSwitchView::PointAt (DxuiPointF center, float radius, float degrees)
{
    float  radians = degrees * std::numbers::pi_v<float> / 180.0f;



    return { center.x + radius * std::cos (radians), center.y + radius * std::sin (radians) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Back to front: the base with its bevel, the dark plate under the ring and
//  around the fire button, with the base showing again inside the ring, the
//  dashes, the markers and TOP, the rubber boot and the stick, and the fire
//  button. With no reading nothing lights, the way an unplugged stick reads.
//
////////////////////////////////////////////////////////////////////////////////

void JoyportSwitchView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT             bounds = GetBounds();
    float            side   = (float) std::min (bounds.right - bounds.left, bounds.bottom - bounds.top);
    JoyportStickArt  art;
    HRESULT          hr     = S_OK;



    UNREFERENCED_PARAMETER (painter);
    UNREFERENCED_PARAMETER (theme);

    if (!IsVisible() || side <= 0.0f)
    {
        return;
    }

    art = BuildArt ((float) bounds.left, (float) bounds.top, side);

    hr = text.FillPolygon (art.body.data(), art.body.size(), s_kBodyEdgeColor);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillPolygon (art.bodyTop.data(), art.bodyTop.size(), s_kBodyColor);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillPolygon (art.plate.data(), art.plate.size(), s_kRingPlateColor);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillEllipse (art.ringCenter.x, art.ringCenter.y, art.plateInner, art.plateInner, s_kBodyColor);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (const std::vector<DxuiPointF> & dash : art.dashes)
    {
        hr = text.FillPolygon (dash.data(), dash.size(), s_kMarkingColor);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    PaintMarkers (text, art);
    PaintStick   (text, art);
    PaintFire    (text, art);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintMarkers
//
//  A lit marker, or TOP, is drawn in the lit color.
//
////////////////////////////////////////////////////////////////////////////////

void JoyportSwitchView::PaintMarkers (IDxuiTextRenderer & text, const JoyportStickArt & art) const
{
    bool     isUp  = IsLit (JoystickSwitch::Up);
    HRESULT  hr    = S_OK;



    for (const JoyportStickMarker & marker : art.markers)
    {
        bool  isLit = IsLit (marker.direction);

        for (const std::vector<DxuiPointF> & piece : marker.pieces)
        {
            hr = text.FillPolygon (piece.data(), piece.size(), isLit ? s_kLitColor : s_kMarkingColor);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    hr = text.DrawString (L"TOP", art.topCenter.x - art.topHeight * 2.0f, art.topCenter.y - art.topHeight,
                          art.topHeight * 4.0f, art.topHeight * 2.0f,
                          isUp ? s_kLitColor : s_kMarkingColor, art.topHeight, s_kpszMarkingFont,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Bold, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintStick
//
//  The rubber boot's ridges around the base of the stick, and the stick's
//  rounded-hexagon top with its rim, leaning toward whichever switches are
//  closed so a diagonal leans diagonally.
//
////////////////////////////////////////////////////////////////////////////////

void JoyportSwitchView::PaintStick (IDxuiTextRenderer & text, const JoyportStickArt & art) const
{
    constexpr float          kRidges[]  = { 0.9f, 0.75f, 0.6f };
    constexpr float          kDiagonal  = 0.7071f;
    float                    ridge      = art.topHeight / s_kTopHeight * s_kRidgeThickness;
    float                    dx         = (IsLit (JoystickSwitch::Right) ? 1.0f : 0.0f) - (IsLit (JoystickSwitch::Left) ? 1.0f : 0.0f);
    float                    dy         = (IsLit (JoystickSwitch::Down)  ? 1.0f : 0.0f) - (IsLit (JoystickSwitch::Up)   ? 1.0f : 0.0f);
    float                    scale      = (dx != 0.0f && dy != 0.0f) ? kDiagonal : 1.0f;
    float                    shaftX     = art.ringCenter.x + dx * scale * art.shaftTravel;
    float                    shaftY     = art.ringCenter.y + dy * scale * art.shaftTravel;
    float                    rimRadius  = art.shaftRadius + ridge;
    float                    capRadius  = art.shaftRadius - ridge;
    std::vector<DxuiPointF>  rim        = BuildRoundedHexagon ({ shaftX, shaftY }, rimRadius, rimRadius * s_kStickRounding);
    std::vector<DxuiPointF>  cap        = BuildRoundedHexagon ({ shaftX, shaftY }, capRadius, capRadius * s_kStickRounding);
    HRESULT                  hr         = S_OK;



    hr = text.FillEllipse (art.ringCenter.x, art.ringCenter.y, art.bootRadius, art.bootRadius, s_kBootColor);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (float fraction : kRidges)
    {
        hr = text.DrawEllipse (art.ringCenter.x, art.ringCenter.y, art.bootRadius * fraction, art.bootRadius * fraction,
                               ridge, s_kBootRidgeColor);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    hr = text.FillPolygon (rim.data(), rim.size(), s_kShaftRimColor);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillPolygon (cap.data(), cap.size(), s_kShaftColor);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintFire
//
////////////////////////////////////////////////////////////////////////////////

void JoyportSwitchView::PaintFire (IDxuiTextRenderer & text, const JoyportStickArt & art) const
{
    bool     isLit = IsLit (JoystickSwitch::Fire);
    float    cap   = art.fireRadius * s_kFireCap;
    HRESULT  hr    = S_OK;



    hr = text.FillEllipse (art.fireCenter.x, art.fireCenter.y, art.fireRadius, art.fireRadius, s_kFireRimColor);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillEllipse (art.fireCenter.x, art.fireCenter.y, cap, cap, isLit ? s_kFireLitColor : s_kFireColor);
    IGNORE_RETURN_VALUE (hr, S_OK);
}

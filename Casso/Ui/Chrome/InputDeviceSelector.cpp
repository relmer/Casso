#include "Pch.h"

#include "InputDeviceSelector.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Palette — transcribed from the SVG masters (warm ABS beige family, the
//  fire-button orange, and the drive-bar LED blue for selection).
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr uint32_t  kCase       = 0xFFD8D2C1;   // body plastic
    constexpr uint32_t  kCaseLight  = 0xFFE2DDCD;   // top faces / plateau
    constexpr uint32_t  kCaseEdge   = 0xFF8F8A7A;   // molded edge stroke
    constexpr uint32_t  kSideFace   = 0xFFA9A392;   // oblique right-side faces
    constexpr uint32_t  kFacetTop   = 0xFFC7C1B1;   // funnel facets, light..dark
    constexpr uint32_t  kFacetLeft  = 0xFFB5AF9E;
    constexpr uint32_t  kFacetRight = 0xFFA39D8C;
    constexpr uint32_t  kFacetBot   = 0xFF948E7D;
    constexpr uint32_t  kHole       = 0xFF6B6759;   // pivot hole / well opening
    constexpr uint32_t  kKnob       = 0xFFB9B4A6;   // stick knob / dial caps
    constexpr uint32_t  kKnobEdge   = 0xFF6E6A5C;
    constexpr uint32_t  kDial       = 0xFFABA592;   // paddle dial body
    constexpr uint32_t  kDialSide   = 0xFFA79F8D;   // cylinder side bands
    constexpr uint32_t  kDialTop    = 0xFFC1BBAA;
    constexpr uint32_t  kDialEdge   = 0xFF827D6C;
    constexpr uint32_t  kTick       = 0xFF7E7967;   // knurl ticks
    constexpr uint32_t  kRib        = 0xFFA9A392;   // grip ribs
    constexpr uint32_t  kOrange     = 0xFFF0602B;   // fire buttons
    constexpr uint32_t  kOrangeEdge = 0xFFA63C14;
    constexpr uint32_t  kMouseBtn   = 0xFFB0ADA4;   // mouse button gray
    constexpr uint32_t  kMouseBtnEdge = 0xFF6E6B62;
    constexpr uint32_t  kStick      = 0xFF3A3733;   // joystick grip
    constexpr uint32_t  kShaft      = 0xFF7A6A4E;   // brass shaft
    constexpr uint32_t  kHighlight  = 0x59FFFFFF;   // specular highlights
    constexpr uint32_t  kSeam       = 0xB88F8A7A;   // case seam lines

    constexpr uint32_t  kSelAccent  = 0xFF3DA1FF;   // LED blue (drive bar)
    constexpr uint32_t  kSelGlow    = 0x2E3DA1FF;
    constexpr uint32_t  kChipBg     = 0x1F000000;
    constexpr uint32_t  kChipBgHot  = 0x14FFFFFF;
    constexpr uint32_t  kChipEdge   = 0x5A8F8A7A;

    // Tooltips per display state.
    constexpr wchar_t  kTipOff[] =
        L"Input devices: joystick (arrow keys), paddle, or mouse.\n"
        L"Click a device to connect it to the game port.";
    constexpr wchar_t  kTipJoystick[] =
        L"Arrows, Z, and X keys are mapped to the joystick and its buttons.";
    constexpr wchar_t  kTipPaddle[] =
        L"The mouse drives paddles 0 and 1; left / right buttons fire.\n"
        L"Press Esc to release the mouse and exit paddle mode.";
    constexpr wchar_t  kTipMouse[] =
        L"The host pointer drives the built-in mouse while over the screen\n"
        L"(non-capturing).";
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  boundsDip carries the anchor CENTER point. Chips are laid out
//  [J] | [P][M] around it; the group gap gets a divider at paint time.
//
////////////////////////////////////////////////////////////////////////////////

void InputDeviceSelector::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  chip   = scaler.Px (kChipDp);
    int  gap    = scaler.Px (kChipGapDp);
    int  ggap   = scaler.Px (kGroupGapDp);
    int  pad    = scaler.Px (kPadDp);
    int  n      = SegmentCount ();
    int  w      = pad * 2 + chip * n + ggap + (n == 3 ? gap : 0);
    int  h      = pad * 2 + chip;
    int  cx     = boundsDip.left;
    int  cy     = boundsDip.top;
    int  x      = cx - w / 2;
    int  y      = cy - h / 2;


    m_dpi    = scaler.Dpi ();
    m_bounds = RECT { x, y, x + w, y + h };

    int  sx = x + pad;

    // Keys group: joystick.
    m_segRects[0] = RECT { sx, y + pad, sx + chip, y + pad + chip };
    sx += chip + ggap;

    // Pointer group: paddle [+ mouse].
    m_segRects[1] = RECT { sx, y + pad, sx + chip, y + pad + chip };
    sx += chip + gap;

    if (n == 3)
    {
        m_segRects[2] = RECT { sx, y + pad, sx + chip, y + pad + chip };
    }
    else
    {
        m_segRects[2] = RECT {};
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Hit testing
//
////////////////////////////////////////////////////////////////////////////////

bool InputDeviceSelector::HitTest (int x, int y) const
{
    return x >= m_bounds.left && x < m_bounds.right &&
           y >= m_bounds.top  && y < m_bounds.bottom;
}


InputDeviceSelector::Segment InputDeviceSelector::SegmentAt (int x, int y) const
{
    static constexpr Segment  kOrder[3] = { Segment::Joystick, Segment::Paddle, Segment::Mouse };

    for (int i = 0; i < SegmentCount (); i++)
    {
        const RECT & r = m_segRects[i];
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
        {
            return kOrder[i];
        }
    }
    return Segment::None;
}


bool InputDeviceSelector::SegmentSelected (int index) const
{
    switch (index)
    {
        case 0:  return m_arrowsJoystick;
        case 1:  return m_pointer == InputMappingMode::Paddle;
        case 2:  return m_pointer == InputMappingMode::Mouse;
        default: return false;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  TooltipText
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * InputDeviceSelector::TooltipText () const
{
    if (m_pointer == InputMappingMode::Mouse)  return kTipMouse;
    if (m_pointer == InputMappingMode::Paddle) return kTipPaddle;
    if (m_arrowsJoystick)                      return kTipJoystick;
    return kTipOff;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void InputDeviceSelector::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);

    if (m_bounds.right <= m_bounds.left)
    {
        return;                       // hidden
    }

    float  inset = (float) (m_dpi) / 96.0f * 2.0f;

    for (int i = 0; i < SegmentCount (); i++)
    {
        const RECT & r   = m_segRects[i];
        bool         sel = SegmentSelected (i);
        float        x   = (float) r.left;
        float        y   = (float) r.top;
        float        w   = (float) (r.right - r.left);
        float        h   = (float) (r.bottom - r.top);

        painter.FillRect (x, y, w, h, (m_hovered && !sel) ? kChipBgHot : kChipBg);

        if (sel)
        {
            painter.FillRect    (x, y, w, h, kSelGlow);
            painter.OutlineRect (x, y, w, h, inset, kSelAccent);
        }
        else
        {
            painter.OutlineRect (x, y, w, h, 1.0f, kChipEdge);
        }

        RECT  box = { r.left   + (LONG) inset, r.top    + (LONG) inset,
                      r.right  - (LONG) inset, r.bottom - (LONG) inset };

        switch (i)
        {
            case 0: PaintJoystickGlyph (painter, box, m_skeuoStyle); break;
            case 1: PaintPaddleGlyph   (painter, box, m_skeuoStyle); break;
            case 2: PaintMouseGlyph    (painter, box, m_skeuoStyle); break;
        }
    }

    // Divider between the Keys and Pointer groups.
    {
        float  dx = (float) (m_segRects[0].right + m_segRects[1].left) * 0.5f;
        painter.FillRect (dx, (float) m_bounds.top + 3.0f, 1.0f,
                          (float) (m_bounds.bottom - m_bounds.top) - 6.0f, kChipEdge);
    }

    if (m_focused)
    {
        painter.OutlineRect ((float) m_bounds.left - 1, (float) m_bounds.top - 1,
                             (float) (m_bounds.right - m_bounds.left) + 2,
                             (float) (m_bounds.bottom - m_bounds.top) + 2,
                             1.0f, kSelAccent);
    }

    if (m_pressed)
    {
        painter.FillRect ((float) m_bounds.left, (float) m_bounds.top,
                          (float) (m_bounds.right - m_bounds.left),
                          (float) (m_bounds.bottom - m_bounds.top), 0x28000000);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Glyph painters — direct transcriptions of the SVG masters
//  (Assets/DesignSources/InputIcons, 96x96 grids). X()/Y() map master
//  coordinates into the target box.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    struct GlyphMap
    {
        float  bx, by, s;
        GlyphMap (const RECT & box)
        {
            bx = (float) box.left;
            by = (float) box.top;
            s  = (float) (box.right - box.left) / 96.0f;
        }
        float  X (float v) const { return bx + v * s; }
        float  Y (float v) const { return by + v * s; }
        float  S (float v) const { return v * s; }
    };
}


void InputDeviceSelector::PaintJoystickGlyph (IDxuiPainter & p, const RECT & box, bool skeuo)
{
    GlyphMap  g (box);

    if (!skeuo)
    {
        // Top-down (joystick-icon.svg).
        p.FillRect        (g.X (6),  g.Y (6),  g.S (84), g.S (84), kCase);
        p.OutlineRect     (g.X (6),  g.Y (6),  g.S (84), g.S (84), g.S (2), kCaseEdge);
        p.FillConvexQuad  (g.X (37), g.Y (37), g.X (85), g.Y (37), g.X (69), g.Y (53), g.X (53), g.Y (53), kFacetTop);
        p.FillConvexQuad  (g.X (37), g.Y (37), g.X (53), g.Y (53), g.X (53), g.Y (69), g.X (37), g.Y (85), kFacetLeft);
        p.FillConvexQuad  (g.X (85), g.Y (37), g.X (85), g.Y (85), g.X (69), g.Y (69), g.X (69), g.Y (53), kFacetRight);
        p.FillConvexQuad  (g.X (37), g.Y (85), g.X (85), g.Y (85), g.X (69), g.Y (69), g.X (53), g.Y (69), kFacetBot);
        p.FillRect        (g.X (53), g.Y (53), g.S (16), g.S (16), kHole);
        p.FillCircleApprox (g.X (61), g.Y (61), g.S (11), kKnobEdge);
        p.FillCircleApprox (g.X (61), g.Y (61), g.S (10), kKnob);
        p.FillCircleApprox (g.X (58), g.Y (58), g.S (4),  kHighlight);
        p.FillRect        (g.X (36), g.Y (11), g.S (16), g.S (16), kOrange);
        p.OutlineRect     (g.X (36), g.Y (11), g.S (16), g.S (16), g.S (1.5f), kOrangeEdge);
        p.FillRect        (g.X (11), g.Y (36), g.S (16), g.S (16), kOrange);
        p.OutlineRect     (g.X (11), g.Y (36), g.S (16), g.S (16), g.S (1.5f), kOrangeEdge);
        return;
    }

    // 3/4 perspective (joystick-icon-skeuo.svg).
    p.FillConvexQuad  (g.X (74), g.Y (58), g.X (86), g.Y (42), g.X (86), g.Y (74), g.X (74), g.Y (90), kSideFace);
    p.FillRect        (g.X (10), g.Y (58), g.S (64), g.S (32), kCase);
    p.OutlineRect     (g.X (10), g.Y (58), g.S (64), g.S (32), g.S (2), kCaseEdge);
    p.DrawLineApprox  (g.X (13), g.Y (76), g.X (71), g.Y (76), g.S (1.2f), kSeam);
    p.FillRect        (g.X (10), g.Y (77), g.S (64), g.S (13), 0x0F000000);
    p.FillConvexQuad  (g.X (10), g.Y (58), g.X (22), g.Y (42), g.X (86), g.Y (42), g.X (74), g.Y (58), kCaseLight);
    p.DrawLineApprox  (g.X (10), g.Y (58), g.X (22), g.Y (42), g.S (1.6f), kCaseEdge);
    p.DrawLineApprox  (g.X (22), g.Y (42), g.X (86), g.Y (42), g.S (1.6f), kCaseEdge);
    p.DrawLineApprox  (g.X (86), g.Y (42), g.X (74), g.Y (58), g.S (1.6f), kCaseEdge);
    p.FillConvexQuad  (g.X (28), g.Y (54), g.X (66), g.Y (54), g.X (74), g.Y (46), g.X (36), g.Y (46), kHole);
    p.FillConvexQuad  (g.X (36), g.Y (46), g.X (74), g.Y (46), g.X (61), g.Y (50), g.X (45), g.Y (50), 0xFF8D8877);
    p.FillRect        (g.X (49.4f), g.Y (42), g.S (3.2f), g.S (10), kShaft);
    p.FillConvexQuad  (g.X (47.5f), g.Y (44), g.X (54.5f), g.Y (44), g.X (57), g.Y (14), g.X (45), g.Y (14), kStick);
    p.FillCircleApprox (g.X (51), g.Y (14), g.S (6.5f), kStick);
    p.FillEllipseApprox (g.X (49), g.Y (12), g.S (2.2f), g.S (3), 0x38FFFFFF);
    p.FillRect        (g.X (17), g.Y (61.5f), g.S (13), g.S (12), kOrange);
    p.OutlineRect     (g.X (17), g.Y (61.5f), g.S (13), g.S (12), g.S (1.4f), kOrangeEdge);
    p.FillRect        (g.X (36), g.Y (61.5f), g.S (13), g.S (12), kOrange);
    p.OutlineRect     (g.X (36), g.Y (61.5f), g.S (13), g.S (12), g.S (1.4f), kOrangeEdge);
}


void InputDeviceSelector::PaintPaddleGlyph (IDxuiPainter & p, const RECT & box, bool skeuo)
{
    GlyphMap  g (box);

    if (!skeuo)
    {
        // Top-down (paddle-icon.svg).
        p.FillRect        (g.X (14), g.Y (30), g.S (12), g.S (16), kOrange);
        p.OutlineRect     (g.X (14), g.Y (30), g.S (12), g.S (16), g.S (1.5f), kOrangeEdge);
        p.FillConvexQuad  (g.X (34), g.Y (52), g.X (62), g.Y (52), g.X (58), g.Y (89), g.X (38), g.Y (89), kCase);
        for (int i = 0; i < 4; i++)
        {
            float  y = 60.0f + 6.0f * (float) i;
            float  inw = 1.0f + 0.8f * (float) i;
            p.DrawLineApprox (g.X (37.5f + inw), g.Y (y), g.X (58.5f - inw), g.Y (y), g.S (1.6f), kRib);
        }
        p.FillCircleApprox (g.X (48), g.Y (34), g.S (25), kCaseEdge);
        p.FillCircleApprox (g.X (48), g.Y (34), g.S (24), kCase);
        p.FillCircleApprox (g.X (48), g.Y (34), g.S (19.8f), kDialEdge);
        p.FillCircleApprox (g.X (48), g.Y (34), g.S (19), kDial);
        for (int i = 0; i < 12; i++)
        {
            float  a  = (float) i * 6.2831853f / 12.0f;
            float  ca = cosf (a), sa = sinf (a);
            p.DrawLineApprox (g.X (48 + ca * 18.5f), g.Y (34 + sa * 18.5f),
                              g.X (48 + ca * 13.5f), g.Y (34 + sa * 13.5f), g.S (1.6f), kTick);
        }
        p.FillCircleApprox (g.X (48), g.Y (34), g.S (10.8f), kDialEdge);
        p.FillCircleApprox (g.X (48), g.Y (34), g.S (10), kKnob);
        p.FillCircleApprox (g.X (45), g.Y (31), g.S (3.5f), 0x4DFFFFFF);
        return;
    }

    // 3/4 perspective (paddle-icon-skeuo.svg): ribbed handle to lower-left,
    // stacked knurled cylinders (dial + cap) at upper-right, orange button.
    p.FillConvexQuad  (g.X (52), g.Y (71), g.X (56.7f), g.Y (77.2f), g.X (26.4f), g.Y (93.9f), g.X (22), g.Y (88), 0xFFB3AD9C);
    p.FillConvexQuad  (g.X (44), g.Y (44), g.X (55), g.Y (65), g.X (26), g.Y (84), g.X (14), g.Y (70), kCase);
    p.DrawLineApprox  (g.X (44), g.Y (44), g.X (55), g.Y (65), g.S (1.6f), kCaseEdge);
    p.DrawLineApprox  (g.X (55), g.Y (65), g.X (26), g.Y (84), g.S (1.6f), kCaseEdge);
    p.DrawLineApprox  (g.X (14), g.Y (70), g.X (26), g.Y (84), g.S (1.6f), kCaseEdge);
    for (int i = 0; i < 4; i++)
    {
        float  ox = -6.0f * (float) i;
        p.DrawLineApprox (g.X (44 + ox), g.Y (62 - 3.0f * (float) i),
                          g.X (37 + ox), g.Y (76 - 3.0f * (float) i), g.S (1.7f), kRib);
    }
    p.FillRect        (g.X (80), g.Y (44), g.S (11), g.S (16), kOrange);
    p.OutlineRect     (g.X (80), g.Y (44), g.S (11), g.S (16), g.S (1.5f), kOrangeEdge);

    // Housing collar (cylinder: bottom ellipse + band + top ellipse).
    p.FillEllipseApprox (g.X (58), g.Y (54), g.S (25), g.S (12), 0xFFB5AF9E);
    p.FillRect          (g.X (33), g.Y (48), g.S (50), g.S (6), 0xFFB5AF9E);
    p.FillEllipseApprox (g.X (58), g.Y (48), g.S (25), g.S (12), kCase);

    // Knurled dial ring.
    p.FillEllipseApprox (g.X (58), g.Y (50), g.S (22), g.S (10.5f), kDialSide);
    p.FillRect          (g.X (36), g.Y (40), g.S (44), g.S (10), kDialSide);
    for (int i = 0; i < 7; i++)
    {
        float  x  = 40.0f + 6.0f * (float) i;
        float  dy = (i == 3) ? 10.0f : ((i == 2 || i == 4) ? 9.3f : ((i == 1 || i == 5) ? 7.5f : 4.5f));
        p.DrawLineApprox (g.X (x), g.Y (40 + dy), g.X (x), g.Y (50 + dy), g.S (1.4f), kTick);
    }
    p.FillEllipseApprox (g.X (58), g.Y (40), g.S (22), g.S (10.5f), kDialTop);

    // Cap cylinder + embossed oval.
    p.FillEllipseApprox (g.X (58), g.Y (36), g.S (12.5f), g.S (6), kDialSide);
    p.FillRect          (g.X (45.5f), g.Y (26), g.S (25), g.S (10), kDialSide);
    p.FillEllipseApprox (g.X (58), g.Y (26), g.S (12.5f), g.S (6), kDialTop);
    p.FillEllipseApprox (g.X (58), g.Y (26), g.S (5), g.S (2.3f), 0xFF948E7D);
    p.FillEllipseApprox (g.X (58), g.Y (26), g.S (3.4f), g.S (1.4f), kDialTop);
}


void InputDeviceSelector::PaintMouseGlyph (IDxuiPainter & p, const RECT & box, bool skeuo)
{
    GlyphMap  g (box);

    if (!skeuo)
    {
        // Top-down (mouse-icon.svg).
        p.FillRect     (g.X (20), g.Y (6),  g.S (56), g.S (84), kCase);
        p.OutlineRect  (g.X (20), g.Y (6),  g.S (56), g.S (84), g.S (2), kCaseEdge);
        p.FillRect     (g.X (27), g.Y (13), g.S (42), g.S (70), kCaseLight);
        p.OutlineRect  (g.X (27), g.Y (13), g.S (42), g.S (70), g.S (1), 0xE69B9686);
        p.FillRect     (g.X (30), g.Y (14), g.S (36), g.S (24), kMouseBtn);
        p.OutlineRect  (g.X (30), g.Y (14), g.S (36), g.S (24), g.S (1.8f), kMouseBtnEdge);
        p.DrawLineApprox (g.X (30), g.Y (42), g.X (66), g.Y (42), g.S (1.4f), 0xCC9B9686);
        return;
    }

    // 3/4 perspective (mouse-icon-skeuo.svg): shallow chamfered box, the
    // wide gray button recessed at the front-left of the top face.
    p.FillConvexQuad  (g.X (68), g.Y (58), g.X (86), g.Y (35), g.X (86), g.Y (62), g.X (68), g.Y (86), kSideFace);
    p.FillRect        (g.X (8),  g.Y (58), g.S (60), g.S (28), kCase);
    p.OutlineRect     (g.X (8),  g.Y (58), g.S (60), g.S (28), g.S (2), kCaseEdge);
    p.DrawLineApprox  (g.X (11), g.Y (79), g.X (65), g.Y (79), g.S (1.2f), kSeam);
    p.FillConvexQuad  (g.X (8),  g.Y (58), g.X (24), g.Y (36), g.X (86), g.Y (36), g.X (70), g.Y (58), kCaseLight);
    p.DrawLineApprox  (g.X (8),  g.Y (58), g.X (24), g.Y (36), g.S (1.6f), kCaseEdge);
    p.DrawLineApprox  (g.X (24), g.Y (36), g.X (86), g.Y (36), g.S (1.6f), kCaseEdge);
    p.DrawLineApprox  (g.X (86), g.Y (36), g.X (70), g.Y (58), g.S (1.6f), kCaseEdge);
    // chamfer frame hint
    p.DrawLineApprox  (g.X (15), g.Y (55), g.X (28), g.Y (39), g.S (1.2f), 0xE6B4AE9C);
    p.DrawLineApprox  (g.X (28), g.Y (39), g.X (79), g.Y (39), g.S (1.2f), 0xE6B4AE9C);
    p.DrawLineApprox  (g.X (79), g.Y (39), g.X (66), g.Y (55), g.S (1.2f), 0xE6B4AE9C);
    // recessed button (under-quad = recess edge, then the key face)
    p.FillConvexQuad  (g.X (15), g.Y (56.5f), g.X (24.5f), g.Y (39), g.X (49.5f), g.Y (39), g.X (40), g.Y (56.5f), kMouseBtnEdge);
    p.FillConvexQuad  (g.X (17), g.Y (55), g.X (25.5f), g.Y (40.5f), g.X (47.5f), g.Y (40.5f), g.X (39), g.Y (55), kMouseBtn);
}

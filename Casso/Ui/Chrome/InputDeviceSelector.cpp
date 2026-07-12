#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "InputDeviceSelector.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Palette — transcribed from the SVG masters (warm ABS beige family, the
//  fire-button orange, and the drive-bar LED blue for the state LEDs).
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

    // LED state colors — identical to the drive-bar / old toggle LED.
    constexpr uint32_t  kLedOnCore  = 0xFF3DA1FF;
    constexpr uint32_t  kLedOffCore = 0xFF06121A;

    constexpr uint32_t  kFocusRing  = 0xFF3DA1FF;
    constexpr uint32_t  kHoverBg    = 0x16FFFFFF;
    constexpr uint32_t  kDividerCol = 0x5A8F8A7A;

    constexpr const wchar_t * kFontFamily = DxuiTheme::kBodyFace;

    constexpr const wchar_t * kLabels[3] = { L"Joystick mode", L"Paddle mode", L"Mouse mode" };

    // Per-segment tooltips (independent, per user feedback).
    constexpr wchar_t  kTipJoystickSeg[] =
        L"Joystick mode — arrows steer the joystick, X and Z fire.\n"
        L"Click to toggle. Works alongside the pointer devices.";
    constexpr wchar_t  kTipPaddleSeg[] =
        L"Paddle mode — the captured mouse drives paddles 0 and 1;\n"
        L"left / right buttons fire. Esc releases the pointer.";
    constexpr wchar_t  kTipMouseSeg[] =
        L"Mouse mode — the host pointer drives the built-in mouse while\n"
        L"over the screen (non-capturing).";

    // State-summary fallbacks (paddle-capture notice + gap hover).
    constexpr wchar_t  kTipOffState[] =
        L"Input devices: joystick (arrow keys), paddle, or mouse.\n"
        L"Click a device to connect it to the game port.";
    constexpr wchar_t  kTipPaddleState[] =
        L"The mouse drives paddles 0 and 1; left / right buttons fire.\n"
        L"Press Esc to release the mouse and exit paddle mode.";
    constexpr wchar_t  kTipJoystickState[] =
        L"Arrows, Z, and X keys are mapped to the joystick and its buttons.";
    constexpr wchar_t  kTipMouseState[] =
        L"The host pointer drives the built-in mouse while over the screen\n"
        L"(non-capturing).";
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  boundsDip carries the anchor CENTER point. Each segment lays out as
//  [LED] [icon] [label]; label widths come from the text renderer with a
//  Segoe-advance fallback before the draw target exists (same contract as
//  the old toggle button).
//
////////////////////////////////////////////////////////////////////////////////

void InputDeviceSelector::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    UINT   dpi     = scaler.Dpi ();
    UINT   eDpi    = (dpi == 0) ? 96u : dpi;
    int    icon    = MulDiv (kIconDp,    (int) eDpi, 96);
    int    pad     = MulDiv (kPadDp,     (int) eDpi, 96);
    int    segPad  = MulDiv (kSegPadDp,  (int) eDpi, 96);
    int    ledGap  = MulDiv (kLedGapDp,  (int) eDpi, 96);
    int    ledR    = MulDiv (4,          (int) eDpi, 96);
    int    textGap = MulDiv (kTextGapDp, (int) eDpi, 96);
    int    segGap  = MulDiv (kSegGapDp,  (int) eDpi, 96);
    int    gGap    = MulDiv (kGroupGapDp,(int) eDpi, 96);
    float  fontPx  = kFontDip * (float) eDpi / 96.0f;
    int    n       = SegmentCount ();
    int    h       = icon + pad * 2;
    int    segW[3] = {};
    int    txtW[3] = {};


    m_dpi = eDpi;

    for (int i = 0; i < n; i++)
    {
        float  tw = 0.0f;
        float  th = 0.0f;

        if (m_textRenderer != nullptr)
        {
            HRESULT  hrM = m_textRenderer->MeasureString (SegmentLabel (i), fontPx, kFontFamily, tw, th);
            if (FAILED (hrM)) { tw = 0.0f; }
        }
        if (tw <= 0.0f)
        {
            tw = (float) wcslen (SegmentLabel (i)) * kFallbackCharPx * (float) eDpi / 96.0f;
        }

        txtW[i] = (int) (tw + 0.5f);
        segW[i] = segPad + ledR * 2 + ledGap + icon + textGap + txtW[i] + segPad;
    }

    int  total = segW[0] + gGap;
    for (int i = 1; i < n; i++)
    {
        total += segW[i] + ((i + 1 < n) ? segGap : 0);
    }

    int  x = boundsDip.left - total / 2;
    int  y = boundsDip.top  - h / 2;

    m_bounds = RECT { x, y, x + total, y + h };

    int  sx = x;
    for (int i = 0; i < n; i++)
    {
        m_segRects[i]   = RECT { sx, y, sx + segW[i], y + h };
        m_ledCenters[i] = POINT { sx + segPad + ledR, y + h / 2 };

        int  ix = sx + segPad + ledR * 2 + ledGap;
        m_iconRects[i]  = RECT { ix, y + pad, ix + icon, y + pad + icon };

        int  tx = ix + icon + textGap;
        m_textRects[i]  = RECT { tx, y, tx + txtW[i], y + h };

        sx += segW[i] + ((i == 0) ? gGap : segGap);
    }

    if (n < 3)
    {
        m_segRects[2] = RECT {};
        m_iconRects[2] = RECT {};
        m_textRects[2] = RECT {};
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


const wchar_t * InputDeviceSelector::SegmentLabel (int index) const
{
    return kLabels[(index >= 0 && index < 3) ? index : 0];
}




////////////////////////////////////////////////////////////////////////////////
//
//  Tooltips
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * InputDeviceSelector::TooltipText () const
{
    if (m_pointer == InputMappingMode::Mouse)  return kTipMouseState;
    if (m_pointer == InputMappingMode::Paddle) return kTipPaddleState;
    if (m_arrowsJoystick)                      return kTipJoystickState;
    return kTipOffState;
}


const wchar_t * InputDeviceSelector::TooltipTextAt (int x, int y) const
{
    switch (SegmentAt (x, y))
    {
        case Segment::Joystick: return kTipJoystickSeg;
        case Segment::Paddle:   return kTipPaddleSeg;
        case Segment::Mouse:    return kTipMouseSeg;
        default:                return TooltipText ();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Per segment: hover wash, state LED (lit drive-bar blue / dark off core),
//  peripheral glyph, text label. State is the LED — no selection outline
//  (an outline reads as focus); keyboard focus keeps the thin accent ring
//  around the whole control.
//
////////////////////////////////////////////////////////////////////////////////

void InputDeviceSelector::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    static constexpr Segment  kOrder[3] = { Segment::Joystick, Segment::Paddle, Segment::Mouse };

    if (m_bounds.right <= m_bounds.left)
    {
        return;                       // hidden
    }

    float  fontPx = kFontDip * (float) m_dpi / 96.0f;
    float  ledR   = 4.0f * (float) m_dpi / 96.0f;

    for (int i = 0; i < SegmentCount (); i++)
    {
        const RECT & r   = m_segRects[i];
        bool         sel = SegmentSelected (i);

        if (m_hovered && m_hoverSegment == kOrder[i])
        {
            painter.FillRect ((float) r.left, (float) r.top,
                              (float) (r.right - r.left), (float) (r.bottom - r.top), kHoverBg);
        }

        // State LED.
        if (sel)
        {
            // Soft glow: concentric rings on a quadratic falloff. A single
            // translucent disc reads as a hard-edged puck, not a light.
            constexpr int    kGlowRings  = 10;
            constexpr float  kGlowSpread = 3.4f;    // glow rim, in LED radii
            float  cum = 0.0f;                      // opacity composited so far
            for (int ring = kGlowRings; ring >= 1; ring--)
            {
                float  t      = (float) ring / (float) (kGlowRings + 1);
                float  target = 0.63f * (1.0f - t) * (1.0f - t);
                float  add    = (target - cum) / (1.0f - cum);
                cum = target;
                uint32_t  a = (uint32_t) (add * 255.0f + 0.5f);
                painter.FillCircleApprox ((float) m_ledCenters[i].x, (float) m_ledCenters[i].y,
                                          ledR * (1.0f + (kGlowSpread - 1.0f) * t),
                                          (a << 24) | (kLedOnCore & 0x00FFFFFF));
            }
            painter.FillCircleApprox ((float) m_ledCenters[i].x, (float) m_ledCenters[i].y, ledR, kLedOnCore);
        }
        else
        {
            painter.FillCircleApprox ((float) m_ledCenters[i].x, (float) m_ledCenters[i].y, ledR, kLedOffCore);
        }

        switch (i)
        {
            case 0: PaintJoystickGlyph (painter, m_iconRects[i], m_skeuoStyle); break;
            case 1: PaintPaddleGlyph   (painter, m_iconRects[i], m_skeuoStyle); break;
            case 2: PaintMouseGlyph    (painter, m_iconRects[i], m_skeuoStyle); break;
        }

        HRESULT  hrT = text.DrawString (SegmentLabel (i),
                                        (float) m_textRects[i].left,
                                        (float) m_textRects[i].top,
                                        (float) (m_textRects[i].right - m_textRects[i].left) + 4.0f,
                                        (float) (m_textRects[i].bottom - m_textRects[i].top),
                                        theme.ButtonText (),
                                        fontPx,
                                        kFontFamily,
                                        DxuiTextRenderer::HAlign::Left,
                                        DxuiTextRenderer::VAlign::CenterOnCapHeight);
        IGNORE_RETURN_VALUE (hrT, S_OK);
    }

    // Divider between the Keys and Pointer groups.
    {
        float  dx = (float) (m_segRects[0].right + m_segRects[1].left) * 0.5f;
        painter.FillRect (dx, (float) m_bounds.top + 4.0f, 1.0f,
                          (float) (m_bounds.bottom - m_bounds.top) - 8.0f, kDividerCol);
    }

    if (m_focused)
    {
        painter.OutlineRect ((float) m_bounds.left - 1, (float) m_bounds.top - 1,
                             (float) (m_bounds.right - m_bounds.left) + 2,
                             (float) (m_bounds.bottom - m_bounds.top) + 2,
                             1.0f, kFocusRing);
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

    // 3/4 perspective (joystick-icon-skeuo.svg rev 3): shared lower-right
    // camera; the fire buttons are raised cubes ON THE TOP FACE at the
    // top-down master's NW positions (one north of the well, one west).
    p.FillConvexQuad  (g.X (74), g.Y (58), g.X (86), g.Y (42), g.X (86), g.Y (74), g.X (74), g.Y (90), kSideFace);
    p.FillRect        (g.X (10), g.Y (58), g.S (64), g.S (32), kCase);
    p.OutlineRect     (g.X (10), g.Y (58), g.S (64), g.S (32), g.S (2), kCaseEdge);
    p.DrawLineApprox  (g.X (13), g.Y (76), g.X (71), g.Y (76), g.S (1.2f), kSeam);
    p.FillRect        (g.X (10), g.Y (77), g.S (64), g.S (13), 0x0F000000);
    p.FillConvexQuad  (g.X (10), g.Y (58), g.X (22), g.Y (42), g.X (86), g.Y (42), g.X (74), g.Y (58), kCaseLight);
    p.DrawLineApprox  (g.X (10), g.Y (58), g.X (22), g.Y (42), g.S (1.6f), kCaseEdge);
    p.DrawLineApprox  (g.X (22), g.Y (42), g.X (86), g.Y (42), g.S (1.6f), kCaseEdge);
    p.DrawLineApprox  (g.X (86), g.Y (42), g.X (74), g.Y (58), g.S (1.6f), kCaseEdge);
    // well (top-down (36,36)-(86,86) mapped) + inner facet
    p.FillConvexQuad  (g.X (33.4f), g.Y (57.2f), g.X (71.7f), g.Y (57.2f), g.X (78.8f), g.Y (47.7f), g.X (40.6f), g.Y (47.7f), kHole);
    p.FillConvexQuad  (g.X (40.6f), g.Y (47.7f), g.X (78.8f), g.Y (47.7f), g.X (64), g.Y (51), g.X (49), g.Y (51), 0xFF8D8877);
    // button A (north of the well): raised cube -- front, right side, top
    p.FillConvexQuad  (g.X (42.9f), g.Y (40), g.X (54.1f), g.Y (40), g.X (54.1f), g.Y (46), g.X (42.9f), g.Y (46), kOrange);
    p.FillConvexQuad  (g.X (54.1f), g.Y (40), g.X (56.3f), g.Y (37), g.X (56.3f), g.Y (43), g.X (54.1f), g.Y (46), 0xFFC24418);
    p.FillConvexQuad  (g.X (44.1f), g.Y (37), g.X (56.3f), g.Y (37), g.X (54.1f), g.Y (40), g.X (42.9f), g.Y (40), 0xFFFF7038);
    // button B (west of the well): raised cube
    p.FillConvexQuad  (g.X (19.2f), g.Y (44.8f), g.X (31.4f), g.Y (44.8f), g.X (31.4f), g.Y (50.8f), g.X (19.2f), g.Y (50.8f), kOrange);
    p.FillConvexQuad  (g.X (31.4f), g.Y (44.8f), g.X (33.7f), g.Y (41.7f), g.X (33.7f), g.Y (47.7f), g.X (31.4f), g.Y (50.8f), 0xFFC24418);
    p.FillConvexQuad  (g.X (21.5f), g.Y (41.7f), g.X (33.7f), g.Y (41.7f), g.X (31.4f), g.Y (44.8f), g.X (19.2f), g.Y (44.8f), 0xFFFF7038);
    // stick over the well center
    p.FillRect        (g.X (54.4f), g.Y (42.5f), g.S (3.2f), g.S (10), kShaft);
    p.FillConvexQuad  (g.X (52.5f), g.Y (44.5f), g.X (59.5f), g.Y (44.5f), g.X (62), g.Y (14), g.X (50), g.Y (14), kStick);
    p.FillCircleApprox (g.X (56), g.Y (14), g.S (6.5f), kStick);
    p.FillEllipseApprox (g.X (54), g.Y (12), g.S (2.2f), g.S (3), 0x38FFFFFF);
}


void InputDeviceSelector::PaintPaddleGlyph (IDxuiPainter & p, const RECT & box, bool skeuo)
{
    GlyphMap  g (box);

    if (!skeuo)
    {
        // Top-down (paddle-icon.svg).
        // fire button: a curved chunk of the head's own circle protruding
        // at 2 o'clock (two quads along the arc; the housing circle painted
        // after covers the inner edge, leaving the curved rim)
        p.FillConvexQuad  (g.X (66.3f), g.Y (12.2f), g.X (72.7f), g.Y (19.7f), g.X (64.5f), g.Y (24.5f), g.X (60.2f), g.Y (19.4f), kOrange);
        p.FillConvexQuad  (g.X (72.7f), g.Y (19.7f), g.X (76.1f), g.Y (29.1f), g.X (66.7f), g.Y (30.7f), g.X (64.5f), g.Y (24.5f), kOrange);
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

    // 3/4 perspective (paddle-icon-skeuo.svg rev 14): the handle is one
    // CONSISTENT projection with the dial (plan axis 30 deg west of
    // south under the shared 0.35 camera): every transverse edge and
    // grip line projects along (0.866,0.175) = slope +0.20, every axial
    // line along (-0.5,0.303) = slope -0.61, so slope_t * slope_a =
    // -0.35^2 as the dial's ellipses require. Handle and disc (rx 21)
    // are one cream body, top-surface edges TANGENT to the disc ellipse
    // at (38.4,35.4) / (74.3,42.6). DIAL: two flat knurled CYLINDERS
    // (rx 17.5 / 7.5) joined by a small-radius FILLET skirt. BUTTON:
    // rim-concentric arc segment (rx 24.5 vs 21) at ~3:00-4:30.
    // UNION of slab + disc cylinder. Shell thickness = 10 = the disc's
    // own wall height (top ellipse cy38 -> base cy48). The SE side wall
    // runs to the disc's SE tangent generator at x=74.3 (y 42.6 rim ->
    // 52.6 base), coinciding with the disc wall there so the two merge
    // with no gap; the bottom outline (top outline + (0,10)) lands on
    // the disc base ellipse. Draw order: SE side face, bottom edge, tip
    // face, disc wall (covers the junction), disc top, handle top.
    p.FillConvexQuad  (g.X (26.8f), g.Y (66.9f), g.X (74.3f), g.Y (42.6f), g.X (74.3f), g.Y (52.6f), g.X (26.8f), g.Y (76.9f), 0xFFB3AD9C);
    p.DrawLineApprox  (g.X (26.8f), g.Y (76.9f), g.X (73), g.Y (53.2f), g.S (1.4f), 0xFF8F8A7A);
    p.FillConvexQuad  (g.X (11.2f), g.Y (63.8f), g.X (26.8f), g.Y (66.9f), g.X (26.8f), g.Y (76.9f), g.X (11.2f), g.Y (73.8f), kSideFace);
    p.FillEllipseApprox (g.X (58), g.Y (48), g.S (21), g.S (7.3f), 0xFFBFB9A7);
    p.FillRect          (g.X (37), g.Y (38), g.S (42), g.S (10), 0xFFBFB9A7);
    // unified top surface: disc top + handle top, one cream body
    p.FillEllipseApprox (g.X (58), g.Y (38), g.S (21), g.S (7.3f), kCase);
    p.FillConvexQuad  (g.X (11.2f), g.Y (63.8f), g.X (38.4f), g.Y (35.4f), g.X (74.3f), g.Y (42.6f), g.X (26.8f), g.Y (66.9f), kCase);
    // grip lines: full width NW edge -> SE shoulder; equal edge stations
    // pair up because both edges span tip -> tangency together, and each
    // line projects along the shared transverse direction (slope +0.20)
    for (int i = 0; i < 8; i++)
    {
        float  t  = 0.20f + 0.08f * (float) i;
        float  lx = 11.2f + 27.2f * t, ly = 63.8f - 28.4f * t;   // on the NW edge
        float  sx = 26.8f + 47.5f * t, sy = 66.9f - 24.3f * t;   // on the SE shoulder
        p.DrawLineApprox (g.X (lx), g.Y (ly), g.X (sx), g.Y (sy), g.S (1.3f), kRib);
        // grip lines wrap over the SE shoulder onto the side face
        p.DrawLineApprox (g.X (sx), g.Y (sy), g.X (sx), g.Y (sy + 2.2f), g.S (1.1f), 0xFF98927F);
    }
    // Apple badge chip in the smooth patch near the tip, parting seam
    p.FillConvexQuad  (g.X (20.2f), g.Y (63.5f), g.X (23.1f), g.Y (64.1f), g.X (25.2f), g.Y (61.9f), g.X (22.3f), g.Y (61.3f), 0x99A9A392);
    // parting seam on the SE side wall; stops before the disc so it does
    // not float onto the top surface at the handle/disc union
    p.DrawLineApprox  (g.X (27), g.Y (71.9f), g.X (59), g.Y (55.4f), g.S (1.0f), 0xB38F8A7A);
    // dark opening the dial sits in (dial concentric with the disc)
    p.FillEllipseApprox (g.X (58), g.Y (38), g.S (19.5f), g.S (6.8f), kHole);
    // dial lower cylinder: flat top, 6-unit knurl band on its wall
    p.FillEllipseApprox (g.X (58), g.Y (38), g.S (17.5f), g.S (6.1f), kDialSide);
    p.FillRect          (g.X (40.5f), g.Y (32), g.S (35), g.S (6), kDialSide);
    p.FillEllipseApprox (g.X (58), g.Y (32), g.S (17.5f), g.S (6.1f), kDial);
    for (int i = -6; i <= 6; i++)
    {
        float  dx = 2.6f * (float) i;
        float  s  = sqrtf (1.0f - (dx / 17.5f) * (dx / 17.5f));
        p.DrawLineApprox (g.X (58 + dx), g.Y (32 + 6.1f * s), g.X (58 + dx), g.Y (38 + 6.1f * s), g.S (1.1f), kTick);
    }
    // small-radius fillet where the cap cylinder meets the flat top
    p.FillEllipseApprox (g.X (58), g.Y (31.3f), g.S (10), g.S (3.5f), 0xFFA6A08E);
    // dial upper cylinder (knurled cap), then the groove-and-"0" top
    p.FillEllipseApprox (g.X (58), g.Y (32), g.S (7.5f), g.S (2.6f), kDialSide);
    p.FillRect          (g.X (50.5f), g.Y (23), g.S (15), g.S (9), kDialSide);
    for (int i = -3; i <= 3; i++)
    {
        float  dx = 2.4f * (float) i;
        float  s  = sqrtf (1.0f - (dx / 7.5f) * (dx / 7.5f));
        p.DrawLineApprox (g.X (58 + dx), g.Y (23 + 2.6f * s), g.X (58 + dx), g.Y (32 + 2.6f * s), g.S (1.1f), kTick);
    }
    p.FillEllipseApprox (g.X (58), g.Y (23), g.S (7.5f), g.S (2.6f), kKnob);
    p.FillEllipseApprox (g.X (58), g.Y (23), g.S (4.4f), g.S (1.5f), kDialSide);
    p.FillEllipseApprox (g.X (58), g.Y (23), g.S (3.3f), g.S (1.1f), kKnob);
    p.FillEllipseApprox (g.X (58), g.Y (23), g.S (1.5f), g.S (0.55f), kDialSide);
    // fire button: rim-concentric arc segment (quads sample the arcs at
    // 3:00 / 3:45 / 4:30) — lit top band, orange outer wall, dark S cut
    p.FillConvexQuad  (g.X (82.4f), g.Y (37.8f), g.X (82.4f), g.Y (44.3f), g.X (81), g.Y (47.9f), g.X (81), g.Y (41.4f), kOrange);
    p.FillConvexQuad  (g.X (81), g.Y (41.4f), g.X (81), g.Y (47.9f), g.X (75.3f), g.Y (51), g.X (75.3f), g.Y (44.5f), kOrange);
    p.FillConvexQuad  (g.X (78.9f), g.Y (37.4f), g.X (82.4f), g.Y (37.8f), g.X (81), g.Y (41.4f), g.X (77.7f), g.Y (40.5f), 0xFFFF7D46);
    p.FillConvexQuad  (g.X (77.7f), g.Y (40.5f), g.X (81), g.Y (41.4f), g.X (75.3f), g.Y (44.5f), g.X (72.8f), g.Y (43.2f), 0xFFFF7D46);
    p.FillConvexQuad  (g.X (72.8f), g.Y (43.2f), g.X (75.3f), g.Y (44.5f), g.X (75.3f), g.Y (51), g.X (72.8f), g.Y (49.7f), 0xFFC24418);
}


void InputDeviceSelector::PaintMouseGlyph (IDxuiPainter & p, const RECT & box, bool skeuo)
{
    GlyphMap  g (box);

    if (!skeuo)
    {
        // Top-down (mouse-icon.svg): button on the NORTH side.
        p.FillRect     (g.X (20), g.Y (6),  g.S (56), g.S (84), kCase);
        p.OutlineRect  (g.X (20), g.Y (6),  g.S (56), g.S (84), g.S (2), kCaseEdge);
        p.FillRect     (g.X (27), g.Y (13), g.S (42), g.S (70), kCaseLight);
        p.OutlineRect  (g.X (27), g.Y (13), g.S (42), g.S (70), g.S (1), 0xE69B9686);
        p.FillRect     (g.X (30), g.Y (14), g.S (36), g.S (24), kMouseBtn);
        p.OutlineRect  (g.X (30), g.Y (14), g.S (36), g.S (24), g.S (1.8f), kMouseBtnEdge);
        p.DrawLineApprox (g.X (30), g.Y (42), g.X (66), g.Y (42), g.S (1.4f), 0xCC9B9686);
        return;
    }

    // 3/4 perspective (mouse-icon-skeuo.svg rev 6): shared lower-right
    // camera; proportions follow the top-down master (footprint 56x84,
    // button 64% width at the NORTH end, depth slightly exaggerated).
    // Art sits 16 grid units above the master's desk plane and 4.5 right
    // (silhouette spans 55 units) so the glyph is centered both ways.
    p.FillConvexQuad  (g.X (63.5f), g.Y (48), g.X (75.5f), g.Y (32), g.X (75.5f), g.Y (50), g.X (63.5f), g.Y (68), kSideFace);
    p.FillRect        (g.X (20.5f), g.Y (48), g.S (43), g.S (20), kCase);
    p.OutlineRect     (g.X (20.5f), g.Y (48), g.S (43), g.S (20), g.S (2), kCaseEdge);
    p.DrawLineApprox  (g.X (23), g.Y (62), g.X (61), g.Y (62), g.S (1.1f), kSeam);
    p.FillConvexQuad  (g.X (20.5f), g.Y (48), g.X (32.5f), g.Y (32), g.X (75.5f), g.Y (32), g.X (63.5f), g.Y (48), kCaseLight);
    p.DrawLineApprox  (g.X (20.5f), g.Y (48), g.X (32.5f), g.Y (32), g.S (1.6f), kCaseEdge);
    p.DrawLineApprox  (g.X (32.5f), g.Y (32), g.X (75.5f), g.Y (32), g.S (1.6f), kCaseEdge);
    p.DrawLineApprox  (g.X (75.5f), g.Y (32), g.X (63.5f), g.Y (48), g.S (1.6f), kCaseEdge);
    // STRONG peripheral bevel: raised inner plateau above the bevel band,
    // inset u 4 / y 2 all around in the parallelogram frame (symmetric)
    p.FillConvexQuad  (g.X (26), g.Y (46), g.X (35), g.Y (34), g.X (70), g.Y (34), g.X (61), g.Y (46), 0xFFEFEADB);
    p.DrawLineApprox  (g.X (26), g.Y (46), g.X (35), g.Y (34), g.S (1.5f), 0xFFB4AE9C);
    p.DrawLineApprox  (g.X (35), g.Y (34), g.X (70), g.Y (34), g.S (1.5f), 0xFFB4AE9C);
    p.DrawLineApprox  (g.X (70), g.Y (34), g.X (61), g.Y (46), g.S (1.5f), 0xFFB4AE9C);
    p.DrawLineApprox  (g.X (26), g.Y (46), g.X (61), g.Y (46), g.S (1.5f), 0xFFB4AE9C);
    // the single wide button at the NORTH (back), centered in the plateau
    p.FillConvexQuad  (g.X (38.3f), g.Y (34.3f), g.X (66.3f), g.Y (34.3f), g.X (62.4f), g.Y (39.4f), g.X (34.4f), g.Y (39.4f), kMouseBtnEdge);
    p.FillConvexQuad  (g.X (38.7f), g.Y (35.3f), g.X (64.3f), g.Y (35.3f), g.X (62), g.Y (38.4f), g.X (36.4f), g.Y (38.4f), kMouseBtn);
}

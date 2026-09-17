#include "Pch.h"

#include "InputMonoGlyphs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InputMonoGlyphs::Paint
//
////////////////////////////////////////////////////////////////////////////////

void InputMonoGlyphs::Paint (IDxuiPainter & painter, InputMonoGlyphKind kind,
                             const RECT & box, uint32_t ink)
{
    switch (kind)
    {
        case InputMonoGlyphKind::Gamepad:   PaintGamepad  (painter, box, ink); break;
        case InputMonoGlyphKind::Paddle:    PaintPaddle   (painter, box, ink); break;
        case InputMonoGlyphKind::Keys:      PaintKeys     (painter, box, ink); break;

        case InputMonoGlyphKind::Joystick:
        default:                            PaintJoystick (painter, box, ink); break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputMonoGlyphs::GetStroke
//
////////////////////////////////////////////////////////////////////////////////

float InputMonoGlyphs::GetStroke (float widthPx)
{
    return (std::max) (1.15f, widthPx / 15.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputMonoGlyphs::StrokeCircle
//
////////////////////////////////////////////////////////////////////////////////

void InputMonoGlyphs::StrokeCircle (IDxuiPainter & painter, float cx, float cy,
                                    float r, float stroke, uint32_t ink)
{
    constexpr int  s_kSegments = 20;



    for (int i = 0; i < s_kSegments; i++)
    {
        float  a0 = 6.2831853f * (float) i       / (float) s_kSegments;
        float  a1 = 6.2831853f * (float) (i + 1) / (float) s_kSegments;

        painter.DrawLineApprox (cx + r * std::cos (a0), cy + r * std::sin (a0),
                                cx + r * std::cos (a1), cy + r * std::sin (a1),
                                stroke, ink);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputMonoGlyphs::PaintJoystick
//
//  Ball, stick, slab, all in outline.
//
////////////////////////////////////////////////////////////////////////////////

void InputMonoGlyphs::PaintJoystick (IDxuiPainter & painter, const RECT & box, uint32_t ink)
{
    float  w        = (float) (box.right  - box.left);
    float  h        = (float) (box.bottom - box.top);
    float  stroke   = GetStroke (w);
    float  cx       = (float) box.left + w * 0.5f;
    float  knobR    = w * 0.16f;
    float  knobY    = (float) box.top + h * 0.21f;
    float  baseT    = (float) box.top + h * 0.66f;
    float  baseH    = h * 0.20f;
    float  baseHalf = w * 0.34f;



    StrokeCircle           (painter, cx, knobY, knobR, stroke, ink);
    painter.DrawLineApprox (cx, knobY + knobR, cx, baseT, stroke, ink);
    painter.OutlineRect    (cx - baseHalf, baseT, baseHalf * 2.0f, baseH, stroke, ink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputMonoGlyphs::PaintPaddle
//
//  The keyhole read: one outer contour -- the knob arc wrapping the top, from
//  the left shoulder angle around to the right, handing off to the tapering
//  sides and a flat bottom -- with the knob itself as an inner ring.
//
////////////////////////////////////////////////////////////////////////////////

void InputMonoGlyphs::PaintPaddle (IDxuiPainter & painter, const RECT & box, uint32_t ink)
{
    constexpr int    s_kArcSegments = 16;
    constexpr float  s_kPi          = 3.1415926f;



    float  w       = (float) (box.right  - box.left);
    float  h       = (float) (box.bottom - box.top);
    float  stroke  = GetStroke (w);
    float  cx      = (float) box.left + w * 0.5f;
    float  cy      = (float) box.top + h * 0.34f;
    float  outerR  = w * 0.24f;
    float  botHalf = w * 0.13f;
    float  botY    = (float) box.top + h * 0.88f;
    float  aL      = s_kPi * 160.0f / 180.0f;
    float  aR      = s_kPi *  20.0f / 180.0f;



    for (int i = 0; i < s_kArcSegments; i++)
    {
        float  t0 = aL + (aR + 2.0f * s_kPi - aL) * (float) i       / (float) s_kArcSegments;
        float  t1 = aL + (aR + 2.0f * s_kPi - aL) * (float) (i + 1) / (float) s_kArcSegments;

        painter.DrawLineApprox (cx + outerR * std::cos (t0), cy + outerR * std::sin (t0),
                                cx + outerR * std::cos (t1), cy + outerR * std::sin (t1),
                                stroke, ink);
    }

    painter.DrawLineApprox (cx + outerR * std::cos (aL), cy + outerR * std::sin (aL),
                            cx - botHalf, botY, stroke, ink);
    painter.DrawLineApprox (cx + outerR * std::cos (aR), cy + outerR * std::sin (aR),
                            cx + botHalf, botY, stroke, ink);
    painter.DrawLineApprox (cx - botHalf, botY, cx + botHalf, botY, stroke, ink);

    StrokeCircle (painter, cx, cy, outerR * 0.42f, stroke, ink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputMonoGlyphs::PaintGamepad
//
//  A rounded body with two grips and two thumbsticks. The grips are what
//  separate it from the paddle at a glance: both are rounded outlines, and
//  without the shoulders dropping away at the bottom corners the two read
//  alike at 19 dp.
//
////////////////////////////////////////////////////////////////////////////////

void InputMonoGlyphs::PaintGamepad (IDxuiPainter & painter, const RECT & box, uint32_t ink)
{
    float  w       = (float) (box.right  - box.left);
    float  h       = (float) (box.bottom - box.top);
    float  stroke  = GetStroke (w);
    float  cx      = (float) box.left + w * 0.5f;
    float  bodyT   = (float) box.top + h * 0.30f;
    float  bodyB   = (float) box.top + h * 0.62f;
    float  bodyL   = (float) box.left + w * 0.16f;
    float  bodyR   = (float) box.left + w * 0.84f;
    float  gripB   = (float) box.top + h * 0.80f;
    float  stickR  = w * 0.085f;
    float  stickY  = (float) box.top + h * 0.46f;



    // The body: a flat top between the shoulders, straight sides, and a
    // bottom edge that the two grips hang from.
    painter.DrawLineApprox (bodyL, bodyT, bodyR, bodyT, stroke, ink);
    painter.DrawLineApprox (bodyL, bodyT, bodyL, bodyB, stroke, ink);
    painter.DrawLineApprox (bodyR, bodyT, bodyR, bodyB, stroke, ink);

    // Grips: the outer edge falls away from each bottom corner and returns to
    // the middle of the underside, leaving the notch between them.
    painter.DrawLineApprox (bodyL, bodyB, bodyL + w * 0.06f, gripB, stroke, ink);
    painter.DrawLineApprox (bodyL + w * 0.06f, gripB, cx - w * 0.06f, bodyB, stroke, ink);
    painter.DrawLineApprox (bodyR, bodyB, bodyR - w * 0.06f, gripB, stroke, ink);
    painter.DrawLineApprox (bodyR - w * 0.06f, gripB, cx + w * 0.06f, bodyB, stroke, ink);
    painter.DrawLineApprox (cx - w * 0.06f, bodyB, cx + w * 0.06f, bodyB, stroke, ink);

    // Two thumbsticks, which is the other half of what says gamepad.
    StrokeCircle (painter, cx - w * 0.20f, stickY, stickR, stroke, ink);
    StrokeCircle (painter, cx + w * 0.20f, stickY, stickR, stroke, ink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputMonoGlyphs::PaintKeys
//
//  Four arrow keys in the inverted-T the Apple II's own cursor keys sit in,
//  because what this entry means is the ARROWS driving the stick, not the
//  keyboard as a whole. A full keyboard outline would say "type here".
//
////////////////////////////////////////////////////////////////////////////////

void InputMonoGlyphs::PaintKeys (IDxuiPainter & painter, const RECT & box, uint32_t ink)
{
    float  w      = (float) (box.right  - box.left);
    float  h      = (float) (box.bottom - box.top);
    float  stroke = GetStroke (w);
    float  key    = w * 0.26f;
    float  gap    = w * 0.04f;
    float  cx     = (float) box.left + w * 0.5f;
    float  rowT   = (float) box.top + h * 0.52f;
    float  upT    = rowT - key - gap;



    painter.OutlineRect (cx - key * 0.5f,           upT,  key, key, stroke, ink);
    painter.OutlineRect (cx - key * 0.5f,           rowT, key, key, stroke, ink);
    painter.OutlineRect (cx - key * 1.5f - gap,     rowT, key, key, stroke, ink);
    painter.OutlineRect (cx + key * 0.5f + gap,     rowT, key, key, stroke, ink);
}

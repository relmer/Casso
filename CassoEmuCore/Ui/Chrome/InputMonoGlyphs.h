#pragma once

#include "Pch.h"

#include "Render/IDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InputMonoGlyphKind
//
//  Which drawing answers "what is driving the paddle axes".
//
////////////////////////////////////////////////////////////////////////////////

enum class InputMonoGlyphKind
{
    Gamepad,
    Joystick,
    Paddle,
    Keys
};





////////////////////////////////////////////////////////////////////////////////
//
//  InputMonoGlyphs
//
//  The monoline input icons, in outline at one stroke weight, for the chrome
//  that reads as one icon set: the command bar's paddle-source picker and the
//  input cluster's segments. The 3/4 illustrations of the real Apple
//  peripherals are a separate family in `InputDeviceGlyphs`, used on the
//  skeuomorphic themes.
//
//  DRAWN, NOT SET IN A FONT. MDL2 has no Apple paddle, and mixing a font
//  glyph with drawn ones puts two icon languages on one strip -- which is
//  what the command bar looked like when the picker briefly used MDL2's
//  gamepad beside these.
//
//  DRAWN FOR THE SIZE THEY APPEAR AT, not shrunk to it. The turned paddle
//  profile that preceded the current one -- cap, shoulder, body, waist -- was
//  a blob by the time its box reached 19 dp.
//
////////////////////////////////////////////////////////////////////////////////

class InputMonoGlyphs
{
public:

    static void  Paint         (IDxuiPainter & painter, InputMonoGlyphKind kind,
                                const RECT & box, uint32_t ink);

    static void  PaintGamepad  (IDxuiPainter & painter, const RECT & box, uint32_t ink);
    static void  PaintJoystick (IDxuiPainter & painter, const RECT & box, uint32_t ink);
    static void  PaintPaddle   (IDxuiPainter & painter, const RECT & box, uint32_t ink);
    static void  PaintKeys     (IDxuiPainter & painter, const RECT & box, uint32_t ink);

    // MDL2 draws roughly a fifteenth of its em as stroke; the floor keeps the
    // pen visible once the box is small enough for that ratio to fall under a
    // pixel.
    static float  GetStroke    (float widthPx);
    static void   StrokeCircle (IDxuiPainter & painter, float cx, float cy,
                                float r, float stroke, uint32_t ink);
};

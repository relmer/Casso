#pragma once

#include "Pch.h"

#include "Render/IDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InputDeviceGlyphs
//
//  The three drawn Apple peripheral icons the command toolbar's input
//  segments paint: the Apple Joystick A2M2002, Hand Controller A2M2001 and
//  Apple Mouse M0100, transcribed from the SVG masters in
//  Assets/DesignSources/InputIcons. Drawn rather than taken from a font
//  because MDL2 has no paddle.
//
//  Each painter renders in one of two styles, chosen by the theme: a 3/4
//  perspective illustration on skeuomorphic themes, top-down on
//  DarkModern / retro. `box` is the square pixel rect the 96x96 master grid
//  maps onto.
//
////////////////////////////////////////////////////////////////////////////////

class InputDeviceGlyphs
{
public:
    static void PaintJoystickGlyph (IDxuiPainter & p, const RECT & box, bool skeuo);
    static void PaintPaddleGlyph   (IDxuiPainter & p, const RECT & box, bool skeuo);
    static void PaintMouseGlyph    (IDxuiPainter & p, const RECT & box, bool skeuo);

private:
    struct GlyphMap;   // master-grid -> box coordinate mapper (defined in the .cpp)

    // Palette -- transcribed from the SVG masters: the warm ABS beige family
    // and the fire-button orange.
    static constexpr uint32_t  kCase         = 0xFFD8D2C1;   // body plastic
    static constexpr uint32_t  kCaseLight    = 0xFFE2DDCD;   // top faces / plateau
    static constexpr uint32_t  kCaseEdge     = 0xFF8F8A7A;   // molded edge stroke
    static constexpr uint32_t  kSideFace     = 0xFFA9A392;   // oblique right-side faces
    static constexpr uint32_t  kFacetTop     = 0xFFC7C1B1;   // funnel facets, light..dark
    static constexpr uint32_t  kFacetLeft    = 0xFFB5AF9E;
    static constexpr uint32_t  kFacetRight   = 0xFFA39D8C;
    static constexpr uint32_t  kFacetBot     = 0xFF948E7D;
    static constexpr uint32_t  kHole         = 0xFF6B6759;   // pivot hole / well opening
    static constexpr uint32_t  kKnob         = 0xFFB9B4A6;   // stick knob / dial caps
    static constexpr uint32_t  kKnobEdge     = 0xFF6E6A5C;
    static constexpr uint32_t  kDial         = 0xFFABA592;   // paddle dial body
    static constexpr uint32_t  kDialSide     = 0xFFA79F8D;   // cylinder side bands
    static constexpr uint32_t  kDialEdge     = 0xFF827D6C;
    static constexpr uint32_t  kTick         = 0xFF7E7967;   // knurl ticks
    static constexpr uint32_t  kRib          = 0xFFA9A392;   // grip ribs
    static constexpr uint32_t  kOrange       = 0xFFF0602B;   // fire buttons
    static constexpr uint32_t  kOrangeEdge   = 0xFFA63C14;
    static constexpr uint32_t  kMouseBtn     = 0xFFB0ADA4;   // mouse button gray
    static constexpr uint32_t  kMouseBtnEdge = 0xFF6E6B62;
    static constexpr uint32_t  kStick        = 0xFF3A3733;   // joystick grip
    static constexpr uint32_t  kShaft        = 0xFF7A6A4E;   // brass shaft
    static constexpr uint32_t  kHighlight    = 0x59FFFFFF;   // specular highlights
    static constexpr uint32_t  kSeam         = 0xB88F8A7A;   // case seam lines
};

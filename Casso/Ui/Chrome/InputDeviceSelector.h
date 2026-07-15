#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/DxuiTheme.h"
#include "UiCommandTypes.h"

class IDxuiTheme;




////////////////////////////////////////////////////////////////////////////////
//
//  InputDeviceSelector
//
//  Segmented two-group device selector replacing the JoystickToggleButton
//  cycle-toggle in the drive bar. Reflects the split input model:
//
//      [LED icon Joystick mode]  |  [LED icon Paddle mode] [LED icon Mouse mode]
//        Keys group                  Pointer group (radio; both may be off)
//
//  Each segment shows an LED (lit drive-bar blue when the mapping is on),
//  a 44-dp skeuomorphic glyph of the real Apple peripheral (Apple Joystick
//  A2M2002, Hand Controller A2M2001, Apple Mouse M0100 — transcribed from
//  the SVG masters in Assets/DesignSources/InputIcons), and a text label.
//  Glyph style follows the theme: 3/4 perspective on skeuomorphic themes,
//  top-down on DarkModern / retro (SetSkeuoStyle). The mouse segment
//  appears only on mouse-capable machines with the mouse connected.
//
//  On/off state is the LED (per user feedback, an outline ring reads as
//  focus, not state); keyboard focus keeps the thin accent ring on the
//  whole control. Each segment carries its own tooltip (TooltipTextAt).
//  Clicking a segment toggles its axis via the shell's
//  ToggleInputMappingMode; keyboard activation still cycles presets.
//
////////////////////////////////////////////////////////////////////////////////

class InputDeviceSelector : public IDxuiControl
{
public:
    enum class Segment
    {
        None,
        Joystick,
        Paddle,
        Mouse,
    };

    void     SetTextRenderer (IDxuiTextRenderer * pText) { m_textRenderer = pText; }

    void     Hide            ()      { m_bounds = {}; for (RECT & r : m_segRects) { r = {}; } }

    // Split-model state: which segments light up + whether Mouse exists.
    void     SetState        (bool arrowsJoystick, InputMappingMode pointer, bool mouseAvailable)
    {
        m_arrowsJoystick = arrowsJoystick;
        m_pointer        = pointer;
        m_mouseAvailable = mouseAvailable;
    }

    // Theme style: 3/4 perspective glyphs (skeuo) vs top-down (dark/retro).
    void     SetSkeuoStyle   (bool skeuo)   { m_skeuoStyle = skeuo; }

    void     SetHovered      (bool hovered) { m_hovered = hovered; if (!hovered) { m_hoverSegment = Segment::None; } }
    void     SetHoverPoint   (int x, int y) { m_hoverSegment = SegmentAt (x, y); }
    void     SetFocused      (bool focused) { m_focused = focused; }
    void     SetPressed      (bool pressed) { m_pressed = pressed; }

    bool     HitTest         (int x, int y) const;
    Segment  SegmentAt       (int x, int y) const;
    RECT     Bounds          () const       { return m_bounds; }

    // Per-segment tooltip for the cursor position; falls back to the
    // state summary between segments. TooltipText() keeps the state-based
    // text for non-positional callers (the paddle-capture notice).
    const wchar_t * TooltipText   () const;
    const wchar_t * TooltipTextAt (int x, int y) const;

    // IDxuiControl. boundsDip carries the anchor CENTER point (left/top ==
    // right/bottom), matching the old button's layout contract.
    void     Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void     Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    // Pure glyph painters, exposed for reuse/tests. `box` is the square
    // pixel rect the 96x96 master grid maps onto.
    static void PaintJoystickGlyph (IDxuiPainter & p, const RECT & box, bool skeuo);
    static void PaintPaddleGlyph   (IDxuiPainter & p, const RECT & box, bool skeuo);
    static void PaintMouseGlyph    (IDxuiPainter & p, const RECT & box, bool skeuo);

private:
    struct GlyphMap;   // master-grid -> box coordinate mapper (defined in the .cpp)

    static constexpr int    kIconDp     = 35;    // glyph box; the row height
                                                 // (icon + 2*pad = 37) fits the
                                                 // s_kJoystickButtonBandDp (43 dp)
                                                 // band above the drive widget with
                                                 // ~3 dp margin above and below.
    static constexpr int    kPadDp      = 1;     // vertical padding
    static constexpr int    kSegPadDp   = 6;     // segment leading/trailing pad
    static constexpr int    kLedGapDp   = 5;     // LED -> icon gap
    static constexpr int    kTextGapDp  = 6;     // icon -> label gap
    static constexpr int    kSegGapDp   = 6;     // gap within a group
    static constexpr int    kGroupGapDp = 14;    // gap between Keys | Pointer groups
    static constexpr float  kFontDip    = 13.0f;
    static constexpr float  kFallbackCharPx = 7.5f;

    // Palette — transcribed from the SVG masters (warm ABS beige family, the
    // fire-button orange, and the drive-bar LED blue for the state LEDs).
    static constexpr uint32_t  kCase       = 0xFFD8D2C1;   // body plastic
    static constexpr uint32_t  kCaseLight  = 0xFFE2DDCD;   // top faces / plateau
    static constexpr uint32_t  kCaseEdge   = 0xFF8F8A7A;   // molded edge stroke
    static constexpr uint32_t  kSideFace   = 0xFFA9A392;   // oblique right-side faces
    static constexpr uint32_t  kFacetTop   = 0xFFC7C1B1;   // funnel facets, light..dark
    static constexpr uint32_t  kFacetLeft  = 0xFFB5AF9E;
    static constexpr uint32_t  kFacetRight = 0xFFA39D8C;
    static constexpr uint32_t  kFacetBot   = 0xFF948E7D;
    static constexpr uint32_t  kHole       = 0xFF6B6759;   // pivot hole / well opening
    static constexpr uint32_t  kKnob       = 0xFFB9B4A6;   // stick knob / dial caps
    static constexpr uint32_t  kKnobEdge   = 0xFF6E6A5C;
    static constexpr uint32_t  kDial       = 0xFFABA592;   // paddle dial body
    static constexpr uint32_t  kDialSide   = 0xFFA79F8D;   // cylinder side bands
    static constexpr uint32_t  kDialTop    = 0xFFC1BBAA;
    static constexpr uint32_t  kDialEdge   = 0xFF827D6C;
    static constexpr uint32_t  kTick       = 0xFF7E7967;   // knurl ticks
    static constexpr uint32_t  kRib        = 0xFFA9A392;   // grip ribs
    static constexpr uint32_t  kOrange     = 0xFFF0602B;   // fire buttons
    static constexpr uint32_t  kOrangeEdge = 0xFFA63C14;
    static constexpr uint32_t  kMouseBtn   = 0xFFB0ADA4;   // mouse button gray
    static constexpr uint32_t  kMouseBtnEdge = 0xFF6E6B62;
    static constexpr uint32_t  kStick      = 0xFF3A3733;   // joystick grip
    static constexpr uint32_t  kShaft      = 0xFF7A6A4E;   // brass shaft
    static constexpr uint32_t  kHighlight  = 0x59FFFFFF;   // specular highlights
    static constexpr uint32_t  kSeam       = 0xB88F8A7A;   // case seam lines

    // LED state colors — identical to the drive-bar / old toggle LED.
    static constexpr uint32_t  kLedOnCore  = 0xFF3DA1FF;
    static constexpr uint32_t  kLedOffCore = 0xFF06121A;

    static constexpr uint32_t  kFocusRing  = 0xFF3DA1FF;
    static constexpr uint32_t  kHoverBg    = 0x16FFFFFF;
    static constexpr uint32_t  kDividerCol = 0x5A8F8A7A;

    static constexpr const wchar_t * kFontFamily = DxuiTheme::kBodyFace;
    static constexpr const wchar_t * kLabels[3]  = { L"Joystick mode", L"Paddle mode", L"Mouse mode" };

    // Second label line shown under "Paddle mode" while paddle is active: the
    // captured mouse hides the cursor, so the exit key must be visible on the
    // control itself. Smaller + muted so it reads as a hint, not a label.
    static constexpr const wchar_t * kPaddleEscHint  = L"ESC to exit";
    static constexpr float           kSubLabelScale  = 0.8f;   // hint font vs label font
    static constexpr int             kPaddleSegIndex = 1;      // order: Joystick, Paddle, Mouse

    // Per-segment tooltips (independent, per user feedback).
    static constexpr wchar_t  kTipJoystickSeg[] =
        L"Map the arrow keys and X/Z to the joystick and buttons 0/1.\n"
        L"Click to toggle. Works alongside the pointer devices.";
    static constexpr wchar_t  kTipPaddleSeg[] =
        L"Captures the mouse and maps it to paddles 0/1 and buttons 0/1\n"
        L"Press ESC to exit this mode.";
    static constexpr wchar_t  kTipMouseSeg[] =
        L"Send host mouse inputs to the machine";

    // State-summary fallbacks (paddle-capture notice + gap hover).
    static constexpr wchar_t  kTipOffState[] =
        L"Input devices: joystick (arrow keys), paddle, or mouse.\n"
        L"Click a device to connect it to the game port.";
    static constexpr wchar_t  kTipPaddleState[] =
        L"The mouse drives paddles 0 and 1; left / right click = buttons 0 and 1.\n"
        L"Press Esc to release the mouse and exit paddle mode.";
    static constexpr wchar_t  kTipJoystickState[] =
        L"Arrows, Z, and X keys are mapped to the joystick and its buttons.";
    static constexpr wchar_t  kTipMouseState[] =
        L"The host pointer drives the built-in mouse while over the screen\n"
        L"(non-capturing).";

    int      SegmentCount() const { return m_mouseAvailable ? 3 : 2; }
    bool     SegmentSelected (int index) const;
    const wchar_t * SegmentLabel (int index) const;

    RECT                 m_bounds        = {};
    RECT                 m_segRects[3]   = {};   // full segment (hit) rects
    RECT                 m_iconRects[3]  = {};
    RECT                 m_textRects[3]  = {};
    POINT                m_ledCenters[3] = {};
    UINT                 m_dpi           = 96;
    IDxuiTextRenderer *  m_textRenderer  = nullptr;
    bool                 m_arrowsJoystick = false;
    InputMappingMode     m_pointer        = InputMappingMode::Off;
    bool                 m_mouseAvailable = false;
    bool                 m_skeuoStyle     = false;
    bool                 m_hovered        = false;
    Segment              m_hoverSegment   = Segment::None;
    bool                 m_focused        = false;
    bool                 m_pressed        = false;
};

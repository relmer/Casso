#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
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

#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"
#include "Render/IDxuiPainter.h"
#include "UiCommandTypes.h"

class IDxuiTextRenderer;
class IDxuiTheme;




////////////////////////////////////////////////////////////////////////////////
//
//  InputDeviceSelector (T030d / FR-013a)
//
//  Segmented two-group device selector replacing the JoystickToggleButton
//  cycle-toggle in the drive bar. Reflects the split input model:
//
//      [ joystick ]  |  [ paddle ] [ mouse ]
//       Keys group        Pointer group (radio; both may be off)
//
//  Each segment paints a skeuomorphic glyph of the real Apple peripheral
//  (Apple Joystick A2M2002, Hand Controller A2M2001, Apple Mouse M0100),
//  transcribed from the SVG masters in Assets/DesignSources/InputIcons —
//  the 96x96 grids there ARE this widget's paint coordinates. Two glyph
//  styles: 3/4 perspective for the skeuomorphic theme, top-down for
//  DarkModern / retro (SetSkeuoStyle, driven from the active theme).
//  The mouse segment appears only on mouse-capable machines with the
//  mouse connected. Selection is shown with the LED-blue accent ring
//  (#3DA1FF) used by the drive-bar LEDs.
//
//  Clicking a segment toggles its axis (the shell routes SegmentAt() to
//  ToggleInputMappingMode); keyboard chrome-focus activation still cycles.
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

    // Compat no-op (the old button measured its text label; glyphs don't).
    void     SetTextRenderer (IDxuiTextRenderer * pText) { UNREFERENCED_PARAMETER (pText); }

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

    void     SetHovered      (bool hovered) { m_hovered = hovered; }
    void     SetFocused      (bool focused) { m_focused = focused; }
    void     SetPressed      (bool pressed) { m_pressed = pressed; }

    bool     HitTest         (int x, int y) const;
    Segment  SegmentAt       (int x, int y) const;
    RECT     Bounds          () const       { return m_bounds; }

    const wchar_t * TooltipText () const;

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
    static constexpr int  kChipDp     = 26;   // segment square
    static constexpr int  kChipGapDp  = 3;    // gap within a group
    static constexpr int  kGroupGapDp = 9;    // gap between Keys | Pointer groups
    static constexpr int  kPadDp      = 3;    // outer padding

    int      SegmentCount () const { return m_mouseAvailable ? 3 : 2; }
    bool     SegmentSelected (int index) const;

    RECT                 m_bounds        = {};
    RECT                 m_segRects[3]   = {};
    UINT                 m_dpi           = 96;
    bool                 m_arrowsJoystick = false;
    InputMappingMode     m_pointer        = InputMappingMode::Off;
    bool                 m_mouseAvailable = false;
    bool                 m_skeuoStyle     = false;
    bool                 m_hovered        = false;
    bool                 m_focused        = false;
    bool                 m_pressed        = false;
};

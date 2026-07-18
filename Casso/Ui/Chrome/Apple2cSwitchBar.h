#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/DxuiTheme.h"

class IDxuiTheme;




////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cSwitchBar
//
//  The Apple //c case-top control strip, reproduced as a chrome band that sits
//  between the emulator viewport and the joystick/paddle/mouse drive bar. It
//  models the four things silk-screened across the top-left / top-right of a
//  real //c case:
//
//      [ reset ]   | 80/40   | keyboard          disk use | power |
//        button      switch    switch               LED      LED
//
//  The bar body is painted in the //c's platinum case colour. The
//  reset button is momentary and only "live" while Ctrl is held (hardware
//  Control-Reset); the 80/40 and keyboard buttons are LATCHING — each stays
//  pressed in until clicked again, drawn sunk into the case (a shadowed, lowered
//  key) when in and proud (a raised, highlit key) when out. The disk-use LED
//  lights on drive activity, the power LED is lit whenever the machine is on.
//
//  Like the other Casso chrome (DriveWidget, InputDeviceSelector), this control
//  is manually hit-tested and actioned by EmulatorShell rather than through the
//  Dxui auto-input path; it exposes PartAt/HitTest for that routing. It reads
//  CassoTheme only for text colour fallbacks; the case palette is fixed so the
//  strip always reads as the //c case regardless of the active UI theme.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2cSwitchBar : public IDxuiControl
{
public:
    enum class Part
    {
        None,
        Reset,
        EightyForty,
        Keyboard,
    };

    void  SetTextRenderer (IDxuiTextRenderer * pText) { m_textRenderer = pText; }

    void  Hide            ()
    {
        m_bounds     = {};
        m_resetRect  = {};
        m_eightyRect = {};
        m_kbdRect    = {};
    }

    // Latching-switch + indicator state, fed by the shell. "In" == pressed
    // in (active): 80/40 in selects 80-column startup ($C060), keyboard in
    // selects the Dvorak layout.
    void  SetEightyFortyIn (bool in) { m_eightyFortyIn = in; }
    void  SetKeyboardIn    (bool in) { m_keyboardIn    = in; }
    void  SetDiskActive    (bool on) { m_diskActive    = on; }
    void  SetPowerOn       (bool on) { m_powerOn       = on; }

    bool  IsEightyFortyIn  () const { return m_eightyFortyIn; }
    bool  IsKeyboardIn     () const { return m_keyboardIn; }

    // Interaction state (mirrors InputDeviceSelector's owner-driven model).
    void  SetHovered    (bool hovered)   { m_hovered = hovered; if (!hovered) { m_hoverPart = Part::None; } }
    void  SetHoverPoint (int x, int y)   { m_hoverPart = PartAt (x, y); }
    void  SetPressedPart (Part part)     { m_pressedPart = part; }

    Part  PartAt   (int x, int y) const;
    bool  HitTest  (int x, int y) const  { return PartAt (x, y) != Part::None; }
    RECT  Bounds   () const              { return m_bounds; }

    const wchar_t * TooltipTextAt (int x, int y) const;

    // IDxuiControl. boundsDip is the FULL band rect (left..right x top..bottom),
    // unlike InputDeviceSelector's centre-anchor contract — the strip fills the
    // whole band and anchors its two groups to the left and right edges.
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    // Metrics (dp at 96 DPI; scaled per-DPI in Layout).
    static constexpr int  kEdgePadDp   = 16;   // left / right band inset
    static constexpr int  kResetWDp    = 52;   // reset button
    static constexpr int  kResetHDp    = 24;
    static constexpr int  kGroupGapDp  = 18;   // reset -> first switch
    static constexpr int  kKeyWDp      = 8;    // latching-switch key (thin, near the LED width)
    static constexpr int  kKeyHDp      = 25;
    static constexpr int  kLabelGapDp  = 7;    // key/LED -> its label
    static constexpr int  kSwitchGapDp = 20;   // switch cluster -> switch cluster
    static constexpr int  kLedWDp      = 7;    // indicator LED
    static constexpr int  kLedHDp      = 19;
    static constexpr int  kIndGapDp    = 18;   // disk-use cluster -> power cluster
    static constexpr float kFontDip    = 12.5f;
    static constexpr float kFallbackCharPx = 6.6f;

    // The real //c case switches are slanted parallelograms (top edge kicked
    // right, matching the italic silk-screen labels). kSlantTan is tan of that
    // lean; the raised/sunk click cue is carried by directional shading, not a
    // positional gap (see PaintSlantCap).
    static constexpr float kSlantTan   = 0.176f;   // ~10 degrees

    // Case palette — fixed (theme-independent) so the strip always reads as the
    // //c case. kCase is the //c's platinum case body colour.
    static constexpr uint32_t  kCase      = 0xFFD8D3C6;   // //c platinum case body
    static constexpr uint32_t  kCaseHi    = 0xFFE7E2D5;   // top bevel highlight
    static constexpr uint32_t  kCaseLo    = 0xFFBAB5A6;   // bottom bevel shadow
    static constexpr uint32_t  kCaseEdge  = 0xFF8F8A7A;   // molded edge stroke
    static constexpr uint32_t  kLabel     = 0xFF6E6A5C;   // silk-screen gray-brown

    static constexpr uint32_t  kCap       = 0xFFE8E3D6;   // reset cap face (flat cream)
    static constexpr uint32_t  kCapHi     = 0xFFF4F0E5;   // faint top sheen on the cap
    static constexpr uint32_t  kCapEdge   = 0xFF98917F;
    static constexpr uint32_t  kCapText   = 0xFF5A5647;   // reset silk-screen ink (dark)

    // The cap fills its slot; a thin rim just seats it in the case. Depth is
    // carried by directional shading in PaintSlantCap, not a dark recess fill.
    static constexpr uint32_t  kSocketRim = 0xFF9A9484;   // thin rim around the cap
    // Raised caps (reset + OUT switches) share the cream kCap/kCapHi face; a
    // latched-in switch keeps that cream but a step darker, reading as recessed.
    static constexpr uint32_t  kKeyFaceIn = 0xFFD4D1C7;   // cap face top, in (soft recess)
    static constexpr uint32_t  kKeyLoIn   = 0xFFC9C5BA;   // cap face bottom, in
    static constexpr uint32_t  kShadeProud  = 0x4E000000; // bottom/right bevel on a raised cap
    static constexpr uint32_t  kShadePushed = 0x84000000; // deeper top/left bevel on a sunk switch
    static constexpr uint32_t  kShadowNil   = 0x00000000; // transparent far edge

    static constexpr uint32_t  kLedOff    = 0xFF25281F;   // dark LED (idle)
    static constexpr uint32_t  kLedGreen  = 0xFF3CE070;   // lit green (power / disk use)
    static constexpr uint32_t  kLedGreenGlow = 0xFF2FBF5F;
    static constexpr uint32_t  kLedRim    = 0xFF15140F;

    static constexpr uint32_t  kHoverWash = 0x18FFFFFF;

    static constexpr const wchar_t * kFontFamily = DxuiTheme::kBodyFace;
    static constexpr const wchar_t * kLabelEighty   = L"80/40";
    static constexpr const wchar_t * kLabelKeyboard = L"keyboard";
    static constexpr const wchar_t * kLabelDiskUse  = L"disk use";
    static constexpr const wchar_t * kLabelPower    = L"power";
    static constexpr const wchar_t * kLabelReset    = L"reset";

    static constexpr wchar_t  kTipReset[] =
        L"Reset. Inert on its own, like the real //c key.\n"
        L"Hold Ctrl and click to reset; add Open-Apple (left Alt) to cold-boot.";
    static constexpr wchar_t  kTipEighty[] =
        L"80/40 column switch. Pressed in selects 80-column startup;\n"
        L"software reads it at $C060. Takes effect when a disk boots.";
    static constexpr wchar_t  kTipKeyboard[] =
        L"Keyboard layout switch. Pressed in selects the Dvorak layout;\n"
        L"out is the standard QWERTY layout.";

    float  MeasureLabel (const wchar_t * text, float fontPx) const;

    // Painters for the composable pieces.
    void  PaintResetButton (IDxuiPainter & p, IDxuiTextRenderer & text, const RECT & r);
    void  PaintKey         (IDxuiPainter & p, const RECT & keyRect, bool pressedIn, bool hovered);
    void  PaintLed         (IDxuiPainter & p, const RECT & r, bool lit);
    void  PaintLabel       (IDxuiTextRenderer & text, const RECT & r, const wchar_t * s, float fontPx);

    // Paints a slanted (parallelogram) cap lit from the top-left. Raised, it is
    // highlit top-left / shadowed bottom-right. Pressed with deepPress (the
    // latching switches) it darkens under a dominant top-left shadow; pressed
    // without it (the momentary reset) the lighting simply flips to shadow
    // top-left / highlight bottom-right, keeping the face light.
    void  PaintSlantCap    (IDxuiPainter & p, const RECT & r, bool pressedIn,
                            bool deepPress, bool hovered, uint32_t faceHi, uint32_t faceLo);

    // Low-level slanted-fill primitives shared by the cap/LED painters. Every
    // piece shares one shear field: a point at height y shifts right by
    // (refBottom - y) * tan. ShearFill draws one parallelogram strip; ShearGrad
    // / ShearGradH stack strips with an interpolated colour for a gradient.
    static uint32_t  LerpArgb   (uint32_t a, uint32_t b, float t);
    static void      ShearFill  (IDxuiPainter & p, float xL, float yTop, float w, float h,
                                 float tan, float refBottom, uint32_t argb);
    static void      ShearGrad  (IDxuiPainter & p, float xL, float yTop, float w, float h,
                                 float tan, float refBottom, uint32_t top, uint32_t bot, int strips);
    static void      ShearGradH (IDxuiPainter & p, float xL, float yTop, float w, float h,
                                 float tan, float refBottom, uint32_t left, uint32_t right, int cols);

    RECT                 m_bounds       = {};
    RECT                 m_resetRect    = {};
    RECT                 m_eightyRect   = {};   // clickable: key + label
    RECT                 m_kbdRect      = {};
    RECT                 m_eightyKey    = {};   // just the key glyph
    RECT                 m_kbdKey       = {};
    RECT                 m_eightyLabel  = {};
    RECT                 m_kbdLabel     = {};
    RECT                 m_diskLed      = {};
    RECT                 m_diskLabel    = {};
    RECT                 m_powerLed     = {};
    RECT                 m_powerLabel   = {};
    UINT                 m_dpi          = 96;
    IDxuiTextRenderer *  m_textRenderer = nullptr;

    bool   m_eightyFortyIn = false;
    bool   m_keyboardIn    = false;
    bool   m_diskActive    = false;
    bool   m_powerOn       = true;
    bool   m_hovered       = false;
    Part   m_hoverPart     = Part::None;
    Part   m_pressedPart   = Part::None;
};

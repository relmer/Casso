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
//  The bar body is painted in the ImageWriter II platinum case colour. The
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

    // The reset button is inert unless Ctrl is held (real Control-Reset). The
    // shell sets this from the live modifier state so the button paints "armed"
    // (able to fire) vs dormant.
    void  SetResetArmed    (bool armed) { m_resetArmed = armed; }

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
    static constexpr int  kKeyWDp      = 11;   // latching-switch key
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
    // lean; kCapTravel is how far a cap rides proud of / sunk below the case
    // surface, as a fraction of the switch height -- the visible click cue.
    static constexpr float kSlantTan   = 0.176f;   // ~10 degrees
    static constexpr float kCapTravel  = 0.22f;

    // Case palette — fixed (theme-independent) so the strip always reads as the
    // //c case. kCase is the ImageWriter II platinum body colour, matching the
    // printer chrome so the two peripherals share a family.
    static constexpr uint32_t  kCase      = 0xFFD8D3C6;   // ImageWriter platinum body
    static constexpr uint32_t  kCaseHi    = 0xFFE7E2D5;   // top bevel highlight
    static constexpr uint32_t  kCaseLo    = 0xFFBAB5A6;   // bottom bevel shadow
    static constexpr uint32_t  kCaseEdge  = 0xFF8F8A7A;   // molded edge stroke
    static constexpr uint32_t  kLabel     = 0xFF6E6A5C;   // silk-screen gray-brown

    static constexpr uint32_t  kCap       = 0xFFE8E3D6;   // reset cap (lighter cream)
    static constexpr uint32_t  kCapHi     = 0xFFF4F0E5;
    static constexpr uint32_t  kCapLo     = 0xFFCBC5B6;
    static constexpr uint32_t  kCapEdge   = 0xFF98917F;
    static constexpr uint32_t  kCapText   = 0xFF5A5647;
    static constexpr uint32_t  kCapTextOff = 0xFF9A9484; // dormant (no Ctrl) reset label

    // The recess is a shallow molded depression, only a touch darker than the
    // case -- depth comes from soft gradient shadows, not a dark fill (a
    // near-black slot read as an unexplained block behind a proud cap).
    static constexpr uint32_t  kSocket    = 0xFFC9C4B5;   // molded recess floor (subtle)
    static constexpr uint32_t  kSocketRim = 0xFF9A9484;   // thin rim around the recess
    static constexpr uint32_t  kKeyFace   = 0xFFDED9CC;   // key top, out
    static constexpr uint32_t  kKeyHi     = 0xFFF0ECE0;   // cap face top, out (proud)
    static constexpr uint32_t  kKeyLo     = 0xFFBDB8AA;   // cap face bottom, out
    static constexpr uint32_t  kKeyFaceIn = 0xFFB2AD9F;   // cap face top, in (darkened)
    static constexpr uint32_t  kKeyLoIn   = 0xFF938E80;   // cap face bottom, in (darker still)
    static constexpr uint32_t  kShadowDk  = 0x70000000;   // near edge of a cast shadow gradient
    static constexpr uint32_t  kShadowLt  = 0x55000000;   // proud cap's lighter drop shadow
    static constexpr uint32_t  kShadowNil = 0x00000000;   // far edge (fully transparent)

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

    // Paints a slanted (parallelogram) cap seated in its case recess, riding
    // proud of the surface when out and depressed below it when in -- the
    // shared look for the reset button and both latching switches.
    void  PaintSlantCap    (IDxuiPainter & p, const RECT & r, bool pressedIn,
                            bool hovered, uint32_t faceHi, uint32_t faceLo);

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
    bool   m_resetArmed    = false;
    bool   m_hovered       = false;
    Part   m_hoverPart     = Part::None;
    Part   m_pressedPart   = Part::None;
};

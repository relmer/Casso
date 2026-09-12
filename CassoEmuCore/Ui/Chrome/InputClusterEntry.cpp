#include "Pch.h"
#include "Theme/DxuiColor.h"
#include "Theme/DxuiTheme.h"

#include "CassoTheme.h"
#include "InputClusterEntry.h"
#include "InputDeviceGlyphs.h"
#include "PrinterStatusLed.h"




static constexpr int      s_kBaseDpi        = 96;
static constexpr int      s_kBtnPadXDp      = 10;   // the collapsed form is a standard button
static constexpr float    s_kIconDip        = 15.0f;
static constexpr float    s_kFontDip        = 13.0f;
static constexpr float    s_kFallbackCharPx = 7.5f;

static constexpr const wchar_t * s_kFontFamily = DxuiTheme::kBodyFace;
static constexpr const wchar_t * s_kIconFamily = L"Segoe MDL2 Assets";
static constexpr const wchar_t * s_kGlyphMouse = L"\uE962";   // the cluster's one font glyph

// LED + glyph segments under one shared label. The glyph box is sized so
// its INK matches the MDL2 icons' (their 15 dip em draws about 15 dp of
// ink); a drawn glyph filling its box needs the smaller number.
static constexpr int      s_kSegIconDp       = 19;
static constexpr int      s_kSegPadXDp       = 5;
static constexpr int      s_kSegLedDp        = 7;    // LED diameter
static constexpr int      s_kSegLedGapDp     = 4;
static constexpr int      s_kSegGapDp        = 2;
static constexpr int      s_kLabelGapDp      = 8;    // label -> first segment

static constexpr const wchar_t * s_kLabel = L"Input";

// Contrast an unlit segment LED keeps against the strip -- the ratio
// DxuiTreeView tints a locked checkbox's fill with, so the two read alike.
static constexpr float    s_kOffLedContrast  = 1.6f;

// The rows, worded as the segment tooltips name the modes.
static constexpr const wchar_t * s_kRows[3] =
{
    L"Joystick (arrow keys)",
    L"Paddles (mouse)",
    L"Mouse",
};

static constexpr InputMappingMode s_kModes[3] =
{
    InputMappingMode::Joystick,
    InputMappingMode::Paddle,
    InputMappingMode::Mouse,
};

// Per-segment tooltips. The segments carry no labels of their own, so the
// tips lead with the mode name the old selector showed as text.
static constexpr const wchar_t * s_kTips[3] =
{
    L"Joystick mode: map the arrow keys and X/Z to the joystick and\n"
    L"buttons 0/1. Click to toggle; works alongside the pointer devices.",
    L"Paddle mode: captures the mouse and maps it to paddles 0/1 and\n"
    L"buttons 0/1. Press ESC to exit this mode.",
    L"Mouse mode: send host mouse inputs to the machine.",
};





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::InputClusterEntry
//
//  The three mode commands: each dispatches its mode through the sink and
//  reads its check from the same state the segments light from.
//
////////////////////////////////////////////////////////////////////////////////

InputClusterEntry::InputClusterEntry()
{
    for (int i = 0; i < 3; i++)
    {
        m_modes[i].id        = i;
        m_modes[i].label     = s_kRows[i];
        m_modes[i].isChecked = [this, i] () { return IsSegmentOn (i); };
        m_modes[i].dispatch  = [this, i] () { if (m_sink) { m_sink (s_kModes[i]); } };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::SetInputState
//
//  The mouse segment exists only when the machine has a mouse. Whether it
//  does decides HOW MANY segments there are, and the segment rects belong to
//  Layout, so the caller re-lays the entry when this reports a change.
//
////////////////////////////////////////////////////////////////////////////////

bool InputClusterEntry::SetInputState (bool arrowsJoystick, InputMappingMode pointer, bool mouseAvailable)
{
    bool  countChanged = (mouseAvailable != m_mouseAvailable);



    m_arrowsJoystick = arrowsJoystick;
    m_pointerMode    = pointer;
    m_mouseAvailable = mouseAvailable;

    return countChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::GetSlotAt
//
//  Display position to mode slot. Only the mouse is drawn here now, so every
//  position is the mouse; the slots keep their old numbering because the
//  modes, their labels, their tips and IsSegmentOn are all indexed by them.
//
////////////////////////////////////////////////////////////////////////////////

int InputClusterEntry::GetSlotAt (int index) const
{
    constexpr int  kSlotMouse = 2;



    UNREFERENCED_PARAMETER (index);

    return kSlotMouse;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::IsSegmentOn
//
//  Takes a SLOT, not a display position. The joystick slot lights from the
//  arrows mapping, paddle and mouse from the pointer mode.
//
////////////////////////////////////////////////////////////////////////////////

bool InputClusterEntry::IsSegmentOn (int index) const
{
    switch (index)
    {
        case 0:  return m_arrowsJoystick;
        case 1:  return m_pointerMode == InputMappingMode::Paddle;
        case 2:  return m_pointerMode == InputMappingMode::Mouse;
        default: return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::GetPickerItems
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> InputClusterEntry::GetPickerItems() const
{
    std::vector<DxuiPopupMenuItem>  items;



    for (int i = 0; i < GetSegmentCount(); i++)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (&m_modes[GetSlotAt (i)]));
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::IsPointInRect / MeasureLabelPx
//
////////////////////////////////////////////////////////////////////////////////

bool InputClusterEntry::IsPointInRect (const RECT & rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}


int InputClusterEntry::MeasureLabelPx (IDxuiTextRenderer * text, const wchar_t * label, float fontPx)
{
    float    w         = 0.0f;
    float    h         = 0.0f;
    HRESULT  hrMeasure = E_FAIL;



    if (text != nullptr)
    {
        hrMeasure = text->MeasureString (label, fontPx, s_kFontFamily, w, h);
    }

    if (SUCCEEDED (hrMeasure) && w > 0.0f)
    {
        return (int) (w + 0.5f);
    }

    return (int) ((float) wcslen (label) * s_kFallbackCharPx * fontPx / s_kFontDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::GetWidthPx
//
//  The full form is a shared label over a row of LED segments; the collapsed
//  form is a single icon like everything else on the strip.
//
////////////////////////////////////////////////////////////////////////////////

int InputClusterEntry::GetWidthPx (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const
{
    UINT   dpi    = scaler.GetDpi();
    int    padX   = MulDiv (s_kBtnPadXDp, (int) dpi, s_kBaseDpi);
    float  fontPx = s_kFontDip * (float) dpi / (float) s_kBaseDpi;
    int    iconW  = (int) (s_kIconDip * (float) dpi / (float) s_kBaseDpi + 0.5f);



    if (labeled)
    {
        int  segW     = MulDiv (s_kSegPadXDp * 2 + s_kSegLedDp + s_kSegLedGapDp + s_kSegIconDp,
                                (int) dpi, s_kBaseDpi);
        int  segGap   = MulDiv (s_kSegGapDp,   (int) dpi, s_kBaseDpi);
        int  labelGap = MulDiv (s_kLabelGapDp, (int) dpi, s_kBaseDpi);

        // +3px slack over the measured width: DrawString wraps on a rect even
        // fractionally narrower than the layout width it measured.
        return MeasureLabelPx (text != nullptr ? text : m_textRenderer, s_kLabel, fontPx) + 3 + labelGap +
               GetSegmentCount() * segW + (GetSegmentCount() - 1) * segGap;
    }

    return padX * 2 + iconW;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::Layout
//
//  The segments live inside the entry's own rect, so the cluster travels
//  with it. A collapsed entry has no segments, and their empty rects are
//  what keeps their tips and clicks silent.
//
////////////////////////////////////////////////////////////////////////////////

void InputClusterEntry::Layout (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler)
{
    UINT   dpi      = scaler.GetDpi();
    int    segW     = MulDiv (s_kSegPadXDp * 2 + s_kSegLedDp + s_kSegLedGapDp + s_kSegIconDp,
                              (int) dpi, s_kBaseDpi);
    int    segGap   = MulDiv (s_kSegGapDp,   (int) dpi, s_kBaseDpi);
    int    labelGap = MulDiv (s_kLabelGapDp, (int) dpi, s_kBaseDpi);
    int    ix       = rc.left;
    float  fontPx   = s_kFontDip * (float) dpi / (float) s_kBaseDpi;
    int    i        = 0;



    m_dpi     = dpi;
    m_rc      = rc;
    m_labeled = labeled;
    m_labelRc = {};

    for (i = 0; i < 3; i++)
    {
        m_segments[i].rc = {};
    }

    if (labeled)
    {
        int  labelW = MeasureLabelPx (m_textRenderer, s_kLabel, fontPx) + 3;

        m_labelRc = RECT { ix, rc.top, ix + labelW, rc.bottom };
        ix += labelW + labelGap;

        for (i = 0; i < GetSegmentCount(); i++)
        {
            m_segments[i].rc = RECT { ix, rc.top, ix + segW, rc.bottom };
            ix += segW + ((i + 1 < GetSegmentCount()) ? segGap : 0);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::Paint
//
////////////////////////////////////////////////////////////////////////////////

void InputClusterEntry::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme,
                               bool hovered, bool pressed, bool labeled)
{
    const CassoTheme &  casso = static_cast<const CassoTheme &> (theme);



    UNREFERENCED_PARAMETER (hovered);
    UNREFERENCED_PARAMETER (pressed);

    _ASSERTE (dynamic_cast<const CassoTheme *> (&theme) != nullptr);

    if (labeled)
    {
        PaintExpanded (painter, text, casso);
    }
    else
    {
        PaintCollapsed (painter, casso);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::GetTooltipAt
//
//  The segments carry no labels in ANY form -- the shared label only names
//  the group -- so their tooltips always show and lead with the mode.
//  Collapsed, the segments have no rects, and the entry's own name serves.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * InputClusterEntry::GetTooltipAt (int x, int y, RECT & anchor) const
{
    for (int i = 0; i < GetSegmentCount(); i++)
    {
        if (IsPointInRect (m_segments[i].rc, x, y))
        {
            anchor = m_segments[i].rc;
            return s_kTips[i];
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::OnMouseMove / OnMouseLeave / OnLButtonDown / OnClick
//
//  Press-and-release on the same segment toggles its mode; a click that
//  lands on no segment is the entry's, which collapsed means its menu.
//
////////////////////////////////////////////////////////////////////////////////

bool InputClusterEntry::OnMouseMove (int x, int y)
{
    bool  over = false;



    for (int i = 0; i < GetSegmentCount(); i++)
    {
        Segment &  seg = m_segments[i];

        seg.hovered = IsPointInRect (seg.rc, x, y);
        if (!seg.hovered) { seg.pressed = false; }
        over = over || seg.hovered;
    }

    return over;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::OnMouseLeave
//
////////////////////////////////////////////////////////////////////////////////

void InputClusterEntry::OnMouseLeave()
{
    for (Segment & seg : m_segments)
    {
        seg.hovered = false;
        seg.pressed = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool InputClusterEntry::OnLButtonDown (int x, int y)
{
    for (int i = 0; i < GetSegmentCount(); i++)
    {
        if (IsPointInRect (m_segments[i].rc, x, y))
        {
            m_segments[i].pressed = true;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::OnClick
//
////////////////////////////////////////////////////////////////////////////////

bool InputClusterEntry::OnClick (int x, int y)
{
    bool  consumed = false;



    for (int i = 0; i < 3; i++)
    {
        Segment &  seg        = m_segments[i];
        bool       wasPressed = seg.pressed;

        seg.pressed = false;

        if (!consumed && wasPressed && i < GetSegmentCount() && IsPointInRect (seg.rc, x, y))
        {
            if (m_sink) { m_sink (s_kModes[GetSlotAt (i)]); }
            consumed = true;
        }
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::PaintCollapsed
//
//  The joystick drawn in the same monoline pen as the segments, centered
//  where a font glyph would sit, and the same light the printer carries to
//  say that SOMETHING is mapped; which device it is lives in the menu behind
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void InputClusterEntry::PaintCollapsed (IDxuiPainter & painter, const CassoTheme & theme)
{
    float  bl        = (float) m_rc.left;
    float  bt        = (float) m_rc.top;
    float  bw        = (float) (m_rc.right  - m_rc.left);
    float  bh        = (float) (m_rc.bottom - m_rc.top);
    float  iconDip   = s_kIconDip * (float) m_dpi / (float) s_kBaseDpi;
    float  iconX     = bl + (bw - iconDip) * 0.5f;
    bool   anyMapped = m_arrowsJoystick || m_pointerMode != InputMappingMode::Off;
    RECT   box       = {};



    box.left   = (int) iconX;
    box.top    = (int) (bt + (bh - iconDip) * 0.5f);
    box.right  = box.left + (int) iconDip;
    box.bottom = box.top  + (int) iconDip;

    // The input devices are the only drawn icon: MDL2 has no joystick, and
    // one hand-drawn glyph beside two from the font would mismatch stroke
    // weight, so the whole device set is drawn with the same pen.
    PaintJoystickMono (painter, box, theme.navItemText);

    PrinterStatusLed::Paint (painter, iconX + iconDip + 1.0f,
                             bt + bh * 0.5f - iconDip * 0.48f, m_dpi,
                             anyMapped ? theme.ledActive : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::PaintExpanded
//
//  The shared "Input" label (it names the group, it is not a button) and the
//  LED + glyph segments. Hover chrome matches the buttons'; the LED is the
//  state: an outline would read as focus, a lit LED reads as ON. Lit takes
//  the theme's LED color, so it matches the drive widgets' lights under every
//  preset; unlit does NOT take the drive bar's ledIdle, which is that color
//  darkened and reads as a black dot on the strip.
//
////////////////////////////////////////////////////////////////////////////////

void InputClusterEntry::PaintExpanded (IDxuiPainter & painter, IDxuiTextRenderer & text, const CassoTheme & theme)
{
    HRESULT   hr       = S_OK;
    float     fontDip  = s_kFontDip * (float) m_dpi / (float) s_kBaseDpi;
    int       ledD     = MulDiv (s_kSegLedDp,    (int) m_dpi, s_kBaseDpi);
    int       ledGap   = MulDiv (s_kSegLedGapDp, (int) m_dpi, s_kBaseDpi);
    int       segPad   = MulDiv (s_kSegPadXDp,   (int) m_dpi, s_kBaseDpi);
    int       iconD    = MulDiv (s_kSegIconDp,   (int) m_dpi, s_kBaseDpi);
    uint32_t  offLed   = DxuiColor::ComputeTintForContrast (theme.navStrip, s_kOffLedContrast);
    uint32_t  labelInk = theme.navItemText;   // same ink as the button labels



    if (m_labelRc.right > m_labelRc.left)
    {
        hr = text.DrawString (s_kLabel,
                              (float) m_labelRc.left,
                              (float) m_labelRc.top,
                              (float) (m_labelRc.right  - m_labelRc.left),
                              (float) (m_labelRc.bottom - m_labelRc.top),
                              labelInk, fontDip, s_kFontFamily,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (int i = 0; i < GetSegmentCount(); i++)
    {
        const Segment &  seg    = m_segments[i];
        bool             active = seg.hovered || seg.pressed;
        float            sl     = (float) seg.rc.left;
        float            st     = (float) seg.rc.top;
        float            sw     = (float) (seg.rc.right  - seg.rc.left);
        float            sh     = (float) (seg.rc.bottom - seg.rc.top);



        if (active)
        {
            painter.FillRect    (sl, st, sw, sh, seg.pressed ? theme.buttonPressed
                                                             : theme.buttonHover);
            painter.OutlineRect (sl, st, sw, sh, 1.0f, theme.buttonBorder);
        }

        // LED left of the glyph, both vertically centered in the segment.
        {
            float  ledCx = sl + (float) segPad + (float) ledD * 0.5f;
            float  ledCy = st + sh * 0.5f;
            bool   on    = IsSegmentOn (GetSlotAt (i));

            // Unlit is an option not taken, not a dead bulb, so it carries
            // the tint a disabled checkbox fills with -- the same rule Dxui
            // applies, against the surface these actually sit on.
            painter.FillCircleApprox (ledCx, ledCy, (float) ledD * 0.5f,
                                      on ? theme.ledActive : offLed);
        }

        {
            int   boxL = seg.rc.left + segPad + ledD + ledGap;
            int   boxT = seg.rc.top + ((seg.rc.bottom - seg.rc.top) - iconD) / 2;
            RECT  box  = { boxL, boxT, boxL + iconD, boxT + iconD };

            if (m_monoline)
            {
                switch (i)
                {
                    case 0:  PaintJoystickMono (painter, box, theme.navItemText); break;
                    case 1:  PaintPaddleMono   (painter, box, theme.navItemText); break;

                    // The mouse is the one device MDL2 draws itself, and its
                    // glyph beats the drawn one at this size; it renders at
                    // the same em as every other icon on the bar.
                    case 2:
                    {
                        float  emDip = s_kIconDip * (float) m_dpi / (float) s_kBaseDpi;

                        hr = text.DrawString (s_kGlyphMouse, (float) box.left, (float) box.top,
                                              (float) iconD, (float) iconD,
                                              theme.navItemText, emDip, s_kIconFamily,
                                              DxuiTextHAlign::Center, DxuiTextVAlign::Center);
                        IGNORE_RETURN_VALUE (hr, S_OK);
                        break;
                    }

                    default: break;
                }
            }
            else
            {
                // The paddle master sits high in its grid next to the
                // joystick's; a small drop balances the pair visually.
                if (i == 1)
                {
                    int  drop = MulDiv (3, (int) m_dpi, s_kBaseDpi);

                    box.top    += drop;
                    box.bottom += drop;
                }

                switch (i)
                {
                    case 0:  InputDeviceGlyphs::PaintJoystickGlyph (painter, box, m_skeuo); break;
                    case 1:  InputDeviceGlyphs::PaintPaddleGlyph   (painter, box, m_skeuo); break;
                    case 2:  InputDeviceGlyphs::PaintMouseGlyph    (painter, box, m_skeuo); break;
                    default: break;
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::GetGlyphStroke
//
//  MDL2 draws roughly a fifteenth of its em as stroke; the floor keeps the
//  pen visible once the box is small enough for that ratio to fall under a
//  pixel.
//
////////////////////////////////////////////////////////////////////////////////

float InputClusterEntry::GetGlyphStroke (float w)
{
    return (std::max) (1.15f, w / 15.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::StrokeCircle + the monoline glyph painters
//
//  Monoline device glyphs in the Segoe MDL2 language the bar's other icons
//  speak: uniform stroke, dots for controls, no shading. Drawn rather than
//  taken from the font because MDL2 has no joystick or paddle, and one
//  hand-drawn glyph next to two font glyphs would mismatch stroke weight --
//  so all three are drawn with the same pen. Geometry is in box fractions,
//  so the set scales together.
//
////////////////////////////////////////////////////////////////////////////////

void InputClusterEntry::StrokeCircle (IDxuiPainter & painter, float cx, float cy,
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
//  InputClusterEntry::PaintJoystickMono
//
////////////////////////////////////////////////////////////////////////////////

void InputClusterEntry::PaintJoystickMono (IDxuiPainter & painter, const RECT & box, uint32_t ink)
{
    float  w        = (float) (box.right  - box.left);
    float  h        = (float) (box.bottom - box.top);
    float  stroke   = GetGlyphStroke (w);
    float  cx       = (float) box.left + w * 0.5f;
    float  knobR    = w * 0.16f;
    float  knobY    = (float) box.top + h * 0.21f;
    float  baseT    = (float) box.top + h * 0.66f;
    float  baseH    = h * 0.20f;
    float  baseHalf = w * 0.34f;



    // Ball, stick, slab, all in outline. Drawn for this size rather than
    // shrunk to it: the handle's turned profile (cap, shoulder, body, waist)
    // that this replaced was a blob by the time the box reached 19 dp.
    StrokeCircle           (painter, cx, knobY, knobR, stroke, ink);
    painter.DrawLineApprox (cx, knobY + knobR, cx, baseT, stroke, ink);
    painter.OutlineRect    (cx - baseHalf, baseT, baseHalf * 2.0f, baseH, stroke, ink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputClusterEntry::PaintPaddleMono
//
////////////////////////////////////////////////////////////////////////////////

void InputClusterEntry::PaintPaddleMono (IDxuiPainter & painter, const RECT & box, uint32_t ink)
{
    constexpr int  s_kArcSegments = 16;



    float  w       = (float) (box.right  - box.left);
    float  h       = (float) (box.bottom - box.top);
    float  stroke  = GetGlyphStroke (w);
    float  cx      = (float) box.left + w * 0.5f;
    float  cy      = (float) box.top + h * 0.34f;
    float  outerR  = w * 0.24f;
    float  botHalf = w * 0.13f;
    float  botY    = (float) box.top + h * 0.88f;
    float  aL      = 3.1415926f * 160.0f / 180.0f;
    float  aR      = 3.1415926f *  20.0f / 180.0f;



    // The keyhole read: one outer contour -- the knob arc wrapping the top,
    // from the left shoulder angle around to the right, handing off to the
    // tapering sides and a flat bottom -- with the knob itself as an inner
    // ring (screen coords, y down).
    for (int i = 0; i < s_kArcSegments; i++)
    {
        float  t0 = aL + (aR + 2.0f * 3.1415926f - aL) * (float) i       / (float) s_kArcSegments;
        float  t1 = aL + (aR + 2.0f * 3.1415926f - aL) * (float) (i + 1) / (float) s_kArcSegments;

        painter.DrawLineApprox (cx + outerR * std::cos (t0), cy + outerR * std::sin (t0),
                                cx + outerR * std::cos (t1), cy + outerR * std::sin (t1),
                                stroke, ink);
    }

    painter.DrawLineApprox (cx + outerR * std::cos (aL), cy + outerR * std::sin (aL),
                            cx - botHalf, botY, stroke, ink);
    painter.DrawLineApprox (cx + outerR * std::cos (aR), cy + outerR * std::sin (aR),
                            cx + botHalf, botY, stroke, ink);
    painter.DrawLineApprox (cx - botHalf, botY, cx + botHalf, botY, stroke, ink);

    StrokeCircle (painter, cx, cy, w * 0.115f, stroke, ink);
}
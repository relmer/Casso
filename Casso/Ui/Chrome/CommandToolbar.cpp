#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "CassoTheme.h"
#include "CommandToolbar.h"

#include "../../Resource.h"




// Layout metrics (DIP).
static constexpr int      s_kBaseDpi        = 96;
static constexpr int      s_kBarPadXDp      = 10;   // strip left/right padding
static constexpr int      s_kBtnPadXDp      = 10;   // inside a button, around content
static constexpr int      s_kBtnMarginYDp   = 5;    // button top/bottom inset in the strip
static constexpr int      s_kBtnGapDp       = 4;    // between buttons in a group
static constexpr int      s_kGroupGapDp     = 18;   // between button groups
static constexpr int      s_kIconGapDp      = 7;    // icon-to-label gap
static constexpr int      s_kSliderWidthDp    = 120;
static constexpr int      s_kSliderMinWidthDp = 60;   // narrowest the volume slider shrinks to
static constexpr int      s_kSliderMaxHDp     = 30;   // slider stays this tall, centered in the band
static constexpr float    s_kIconDip        = 15.0f;
static constexpr float    s_kFontDip        = 13.0f;
static constexpr float    s_kStackedFontDip = 11.0f;  // label under the icon (ribbon mode)
static constexpr float    s_kFallbackCharPx = 7.5f;

// Band thickness per presentation mode (see CommandToolbar::Mode): the
// stacked ribbon needs the extra rows for icon-over-label.
static constexpr int      s_kBandLabelRightDp = 42;
static constexpr int      s_kBandLabelBelowDp = 56;
static constexpr int      s_kBandIconOnlyDp   = 40;
static constexpr int      s_kStackedPadXDp    = 8;    // tighter side padding in ribbon mode

static constexpr const wchar_t * s_kFontFamily = DxuiTheme::kBodyFace;
static constexpr const wchar_t * s_kIconFamily = L"Segoe MDL2 Assets";

// Segoe MDL2 Assets codepoints.
static constexpr wchar_t  s_kGlyphSettings   = L'\uE713';   // gear
static constexpr wchar_t  s_kGlyphScreenshot = L'\uE722';   // camera
static constexpr wchar_t  s_kGlyphReset      = L'\uE72C';   // refresh arrow
static constexpr wchar_t  s_kGlyphPower      = L'\uE7E8';   // power symbol
static constexpr wchar_t  s_kGlyphVolume     = L'\uE767';   // speaker
static constexpr wchar_t  s_kGlyphMuted      = L'\uE74F';   // muted speaker
static constexpr wchar_t  s_kGlyphPrint      = L'\uE749';   // printer (monoline, matches the set)

// Ink + hover treatment over the machine-tinted (light case-color) strip;
// the un-tinted fallback uses the theme's chrome colors instead.
static constexpr uint32_t  s_kTintInk        = 0xFF3A3428;   // dark ink on beige / platinum
static constexpr uint32_t  s_kTintHover      = 0x1E000000;
static constexpr uint32_t  s_kTintPressed    = 0x33000000;
static constexpr uint32_t  s_kTintBorder     = 0x50000000;
static constexpr uint32_t  s_kTintHairline   = 0x5A46402F;   // strip's bottom edge shade




////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::CommandToolbar
//
//  Fixed command set (spec 015 DCR-2 decision): Settings + Printer, the
//  volume group, then Screenshot / Reset / Power. Every command id is an
//  existing IDM_* routed through the menu's HandleCommand path.
//
////////////////////////////////////////////////////////////////////////////////

CommandToolbar::CommandToolbar ()
{
    m_focusable = false;

    m_buttons.push_back (Button { IDM_VIEW_SETTINGS,        s_kGlyphSettings,   L"Settings",   false });
    m_buttons.push_back (Button { IDM_PRINTER_PREVIEW,      s_kGlyphPrint,      L"Printer",    true  });
    m_buttons.push_back (Button { IDM_EDIT_COPY_SCREENSHOT, s_kGlyphScreenshot, L"Screenshot", false });
    m_buttons.push_back (Button { IDM_MACHINE_RESET,        s_kGlyphReset,      L"Reset",      false });
    m_buttons.push_back (Button { IDM_MACHINE_POWERCYCLE,   s_kGlyphPower,      L"Power",      false });

    m_muteButton.id    = 0;   // not a dispatch: toggles mute locally
    m_muteButton.glyph = s_kGlyphVolume;
    m_muteButton.label = L"Volume";

    m_volumeSlider.SetRange         (0.0f, 100.0f);
    m_volumeSlider.SetStep          (1.0f);
    m_volumeSlider.SetSuffix        (L"%");
    m_volumeSlider.SetDecimalPlaces (0);
    m_volumeSlider.SetShowTicks     (false);
    m_volumeSlider.SetValue         (100.0f);

    m_volumeSlider.SetOnChange ([this] (float v)
    {
        m_volume01 = v / 100.0f;
        if (m_volumeSink) { m_volumeSink (m_volume01, m_muted); }
    });
}




////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::SetVolume
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::SetVolume (float volume01, bool muted)
{
    m_volume01 = std::clamp (volume01, 0.0f, 1.0f);
    m_muted    = muted;

    m_volumeSlider.SetValue   (m_volume01 * 100.0f);
    m_volumeSlider.SetEnabled (!m_muted);
    m_muteButton.glyph = m_muted ? s_kGlyphMuted : s_kGlyphVolume;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PointIn / HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::PointIn (const RECT & rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}


bool CommandToolbar::HitTest (int x, int y) const
{
    return PointIn (m_barRect, x, y);
}




////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::StatusCore
//
//  PrinterStatus -> LED core colour (same mapping the standalone indicator
//  used, so the light keeps its meaning across the move into the toolbar).
//
////////////////////////////////////////////////////////////////////////////////

uint32_t CommandToolbar::StatusCore (PrinterStatus status)
{
    switch (status)
    {
    case PrinterStatus::Receiving: return 0xFF3FD35A;   // green: printing now
    case PrinterStatus::Pending:   return 0xFFF5A623;   // amber: page waiting
    case PrinterStatus::Error:     return 0xFFE5484D;   // red:   failed
    case PrinterStatus::Idle:
    default:                       return 0xFF3B7A46;   // dim green: powered, idle
    }
}


// A small status-light dot riding the printer glyph's corner (halo + core in
// the PrinterStatus colour) -- the monoline glyph keeps the icon set uniform
// while the LED keeps the at-a-glance printer state.
static void PaintStatusLed (IDxuiPainter & painter, float cx, float cy, UINT dpi, uint32_t core)
{
    float     r    = 2.0f * (float) dpi / (float) s_kBaseDpi;
    uint32_t  halo = (core & 0x00FFFFFFu) | 0x66000000u;

    painter.FillCircleApprox (cx, cy, r * 1.8f, halo);
    painter.FillCircleApprox (cx, cy, r,        core);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::Layout
//
//  Lays the buttons left-to-right from the strip's left edge: [Settings]
//  [Printer] | [Volume + slider] | [Screenshot] [Reset] [Power]. Button
//  widths follow their measured label (icon + gap + label + padding); the
//  strip rect is the dock band handed in by the shell.
//
////////////////////////////////////////////////////////////////////////////////

int CommandToolbar::PlanForWidth (int clientWidthPx, const DxuiDpiScaler & scaler)
{
    UINT   dpi        = (scaler.Dpi() == 0) ? (UINT) s_kBaseDpi : scaler.Dpi();
    int    padX       = MulDiv (s_kBtnPadXDp,     (int) dpi, s_kBaseDpi);
    int    padXStack  = MulDiv (s_kStackedPadXDp, (int) dpi, s_kBaseDpi);
    int    btnGap     = MulDiv (s_kBtnGapDp,      (int) dpi, s_kBaseDpi);
    int    groupGap   = MulDiv (s_kGroupGapDp,    (int) dpi, s_kBaseDpi);
    int    iconGap    = MulDiv (s_kIconGapDp,     (int) dpi, s_kBaseDpi);
    int    sliderW    = MulDiv (s_kSliderWidthDp, (int) dpi, s_kBaseDpi);
    float  iconDip    = s_kIconDip * (float) dpi / (float) s_kBaseDpi;
    int    avail      = clientWidthPx - MulDiv (s_kBarPadXDp, (int) dpi, s_kBaseDpi) * 2;

    auto  measure = [&] (const wchar_t * label, float fontDip) -> int
    {
        float  w = 0.0f;
        float  h = 0.0f;

        if (m_textRenderer != nullptr &&
            SUCCEEDED (m_textRenderer->MeasureString (label, fontDip, s_kFontFamily, w, h)) && w > 0.0f)
        {
            return (int) (w + 0.5f);
        }
        return (int) ((float) wcslen (label) * s_kFallbackCharPx * (float) dpi / (float) s_kBaseDpi);
    };

    auto  iconWidth = [&] (const Button & btn) -> int
    {
        (void) btn;   // uniform monoline glyphs: every icon is one MDL2 cell
        return (int) (iconDip + 0.5f);
    };

    auto  buttonWidth = [&] (const Button & btn, Mode mode) -> int
    {
        switch (mode)
        {
        case Mode::LabelRight:
            return padX * 2 + iconWidth (btn) + iconGap +
                   measure (btn.label, s_kFontDip * (float) dpi / (float) s_kBaseDpi);
        case Mode::LabelBelow:
            return padXStack * 2 + (std::max) (iconWidth (btn),
                   measure (btn.label, s_kStackedFontDip * (float) dpi / (float) s_kBaseDpi));
        case Mode::IconOnly:
        default:
            return padX * 2 + iconWidth (btn);
        }
    };

    // Total width a mode wants: six command buttons + the mute button + the
    // slider, with group gaps between the three clusters.
    auto  totalWidth = [&] (Mode mode) -> int
    {
        int  w = 0;

        for (const Button & b : m_buttons) { w += buttonWidth (b, mode) + btnGap; }
        w += buttonWidth (m_muteButton, mode) + sliderW;
        w += (groupGap - btnGap) + groupGap + groupGap;
        return w;
    };

    // Widest presentation that fits wins; icon-only additionally shrinks the
    // slider in Layout when even it overflows.
    if      (totalWidth (Mode::LabelRight) <= avail) { m_mode = Mode::LabelRight; m_bandDp = s_kBandLabelRightDp; }
    else if (totalWidth (Mode::LabelBelow) <= avail) { m_mode = Mode::LabelBelow; m_bandDp = s_kBandLabelBelowDp; }
    else                                             { m_mode = Mode::IconOnly;   m_bandDp = s_kBandIconOnlyDp;   }

    // Stash the per-mode widths for Layout (recomputed there against the same
    // width, but keeping the lambda results here would just duplicate state).
    return m_bandDp;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::Layout
//
//  Places the buttons left-to-right in the current mode: [Settings] [Printer]
//  | [Volume + slider] | [Screenshot] [Reset] [Power]. The mode is re-planned
//  against this exact strip width so mode and layout can never disagree; in
//  icon-only mode the volume slider then shrinks toward its minimum if even
//  the icons overflow.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    PlanForWidth (boundsDip.right - boundsDip.left, scaler);

    UINT   dpi        = (scaler.Dpi() == 0) ? (UINT) s_kBaseDpi : scaler.Dpi();
    int    padX       = MulDiv (s_kBtnPadXDp,     (int) dpi, s_kBaseDpi);
    int    padXStack  = MulDiv (s_kStackedPadXDp, (int) dpi, s_kBaseDpi);
    int    marginY    = MulDiv (s_kBtnMarginYDp,  (int) dpi, s_kBaseDpi);
    int    btnGap     = MulDiv (s_kBtnGapDp,      (int) dpi, s_kBaseDpi);
    int    groupGap   = MulDiv (s_kGroupGapDp,    (int) dpi, s_kBaseDpi);
    int    iconGap    = MulDiv (s_kIconGapDp,     (int) dpi, s_kBaseDpi);
    int    sliderW    = MulDiv (s_kSliderWidthDp, (int) dpi, s_kBaseDpi);
    int    sliderMinW = MulDiv (s_kSliderMinWidthDp, (int) dpi, s_kBaseDpi);
    int    sliderMaxH = MulDiv (s_kSliderMaxHDp,  (int) dpi, s_kBaseDpi);
    float  iconDip    = s_kIconDip * (float) dpi / (float) s_kBaseDpi;
    int    barPad     = MulDiv (s_kBarPadXDp, (int) dpi, s_kBaseDpi);
    int    avail      = (boundsDip.right - boundsDip.left) - barPad * 2;
    int    x          = boundsDip.left + barPad;
    int    top        = boundsDip.top + marginY;
    int    bottom     = boundsDip.bottom - marginY;

    m_dpi     = dpi;
    m_barRect = boundsDip;

    auto  measure = [&] (const wchar_t * label, float fontDip) -> int
    {
        float  w = 0.0f;
        float  h = 0.0f;

        if (m_textRenderer != nullptr &&
            SUCCEEDED (m_textRenderer->MeasureString (label, fontDip, s_kFontFamily, w, h)) && w > 0.0f)
        {
            return (int) (w + 0.5f);
        }
        return (int) ((float) wcslen (label) * s_kFallbackCharPx * (float) dpi / (float) s_kBaseDpi);
    };

    auto  iconWidth = [&] (const Button & btn) -> int
    {
        (void) btn;   // uniform monoline glyphs: every icon is one MDL2 cell
        return (int) (iconDip + 0.5f);
    };

    auto  buttonWidth = [&] (const Button & btn) -> int
    {
        switch (m_mode)
        {
        case Mode::LabelRight:
            return padX * 2 + iconWidth (btn) + iconGap +
                   measure (btn.label, s_kFontDip * (float) dpi / (float) s_kBaseDpi);
        case Mode::LabelBelow:
            return padXStack * 2 + (std::max) (iconWidth (btn),
                   measure (btn.label, s_kStackedFontDip * (float) dpi / (float) s_kBaseDpi));
        case Mode::IconOnly:
        default:
            return padX * 2 + iconWidth (btn);
        }
    };

    // Icon-only overflow: shrink the slider toward its minimum before
    // anything clips off the right edge.
    if (m_mode == Mode::IconOnly)
    {
        int  wanted = (groupGap - btnGap) + groupGap + groupGap + sliderW;

        for (const Button & b : m_buttons) { wanted += buttonWidth (b) + btnGap; }
        wanted += buttonWidth (m_muteButton);

        if (wanted > avail)
        {
            sliderW = (std::max) (sliderMinW, sliderW - (wanted - avail));
        }
    }

    auto  place = [&] (Button & btn)
    {
        int  w = buttonWidth (btn);

        btn.rc = RECT { x, top, x + w, bottom };
        x     += w + btnGap;
    };

    place (m_buttons[0]);                       // Settings
    place (m_buttons[1]);                       // Printer
    x += groupGap - btnGap;

    place (m_muteButton);                       // Volume (mute toggle)
    {
        // The slider stays a comfortable height, vertically centered -- in the
        // taller ribbon band a full-height slider would look stretched.
        int   bandH   = bottom - top;
        int   sliderH = (std::min) (bandH, sliderMaxH);
        int   sy      = top + (bandH - sliderH) / 2;
        RECT  sliderRc = { x, sy, x + sliderW, sy + sliderH };

        m_volumeSlider.SetRect (sliderRc);
        m_volumeSlider.SetDpi  (dpi);
        x += sliderW + groupGap;
    }

    place (m_buttons[2]);                       // Screenshot
    place (m_buttons[3]);                       // Reset
    place (m_buttons[4]);                       // Power

    SetBounds (m_barRect);
}




////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::TooltipAt
//
//  Icon-only mode has no labels, so the hovered button's meaning surfaces as
//  a tooltip (the shell owns the DxuiTooltip and its dwell timing). The mute
//  button's tooltip reflects the action it would take.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CommandToolbar::TooltipAt (int x, int y, RECT & anchor) const
{
    if (m_mode != Mode::IconOnly)
    {
        return nullptr;   // labels are visible; no tooltip needed
    }

    for (const Button & btn : m_buttons)
    {
        if (btn.enabled && PointIn (btn.rc, x, y))
        {
            anchor = btn.rc;
            return btn.label;
        }
    }

    if (PointIn (m_muteButton.rc, x, y))
    {
        anchor = m_muteButton.rc;
        return m_muted ? L"Unmute" : L"Mute";
    }

    return nullptr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::OnToolbarMouseMove / Leave / LButtonDown / LButtonUp
//
//  Shell-forwarded input. The slider gets first claim while it is tracking a
//  drag; otherwise hover / press states update per button and a click on
//  release dispatches the command (mute toggles locally).
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::OnToolbarMouseMove (int x, int y, bool leftDown)
{
    UNREFERENCED_PARAMETER (leftDown);

    bool  over = false;

    if (m_volumeSlider.OnMouseMove (x, y))
    {
        over = true;
    }

    for (Button & btn : m_buttons)
    {
        btn.hovered = btn.enabled && PointIn (btn.rc, x, y);
        if (!btn.hovered) { btn.pressed = false; }
        over = over || btn.hovered;
    }
    m_muteButton.hovered = PointIn (m_muteButton.rc, x, y);
    if (!m_muteButton.hovered) { m_muteButton.pressed = false; }

    return over || m_muteButton.hovered || PointIn (m_barRect, x, y);
}


void CommandToolbar::OnToolbarMouseLeave ()
{
    for (Button & btn : m_buttons)
    {
        btn.hovered = false;
        btn.pressed = false;
    }
    m_muteButton.hovered = false;
    m_muteButton.pressed = false;
}


bool CommandToolbar::OnToolbarLButtonDown (int x, int y)
{
    if (!m_muted && m_volumeSlider.OnLButtonDown (x, y))
    {
        return true;
    }

    for (Button & btn : m_buttons)
    {
        if (btn.enabled && PointIn (btn.rc, x, y))
        {
            btn.pressed = true;
            return true;
        }
    }
    if (PointIn (m_muteButton.rc, x, y))
    {
        m_muteButton.pressed = true;
        return true;
    }

    return PointIn (m_barRect, x, y);   // eat clicks on the bar's dead space
}


bool CommandToolbar::OnToolbarLButtonUp (int x, int y)
{
    if (m_volumeSlider.OnLButtonUp (x, y))
    {
        return true;
    }

    for (Button & btn : m_buttons)
    {
        bool  wasPressed = btn.pressed;

        btn.pressed = false;

        if (wasPressed && btn.enabled && PointIn (btn.rc, x, y))
        {
            if (m_dispatch) { m_dispatch (btn.id); }
            return true;
        }
    }

    {
        bool  wasPressed = m_muteButton.pressed;

        m_muteButton.pressed = false;

        if (wasPressed && PointIn (m_muteButton.rc, x, y))
        {
            SetVolume (m_volume01, !m_muted);
            if (m_volumeSink) { m_volumeSink (m_volume01, m_muted); }
            return true;
        }
    }

    return PointIn (m_barRect, x, y);
}




////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PaintButton
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintButton (Button & btn, IDxuiPainter & painter,
                                  IDxuiTextRenderer & text, const CassoTheme & theme)
{
    HRESULT   hr       = S_OK;
    bool      active   = btn.hovered || btn.pressed;
    float     bl       = (float) btn.rc.left;
    float     bt       = (float) btn.rc.top;
    float     bw       = (float) (btn.rc.right  - btn.rc.left);
    float     bh       = (float) (btn.rc.bottom - btn.rc.top);
    float     fontDip  = s_kFontDip * (float) m_dpi / (float) s_kBaseDpi;
    float     iconDip  = s_kIconDip * (float) m_dpi / (float) s_kBaseDpi;
    int       padX     = MulDiv (s_kBtnPadXDp, (int) m_dpi, s_kBaseDpi);
    int       iconGap  = MulDiv (s_kIconGapDp, (int) m_dpi, s_kBaseDpi);
    bool      tinted   = (m_tintTop != 0);
    uint32_t  ink      = tinted ? s_kTintInk : theme.navItemText;
    float     iconW    = iconDip;
    float     textX    = 0.0f;

    if (!btn.enabled)
    {
        ink = (ink & 0x00FFFFFFu) | 0x60000000u;   // dimmed
    }

    if (active)
    {
        uint32_t  fill   = btn.pressed ? (tinted ? s_kTintPressed : theme.buttonPressed)
                                       : (tinted ? s_kTintHover   : theme.buttonHover);
        uint32_t  border = tinted ? s_kTintBorder : theme.buttonBorder;

        painter.FillRect    (bl, bt, bw, bh, fill);
        painter.OutlineRect (bl, bt, bw, bh, 1.0f, border);
    }

    // Ribbon mode: icon centered over a small label. The icon gets the region
    // above the label row; the label spans the button width, centered.
    if (m_mode == Mode::LabelBelow)
    {
        float  stackedDip = s_kStackedFontDip * (float) m_dpi / (float) s_kBaseDpi;
        float  labelH     = stackedDip + 4.0f;
        float  iconRegH   = bh - labelH;

        {
            wchar_t   glyph[2] = { btn.glyph, 0 };

            hr = text.DrawString (glyph, bl, bt, bw, iconRegH,
                                  ink, iconDip, s_kIconFamily,
                                  DxuiTextRenderer::HAlign::Center,
                                  DxuiTextRenderer::VAlign::Center);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (btn.statusLed)
        {
            PaintStatusLed (painter, bl + bw * 0.5f + iconDip * 0.62f,
                            bt + (iconRegH - iconDip) * 0.5f + 2.0f, m_dpi,
                            StatusCore (m_printerStatus));
        }

        hr = text.DrawString (btn.label, bl, bt + iconRegH - 2.0f, bw, labelH,
                              ink, stackedDip, s_kFontFamily,
                              DxuiTextRenderer::HAlign::Center,
                              DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    // Icon-only mode centers the icon; LabelRight keeps it left-padded with
    // the label beside it.
    {
        bool   centered = (m_mode == Mode::IconOnly);
        float  iconX    = centered ? bl + (bw - iconDip) * 0.5f : bl + (float) padX;

        {
            wchar_t   glyph[2] = { btn.glyph, 0 };

            hr = text.DrawString (glyph, iconX, bt, iconDip + 2.0f, bh,
                                  ink, iconDip, s_kIconFamily,
                                  DxuiTextRenderer::HAlign::Left,
                                  DxuiTextRenderer::VAlign::Center);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (btn.statusLed)
        {
            PaintStatusLed (painter, iconX + iconDip + 1.0f,
                            bt + bh * 0.5f - iconDip * 0.48f, m_dpi,
                            StatusCore (m_printerStatus));
        }

        if (m_mode == Mode::IconOnly)
        {
            return;   // tooltips carry the labels
        }

        textX = bl + (float) padX + iconW + (float) iconGap;

        hr = text.DrawString (btn.label, textX, bt,
                              (float) btn.rc.right - textX, bh,
                              ink, fontDip, s_kFontFamily,
                              DxuiTextRenderer::HAlign::Left,
                              DxuiTextRenderer::VAlign::CenterOnCapHeight);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::Paint
//
//  A bottom hairline separates the strip from the emulator viewport; buttons
//  and the volume group paint over the window's existing chrome backdrop
//  (frameless until hovered, like the rest of the chrome).
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & dxuiTheme)
{
    _ASSERTE (dynamic_cast<const CassoTheme *> (&dxuiTheme) != nullptr);
    const CassoTheme & theme = static_cast<const CassoTheme &> (dxuiTheme);

    float  bl = (float) m_barRect.left;
    float  btTop = (float) m_barRect.top;
    float  bw = (float) (m_barRect.right - m_barRect.left);
    float  bhAll = (float) (m_barRect.bottom - m_barRect.top);

    if (bw <= 0.0f)
    {
        return;
    }

    // Machine-colored chrome: the strip wears the selected machine's case
    // color (Disk ][ beige for the II family, platinum for //c-era machines),
    // tying the toolbar to the hardware like the drive widgets. Without a
    // tint it stays on the theme backdrop.
    if (m_tintTop != 0)
    {
        painter.FillGradientRect (bl, btTop, bw, bhAll, m_tintTop, m_tintBot);
        painter.FillRect (bl, (float) m_barRect.bottom - 1.0f, bw, 1.0f, s_kTintHairline);
    }
    else
    {
        painter.FillRect (bl, (float) m_barRect.bottom - 1.0f, bw, 1.0f, theme.buttonBorder);
    }

    // The printer button follows card presence: no card, no printer button.
    m_buttons[1].enabled = m_printerPresent;

    for (Button & btn : m_buttons)
    {
        PaintButton (btn, painter, text, theme);
    }

    PaintButton (m_muteButton, painter, text, theme);
    m_volumeSlider.Paint (painter, text, dxuiTheme);
}

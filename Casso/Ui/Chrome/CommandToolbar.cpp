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
static constexpr int      s_kSliderWidthDp  = 120;
static constexpr int      s_kPrinterIconWDp = 24;
static constexpr float    s_kIconDip        = 15.0f;
static constexpr float    s_kFontDip        = 13.0f;
static constexpr float    s_kFallbackCharPx = 7.5f;

static constexpr const wchar_t * s_kFontFamily = DxuiTheme::kBodyFace;
static constexpr const wchar_t * s_kIconFamily = L"Segoe MDL2 Assets";

// Segoe MDL2 Assets codepoints.
static constexpr wchar_t  s_kGlyphSettings   = L'\uE713';   // gear
static constexpr wchar_t  s_kGlyphScreenshot = L'\uE722';   // camera
static constexpr wchar_t  s_kGlyphReset      = L'\uE72C';   // refresh arrow
static constexpr wchar_t  s_kGlyphPower      = L'\uE7E8';   // power symbol
static constexpr wchar_t  s_kGlyphVolume     = L'\uE767';   // speaker
static constexpr wchar_t  s_kGlyphMuted      = L'\uE74F';   // muted speaker

// Mini ImageWriter II palette (from the retired PrinterIndicator).
static constexpr uint32_t  s_kPlatTop  = 0xFFEDE9DD;
static constexpr uint32_t  s_kPlatBot  = 0xFFCAC5B5;
static constexpr uint32_t  s_kEdge     = 0xFF8C8879;
static constexpr uint32_t  s_kSmoked   = 0xFF42454B;
static constexpr uint32_t  s_kPaper    = 0xFFFBFBF6;




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
    m_buttons.push_back (Button { IDM_PRINTER_PREVIEW,      0,                  L"Printer",    true  });
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

void CommandToolbar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    UINT   dpi      = (scaler.Dpi() == 0) ? (UINT) s_kBaseDpi : scaler.Dpi();
    int    padX     = MulDiv (s_kBtnPadXDp,    (int) dpi, s_kBaseDpi);
    int    marginY  = MulDiv (s_kBtnMarginYDp, (int) dpi, s_kBaseDpi);
    int    btnGap   = MulDiv (s_kBtnGapDp,     (int) dpi, s_kBaseDpi);
    int    groupGap = MulDiv (s_kGroupGapDp,   (int) dpi, s_kBaseDpi);
    int    iconGap  = MulDiv (s_kIconGapDp,    (int) dpi, s_kBaseDpi);
    int    sliderW  = MulDiv (s_kSliderWidthDp,(int) dpi, s_kBaseDpi);
    float  fontDip  = s_kFontDip * (float) dpi / (float) s_kBaseDpi;
    float  iconDip  = s_kIconDip * (float) dpi / (float) s_kBaseDpi;
    int    x        = boundsDip.left + MulDiv (s_kBarPadXDp, (int) dpi, s_kBaseDpi);
    int    top      = boundsDip.top + marginY;
    int    bottom   = boundsDip.bottom - marginY;

    m_dpi     = dpi;
    m_barRect = boundsDip;

    auto  measure = [&] (const wchar_t * label) -> int
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

    auto  place = [&] (Button & btn)
    {
        int  iconW = btn.printerGlyph ? MulDiv (s_kPrinterIconWDp, (int) dpi, s_kBaseDpi)
                                      : (int) (iconDip + 0.5f);
        int  w     = padX * 2 + iconW + iconGap + measure (btn.label);

        btn.rc = RECT { x, top, x + w, bottom };
        x     += w + btnGap;
    };

    place (m_buttons[0]);                       // Settings
    place (m_buttons[1]);                       // Printer
    x += groupGap - btnGap;

    place (m_muteButton);                       // Volume (mute toggle)
    {
        RECT  sliderRc = { x, top, x + sliderW, bottom };

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
//  CommandToolbar::PaintMiniPrinter
//
//  The Printer button's icon: a compact skeuomorphic ImageWriter II (paper
//  rising from the platen, platinum chassis, smoked hood) with the status
//  light on the front panel -- the retired standalone indicator, shrunk to
//  toolbar-icon size so the printer keeps its distinctive glyph and LED.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintMiniPrinter (const RECT & rc, IDxuiPainter & painter)
{
    DxuiDpiScaler  scaler;
    float          x = (float) rc.left;
    float          y = (float) rc.top;
    float          w = (float) (rc.right  - rc.left);
    float          h = (float) (rc.bottom - rc.top);

    scaler.SetDpi (m_dpi);

    const float  bodyTop = y + h * 0.34f;
    const float  bodyH   = y + h - bodyTop;

    // Fanfold paper rising behind the chassis.
    {
        float  paperW = w * 0.52f;
        float  paperX = x + (w - paperW) * 0.5f;

        painter.FillRect (paperX, y, paperW, bodyTop + bodyH * 0.2f - y, s_kPaper);
    }

    // Platinum chassis + smoked hood.
    painter.FillGradientRect (x, bodyTop, w, bodyH, s_kPlatTop, s_kPlatBot);
    painter.OutlineRect      (x, bodyTop, w, bodyH, 1.0f, s_kEdge);
    painter.FillRect         (x + scaler.Pxf (1.0f), bodyTop + scaler.Pxf (1.0f),
                              w - scaler.Pxf (2.0f), bodyH * 0.32f, s_kSmoked);

    // Status light on the front-right panel.
    {
        uint32_t  core = StatusCore (m_printerStatus);
        uint32_t  halo = (core & 0x00FFFFFFu) | 0x66000000u;
        float     r    = scaler.Pxf (1.6f);
        float     cx   = x + w - scaler.Pxf (4.0f);
        float     cy   = bodyTop + bodyH * 0.72f;

        painter.FillCircleApprox (cx, cy, r * 1.9f, halo);
        painter.FillCircleApprox (cx, cy, r,        core);
    }
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
    uint32_t  ink      = theme.navItemText;
    float     iconW    = iconDip;
    float     textX    = 0.0f;

    if (!btn.enabled)
    {
        ink = (ink & 0x00FFFFFFu) | 0x60000000u;   // dimmed
    }

    if (active)
    {
        uint32_t  fill = btn.pressed ? theme.buttonPressed
                                     : (btn.hovered ? theme.buttonHover : theme.buttonIdle);

        painter.FillRect    (bl, bt, bw, bh, fill);
        painter.OutlineRect (bl, bt, bw, bh, 1.0f, theme.buttonBorder);
    }

    if (btn.printerGlyph)
    {
        int   iconWi = MulDiv (s_kPrinterIconWDp, (int) m_dpi, s_kBaseDpi);
        int   iconH  = (int) (bh * 0.62f);
        RECT  icon   = { btn.rc.left + padX,
                         btn.rc.top + ((int) bh - iconH) / 2,
                         btn.rc.left + padX + iconWi,
                         btn.rc.top + ((int) bh - iconH) / 2 + iconH };

        PaintMiniPrinter (icon, painter);
        iconW = (float) iconWi;
    }
    else
    {
        wchar_t   glyph[2] = { btn.glyph, 0 };

        hr = text.DrawString (glyph, bl + (float) padX, bt, iconDip + 2.0f, bh,
                              ink, iconDip, s_kIconFamily,
                              DxuiTextRenderer::HAlign::Left,
                              DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    textX = bl + (float) padX + iconW + (float) iconGap;

    hr = text.DrawString (btn.label, textX, bt,
                          (float) btn.rc.right - textX, bh,
                          ink, fontDip, s_kFontFamily,
                          DxuiTextRenderer::HAlign::Left,
                          DxuiTextRenderer::VAlign::CenterOnCapHeight);
    IGNORE_RETURN_VALUE (hr, S_OK);
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
    float  bw = (float) (m_barRect.right - m_barRect.left);

    if (bw <= 0.0f)
    {
        return;
    }

    painter.FillRect (bl, (float) m_barRect.bottom - 1.0f, bw, 1.0f, theme.buttonBorder);

    // The printer button follows card presence: no card, no printer button.
    m_buttons[1].enabled = m_printerPresent;

    for (Button & btn : m_buttons)
    {
        PaintButton (btn, painter, text, theme);
    }

    PaintButton (m_muteButton, painter, text, theme);
    m_volumeSlider.Paint (painter, text, dxuiTheme);
}

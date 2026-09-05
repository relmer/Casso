#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "CassoTheme.h"
#include "CommandToolbar.h"
#include "InputDeviceGlyphs.h"

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

// Volume flyout (vertical slider + readout under the track).
static constexpr int      s_kFlyoutWidthDp    = 56;
static constexpr int      s_kFlyoutHeightDp   = 154;
static constexpr int      s_kFlyoutPadDp      = 8;
static constexpr int      s_kFlyoutDropDp     = 2;    // gap under the bar

// Input cluster: LED + glyph segments under one shared label.
static constexpr int      s_kSegIconDp        = 28;
static constexpr int      s_kSegPadXDp        = 5;
static constexpr int      s_kSegLedDp         = 7;    // LED diameter
static constexpr int      s_kSegLedGapDp      = 4;
static constexpr int      s_kSegGapDp         = 2;
static constexpr int      s_kInputLabelGapDp  = 8;    // label -> first segment

static constexpr const wchar_t * s_kInputLabel = L"Input";

// Per-segment tooltips. The segments carry no labels of their own, so the
// tips lead with the mode name the old selector showed as text.
static constexpr const wchar_t * s_kTipJoystickSeg =
    L"Joystick mode: map the arrow keys and X/Z to the joystick and\n"
    L"buttons 0/1. Click to toggle; works alongside the pointer devices.";
static constexpr const wchar_t * s_kTipPaddleSeg =
    L"Paddle mode: captures the mouse and maps it to paddles 0/1 and\n"
    L"buttons 0/1. Press ESC to exit this mode.";
static constexpr const wchar_t * s_kTipMouseSeg =
    L"Mouse mode: send host mouse inputs to the machine.";





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::CommandToolbar
//
//  Fixed command set (spec 015 DCR-2 decision): Settings + Printer, the
//  volume group, then Screenshot / Reset / Power. Every command id is an
//  existing IDM_* routed through the menu's HandleCommand path.
//
////////////////////////////////////////////////////////////////////////////////

CommandToolbar::CommandToolbar()
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

    m_volumeSlider.SetVertical      (true);
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

    // The readout names the state, not just the number: "Muted" while muted
    // (the slider shows 0 then), the percentage otherwise.
    m_volumeSlider.SetValueFormatter ([this] (float v) -> std::wstring
    {
        wchar_t  buf[16] = {};

        if (m_muted)
        {
            return L"Muted";
        }

        swprintf_s (buf, L"%d%%", (int) std::lround (v));
        return buf;
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

    // Muted DISPLAYS as silence -- slider at the bottom, 0% -- while the
    // stored level survives underneath, so unmuting restores exactly what
    // the user had. The slider is disabled while muted, so the zeroed
    // display can never be dragged into becoming the stored value.
    m_volumeSlider.SetValue   (m_muted ? 0.0f : m_volume01 * 100.0f);
    m_volumeSlider.SetEnabled (!m_muted);
    m_muteButton.glyph = m_muted ? s_kGlyphMuted : s_kGlyphVolume;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::IsPointInRect / HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::IsPointInRect (const RECT & rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}


bool CommandToolbar::HitTest (int x, int y) const
{
    return IsPointInRect (m_barRect, x, y) ||
           (m_flyoutOpen && IsPointInRect (m_flyoutRc, x, y));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::GetStatusCoreColor
//
//  PrinterStatus -> LED core color (same mapping the standalone indicator
//  used, so the light keeps its meaning across the move into the toolbar).
//
////////////////////////////////////////////////////////////////////////////////

uint32_t CommandToolbar::GetStatusCoreColor (PrinterStatus status)
{
    // Event-only light: no LED at all while idle (no light = no problem), and
    // the lit states run bright -- dim colors disappear against the themed
    // strip. 0 == unlit, which is why Idle keeps the initializer.
    uint32_t  core = 0;



    switch (status)
    {
    case PrinterStatus::Receiving: core = 0xFF4CE96A; break;   // bright green: printing now
    case PrinterStatus::Pending:   core = 0xFFFFB938; break;   // bright amber: page waiting
    case PrinterStatus::Error:     core = 0xFFFF5257; break;   // bright red:   failed
    case PrinterStatus::Idle:
    default:                                          break;   // off: powered + idle
    }

    return core;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintStatusLed
//
//  A small status-light dot riding the printer glyph's corner (halo + core in
//  the PrinterStatus color) -- the monoline glyph keeps the icon set uniform
//  while the LED keeps the at-a-glance printer state. core == 0 means unlit
//  (idle): paint nothing at all.
//
////////////////////////////////////////////////////////////////////////////////

static void PaintStatusLed (IDxuiPainter & painter, float cx, float cy, UINT dpi, uint32_t core)
{
    float     r    = 2.0f * (float) dpi / (float) s_kBaseDpi;
    uint32_t  halo = (core & 0x00FFFFFFu) | 0x80000000u;



    // core == 0 is the idle state: paint nothing at all, so an idle printer
    // shows no light rather than a dark dot.
    if (core != 0)
    {
        painter.FillCircleApprox (cx, cy, r * 1.8f, halo);
        painter.FillCircleApprox (cx, cy, r,        core);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::SetInputState / InputSegSelected
//
//  The joystick segment lights from the arrows mapping, paddle / mouse from
//  the pointer mode, and the mouse segment exists only when the machine has
//  a mouse. The keep-alive rect is the union of the volume button and its
//  flyout, so the pointer can travel between them across the bar's margin
//  without the flyout closing.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::SetInputState (bool arrowsJoystick, InputMappingMode pointer, bool mouseAvailable)
{
    m_arrowsJoystick = arrowsJoystick;
    m_pointerMode    = pointer;
    m_mouseAvailable = mouseAvailable;
}


bool CommandToolbar::InputSegSelected (int index) const
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
//  CommandToolbar::FlyoutKeepAliveRc
//
////////////////////////////////////////////////////////////////////////////////

RECT CommandToolbar::FlyoutKeepAliveRc() const
{
    RECT  rc = m_muteButton.rc;



    rc.left   = (std::min) (rc.left,   m_flyoutRc.left);
    rc.right  = (std::max) (rc.right,  m_flyoutRc.right);
    rc.bottom = (std::max) (rc.bottom, m_flyoutRc.bottom);

    return rc;
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
    UINT   dpi        = (scaler.GetDpi() == 0) ? (UINT) s_kBaseDpi : scaler.GetDpi();
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
        float    w         = 0.0f;
        float    h         = 0.0f;
        HRESULT  hrMeasure = E_FAIL;

        if (m_textRenderer != nullptr)
        {
            hrMeasure = m_textRenderer->MeasureString (label, fontDip, s_kFontFamily, w, h);
        }

        if (SUCCEEDED (hrMeasure) && w > 0.0f)
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

    int  segW      = MulDiv (s_kSegPadXDp * 2 + s_kSegLedDp + s_kSegLedGapDp + s_kSegIconDp,
                             (int) dpi, s_kBaseDpi);
    int  segGap    = MulDiv (s_kSegGapDp,       (int) dpi, s_kBaseDpi);
    int  labelGap  = MulDiv (s_kInputLabelGapDp, (int) dpi, s_kBaseDpi);

    // The input cluster: the shared label (dropped in icon-only mode, like
    // every other label) + LED/glyph segments.
    auto  clusterWidth = [&] (Mode mode) -> int
    {
        int  w = InputSegCount() * segW + (InputSegCount() - 1) * segGap;

        if (mode != Mode::IconOnly)
        {
            w += measure (s_kInputLabel, s_kFontDip * (float) dpi / (float) s_kBaseDpi) + 3 + labelGap;
        }

        return w;
    };

    // Total width a mode wants: the command buttons, the volume button (its
    // slider lives in the flyout now, not the bar), and the input cluster,
    // with group gaps between the four clusters.
    auto  totalWidth = [&] (Mode mode) -> int
    {
        int  w = 0;

        for (const Button & b : m_buttons) { w += buttonWidth (b, mode) + btnGap; }
        w += buttonWidth (m_muteButton, mode);
        w += clusterWidth (mode);
        w += (groupGap - btnGap) + groupGap + groupGap + groupGap;
        return w;
    };

    (void) sliderW;

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
    UINT   dpi        = 0;
    int    padX       = 0;
    int    padXStack  = 0;
    int    marginY    = 0;
    int    btnGap     = 0;
    int    groupGap   = 0;
    int    iconGap    = 0;
    int    sliderW    = 0;
    int    sliderMinW = 0;
    int    sliderMaxH = 0;
    float  iconDip    = 0.0f;
    int    barPad     = 0;
    int    avail      = 0;
    int    x          = 0;
    int    top        = 0;
    int    bottom     = 0;



    PlanForWidth (boundsDip.right - boundsDip.left, scaler);

    dpi = (scaler.GetDpi() == 0) ? (UINT) s_kBaseDpi : scaler.GetDpi();
    padX = MulDiv (s_kBtnPadXDp,     (int) dpi, s_kBaseDpi);
    padXStack = MulDiv (s_kStackedPadXDp, (int) dpi, s_kBaseDpi);
    marginY = MulDiv (s_kBtnMarginYDp,  (int) dpi, s_kBaseDpi);
    btnGap = MulDiv (s_kBtnGapDp,      (int) dpi, s_kBaseDpi);
    groupGap = MulDiv (s_kGroupGapDp,    (int) dpi, s_kBaseDpi);
    iconGap = MulDiv (s_kIconGapDp,     (int) dpi, s_kBaseDpi);
    sliderW = MulDiv (s_kSliderWidthDp, (int) dpi, s_kBaseDpi);
    sliderMinW = MulDiv (s_kSliderMinWidthDp, (int) dpi, s_kBaseDpi);
    sliderMaxH = MulDiv (s_kSliderMaxHDp,  (int) dpi, s_kBaseDpi);
    iconDip = s_kIconDip * (float) dpi / (float) s_kBaseDpi;
    barPad = MulDiv (s_kBarPadXDp, (int) dpi, s_kBaseDpi);
    avail = (boundsDip.right - boundsDip.left) - barPad * 2;
    x = boundsDip.left + barPad;
    top = boundsDip.top + marginY;
    bottom = boundsDip.bottom - marginY;

    m_dpi     = dpi;
    m_barRect = boundsDip;

    auto  measure = [&] (const wchar_t * label, float fontDip) -> int
    {
        float    w         = 0.0f;
        float    h         = 0.0f;
        HRESULT  hrMeasure = E_FAIL;

        if (m_textRenderer != nullptr)
        {
            hrMeasure = m_textRenderer->MeasureString (label, fontDip, s_kFontFamily, w, h);
        }

        if (SUCCEEDED (hrMeasure) && w > 0.0f)
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

    (void) sliderW;
    (void) sliderMinW;
    (void) sliderMaxH;

    auto  place = [&] (Button & btn)
    {
        int  w = buttonWidth (btn);

        btn.rc = RECT { x, top, x + w, bottom };
        x     += w + btnGap;
    };

    place (m_buttons[0]);                       // Settings
    place (m_buttons[1]);                       // Printer
    x += groupGap - btnGap;

    place (m_muteButton);                       // Volume (opens the flyout)
    x += groupGap - btnGap;

    // The flyout hangs under the volume button, left-aligned to it; the
    // slider fills it inside the padding, readout under the track.
    {
        int   flyW   = MulDiv (s_kFlyoutWidthDp,  (int) dpi, s_kBaseDpi);
        int   flyH   = MulDiv (s_kFlyoutHeightDp, (int) dpi, s_kBaseDpi);
        int   flyPad = MulDiv (s_kFlyoutPadDp,    (int) dpi, s_kBaseDpi);
        int   drop   = MulDiv (s_kFlyoutDropDp,   (int) dpi, s_kBaseDpi);
        int   fx     = m_muteButton.rc.left +
                       ((m_muteButton.rc.right - m_muteButton.rc.left) - flyW) / 2;

        fx         = (std::max) (fx, (int) m_barRect.left);
        m_flyoutRc = RECT { fx, m_barRect.bottom + drop,
                            fx + flyW, m_barRect.bottom + drop + flyH };

        m_volumeSlider.SetRect (RECT { m_flyoutRc.left + flyPad,  m_flyoutRc.top + flyPad,
                                       m_flyoutRc.right - flyPad, m_flyoutRc.bottom - flyPad });
        m_volumeSlider.SetDpi  (dpi);
    }

    // Input cluster: the shared label, then LED + glyph segments.
    {
        int  segW     = MulDiv (s_kSegPadXDp * 2 + s_kSegLedDp + s_kSegLedGapDp + s_kSegIconDp,
                                (int) dpi, s_kBaseDpi);
        int  segGap   = MulDiv (s_kSegGapDp,        (int) dpi, s_kBaseDpi);
        int  labelGap = MulDiv (s_kInputLabelGapDp, (int) dpi, s_kBaseDpi);

        m_inputLabelRc = {};

        if (m_mode != Mode::IconOnly)
        {
            // +3px slack over the measured width: DrawString wraps on a rect
            // even fractionally narrower than the layout width it measured.
            int  labelW = measure (s_kInputLabel, s_kFontDip * (float) dpi / (float) s_kBaseDpi) + 3;

            m_inputLabelRc = RECT { x, top, x + labelW, bottom };
            x += labelW + labelGap;
        }

        for (int i = 0; i < 3; i++)
        {
            if (i < InputSegCount())
            {
                m_inputSegs[i].rc = RECT { x, top, x + segW, bottom };
                x += segW + ((i + 1 < InputSegCount()) ? segGap : 0);
            }
            else
            {
                m_inputSegs[i].rc = {};
            }
        }

        x += groupGap;
    }

    place (m_buttons[2]);                       // Screenshot
    place (m_buttons[3]);                       // Reset
    place (m_buttons[4]);                       // Power

    SetBounds (m_barRect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::GetTooltipAt
//
//  Icon-only mode has no labels, so the hovered button's meaning surfaces as
//  a tooltip (the shell owns the DxuiTooltip and its dwell timing). The mute
//  button's tooltip reflects the action it would take.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CommandToolbar::GetTooltipAt (int x, int y, RECT & anchor) const
{
    const wchar_t *  tip = nullptr;



    // The input segments carry no labels in ANY mode -- the cluster's shared
    // label only names the group -- so their tooltips always show and lead
    // with the mode name.
    {
        static constexpr const wchar_t * s_kSegTips[3] =
            { s_kTipJoystickSeg, s_kTipPaddleSeg, s_kTipMouseSeg };

        for (int i = 0; i < InputSegCount(); i++)
        {
            if (tip == nullptr && IsPointInRect (m_inputSegs[i].rc, x, y))
            {
                anchor = m_inputSegs[i].rc;
                tip    = s_kSegTips[i];
            }
        }
    }

    // Outside icon-only mode the button labels are visible, so a tooltip
    // would just repeat them. Null means "no tip"; `anchor` is left alone.
    if (tip == nullptr && m_mode == Mode::IconOnly)
    {
        for (const Button & btn : m_buttons)
        {
            if (tip == nullptr && btn.enabled && IsPointInRect (btn.rc, x, y))
            {
                anchor = btn.rc;
                tip    = btn.label;
            }
        }

        // The mute button's tip names the action it would take, not its state.
        if (tip == nullptr && IsPointInRect (m_muteButton.rc, x, y))
        {
            anchor = m_muteButton.rc;
            tip    = m_muted ? L"Unmute" : L"Mute";
        }
    }

    return tip;
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
    bool  over = false;



    UNREFERENCED_PARAMETER (leftDown);


    if (m_flyoutOpen && m_volumeSlider.OnMouseMove (x, y))
    {
        over = true;
    }

    if (m_flyoutOpen)
    {
        m_volumeSlider.SetMouseHover (x, y);
    }

    for (Button & btn : m_buttons)
    {
        btn.hovered = btn.enabled && IsPointInRect (btn.rc, x, y);
        if (!btn.hovered) { btn.pressed = false; }
        over = over || btn.hovered;
    }

    m_muteButton.hovered = IsPointInRect (m_muteButton.rc, x, y);
    if (!m_muteButton.hovered) { m_muteButton.pressed = false; }

    for (int i = 0; i < InputSegCount(); i++)
    {
        m_inputSegs[i].hovered = IsPointInRect (m_inputSegs[i].rc, x, y);
        if (!m_inputSegs[i].hovered) { m_inputSegs[i].pressed = false; }
        over = over || m_inputSegs[i].hovered;
    }

    // The flyout opens on hover over the volume button and stays while the
    // pointer remains in the button-flyout corridor -- the union rect, so
    // the travel across the bar's bottom margin cannot close it. A drag in
    // progress pins it open regardless (the pointer may leave the track).
    if (m_muteButton.hovered)
    {
        m_flyoutOpen = true;
    }
    else if (m_flyoutOpen && !m_volumeSlider.IsDragging() &&
             !IsPointInRect (FlyoutKeepAliveRc(), x, y))
    {
        m_flyoutOpen = false;
    }

    return over || m_muteButton.hovered ||
           (m_flyoutOpen && IsPointInRect (m_flyoutRc, x, y)) ||
           IsPointInRect (m_barRect, x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::OnToolbarMouseLeave
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::OnToolbarMouseLeave()
{
    for (Button & btn : m_buttons)
    {
        btn.hovered = false;
        btn.pressed = false;
    }

    m_muteButton.hovered = false;
    m_muteButton.pressed = false;

    for (InputSeg & seg : m_inputSegs)
    {
        seg.hovered = false;
        seg.pressed = false;
    }

    // The pointer left the window entirely; a drag can survive that (the
    // shell keeps forwarding while captured), so only close when idle.
    if (!m_volumeSlider.IsDragging())
    {
        m_flyoutOpen = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::OnToolbarLButtonDown
//
//  Press handling: arm a button, start a slider drag, or eat the click.
//
//  The volume slider gets first claim, but only while UNMUTED. A muted slider
//  is inert, so a press there should fall through to the bar rather than
//  starting a drag that changes a value nobody can hear.
//
//  A press only ARMS a button; the command fires on release. That is what
//  makes press-then-drag-off cancel, the behavior every Windows button has.
//
//  A press on the bar's DEAD SPACE is still consumed. The toolbar sits over
//  the emulator viewport, so an unclaimed click would otherwise reach the
//  guest -- clicking the empty part of a toolbar must not type into the //e or
//  move the guest mouse.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::OnToolbarLButtonDown (int x, int y)
{
    // The flyout's slider gets first claim while open and unmuted -- a muted
    // slider is inert and the press should fall through.
    bool  handled = m_flyoutOpen && !m_muted && m_volumeSlider.OnLButtonDown (x, y);



    for (Button & btn : m_buttons)
    {
        if (!handled && btn.enabled && IsPointInRect (btn.rc, x, y))
        {
            btn.pressed = true;
            handled     = true;
        }
    }

    if (!handled && IsPointInRect (m_muteButton.rc, x, y))
    {
        m_muteButton.pressed = true;
        handled              = true;
    }

    for (int i = 0; !handled && i < InputSegCount(); i++)
    {
        if (IsPointInRect (m_inputSegs[i].rc, x, y))
        {
            m_inputSegs[i].pressed = true;
            handled                = true;
        }
    }

    // Clicks on the bar's dead space -- and the open flyout's -- are eaten
    // so they do not fall through to whatever is behind.
    return handled || IsPointInRect (m_barRect, x, y) ||
           (m_flyoutOpen && IsPointInRect (m_flyoutRc, x, y));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::OnToolbarLButtonUp
//
//  Release handling: fire the command for a completed click, and clear every
//  pressed visual.
//
//  The loop clears EVERY button's pressed state regardless of where the
//  release landed, because a press that ends elsewhere is a cancel and must
//  leave nothing stuck down. Only a press and release on the SAME button fires
//  its command.
//
//  Mute is handled locally rather than dispatched as a command, because it
//  owns state the slider reads back -- routing it through the command path
//  would put the toolbar's own model a round trip behind its own control.
//
//  Like the press, a release on dead space is consumed so it cannot reach the
//  emulator viewport underneath.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::OnToolbarLButtonUp (int x, int y)
{
    bool  handled    = m_volumeSlider.OnLButtonUp (x, y);
    bool  wasPressed = false;



    // Every button drops its pressed visual on any release, whether or not
    // the release landed on it -- a press that ends elsewhere is a cancel.
    // Only a press-and-release on the SAME button fires its command.
    for (Button & btn : m_buttons)
    {
        wasPressed  = btn.pressed;
        btn.pressed = false;

        if (!handled && wasPressed && btn.enabled && IsPointInRect (btn.rc, x, y))
        {
            if (m_dispatch) { m_dispatch (btn.id); }
            handled = true;
        }
    }

    wasPressed           = m_muteButton.pressed;
    m_muteButton.pressed = false;

    // Mute is handled locally rather than dispatched: it owns the state the
    // slider reads back.
    if (!handled && wasPressed && IsPointInRect (m_muteButton.rc, x, y))
    {
        SetVolume (m_volume01, !m_muted);

        if (m_volumeSink) { m_volumeSink (m_volume01, m_muted); }

        handled = true;
    }

    // Input segments: press-and-release on the same segment toggles its mode.
    {
        static constexpr InputMappingMode s_kSegModes[3] =
            { InputMappingMode::Joystick, InputMappingMode::Paddle, InputMappingMode::Mouse };

        for (int i = 0; i < 3; i++)
        {
            bool  segWasPressed = m_inputSegs[i].pressed;

            m_inputSegs[i].pressed = false;

            if (!handled && segWasPressed && i < InputSegCount() &&
                IsPointInRect (m_inputSegs[i].rc, x, y))
            {
                if (m_inputSink) { m_inputSink (s_kSegModes[i]); }
                handled = true;
            }
        }
    }

    return handled || IsPointInRect (m_barRect, x, y) ||
           (m_flyoutOpen && IsPointInRect (m_flyoutRc, x, y));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PaintButton
//
//  Draws one toolbar button in whichever of the three display modes is active:
//  icon only, icon with the label beside it, or ribbon-style with the label
//  below.
//
//  Background chrome is drawn ONLY when hovered or pressed. An idle toolbar
//  shows bare icons on the bar, which is what keeps a row of eight buttons
//  from reading as eight boxes.
//
//  Disabled buttons dim the ink by rewriting its ALPHA rather than
//  substituting a theme color, so the disabled look follows whatever the
//  theme's foreground is instead of needing a matching swatch per theme.
//
//  Icon-only and label-right share one draw call and differ only in the x
//  offset -- centered versus left-padded -- because the icon itself is
//  identical in both. Only the ribbon mode needs its own path, since it splits
//  the button vertically into an icon region and a label row.
//
//  The status LED is positioned relative to the ICON, not the button, so it
//  stays pinned to the glyph's corner regardless of how much label space the
//  current mode leaves around it.
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
    wchar_t   glyph[2] = { btn.glyph, 0 };



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

    // Ribbon mode: icon centered over a small label. The icon gets the region
    // above the label row; the label spans the button width, centered.
    if (m_mode == Mode::LabelBelow)
    {
        float  stackedDip = s_kStackedFontDip * (float) m_dpi / (float) s_kBaseDpi;
        float  labelH     = stackedDip + 4.0f;
        float  iconRegH   = bh - labelH;

        hr = text.DrawString (glyph, bl, bt, bw, iconRegH,
                              ink, iconDip, s_kIconFamily,
                              DxuiTextRenderer::HAlign::Center,
                              DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (btn.statusLed)
        {
            PaintStatusLed (painter, bl + bw * 0.5f + iconDip * 0.62f,
                            bt + (iconRegH - iconDip) * 0.5f + 2.0f, m_dpi,
                            GetStatusCoreColor (m_printerStatus));
        }

        hr = text.DrawString (btn.label, bl, bt + iconRegH - 2.0f, bw, labelH,
                              ink, stackedDip, s_kFontFamily,
                              DxuiTextRenderer::HAlign::Center,
                              DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
    else
    {
        // Icon-only centers the icon; LabelRight keeps it left-padded with the
        // label beside it. Both draw the icon the same way, so only the x
        // differs.
        float  iconX = (m_mode == Mode::IconOnly) ? bl + (bw - iconDip) * 0.5f
                                                  : bl + (float) padX;

        hr = text.DrawString (glyph, iconX, bt, iconDip + 2.0f, bh,
                              ink, iconDip, s_kIconFamily,
                              DxuiTextRenderer::HAlign::Left,
                              DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (btn.statusLed)
        {
            PaintStatusLed (painter, iconX + iconDip + 1.0f,
                            bt + bh * 0.5f - iconDip * 0.48f, m_dpi,
                            GetStatusCoreColor (m_printerStatus));
        }

        // Icon-only draws no label at all -- tooltips carry them (GetTooltipAt).
        if (m_mode != Mode::IconOnly)
        {
            textX = bl + (float) padX + iconW + (float) iconGap;

            hr = text.DrawString (btn.label, textX, bt,
                                  (float) btn.rc.right - textX, bh,
                                  ink, fontDip, s_kFontFamily,
                                  DxuiTextRenderer::HAlign::Left,
                                  DxuiTextRenderer::VAlign::CenterOnCapHeight);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
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
    const CassoTheme &  theme = static_cast<const CassoTheme &> (dxuiTheme);
    float               bl    = 0.0f;
    float               btTop = 0.0f;
    float               bw    = 0.0f;
    float               bhAll = 0.0f;



    _ASSERTE (dynamic_cast<const CassoTheme *> (&dxuiTheme) != nullptr);

    bl = (float) m_barRect.left;
    btTop = (float) m_barRect.top;
    bw = (float) (m_barRect.right - m_barRect.left);
    bhAll = (float) (m_barRect.bottom - m_barRect.top);

    if (bw <= 0.0f)
    {
        return;
    }

    // The strip continues the menu bar's themed surface (navStrip), so the
    // two chrome rows above the emulator read as one block; a hairline
    // separates the toolbar from the viewport below.
    painter.FillRect (bl, btTop, bw, bhAll, theme.navStrip);
    painter.FillRect (bl, (float) m_barRect.bottom - 1.0f, bw, 1.0f, theme.buttonBorder);

    // The printer button follows card presence: no card, no printer button.
    m_buttons[1].enabled = m_printerPresent;

    for (Button & btn : m_buttons)
    {
        PaintButton (btn, painter, text, theme);
    }

    PaintButton (m_muteButton, painter, text, theme);
    PaintInputCluster (painter, text, theme);

    // The flyout paints LAST: it hangs below the bar over whatever chrome or
    // scene is there, and everything on the bar must be under it.
    if (m_flyoutOpen)
    {
        PaintVolumeFlyout (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PaintInputCluster
//
//  The shared "Input" label (muted ink -- it names the group, it is not a
//  button) and the LED + glyph segments. Hover chrome matches the buttons';
//  the LED is the state: an outline would read as focus, a lit LED reads as
//  ON. The core tracks the theme's LED tokens, so the dot matches the drive
//  widgets' lights under every preset.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintInputCluster (IDxuiPainter & painter, IDxuiTextRenderer & text,
                                        const CassoTheme & theme)
{
    HRESULT   hr       = S_OK;
    float     fontDip  = s_kFontDip * (float) m_dpi / (float) s_kBaseDpi;
    int       ledD     = MulDiv (s_kSegLedDp,    (int) m_dpi, s_kBaseDpi);
    int       ledGap   = MulDiv (s_kSegLedGapDp, (int) m_dpi, s_kBaseDpi);
    int       segPad   = MulDiv (s_kSegPadXDp,   (int) m_dpi, s_kBaseDpi);
    int       iconD    = MulDiv (s_kSegIconDp,   (int) m_dpi, s_kBaseDpi);
    uint32_t  labelInk = theme.navItemText;   // same ink as the button labels



    if (m_inputLabelRc.right > m_inputLabelRc.left)
    {
        hr = text.DrawString (s_kInputLabel,
                              (float) m_inputLabelRc.left,
                              (float) m_inputLabelRc.top,
                              (float) (m_inputLabelRc.right  - m_inputLabelRc.left),
                              (float) (m_inputLabelRc.bottom - m_inputLabelRc.top),
                              labelInk, fontDip, s_kFontFamily,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (int i = 0; i < InputSegCount(); i++)
    {
        const InputSeg &  seg    = m_inputSegs[i];
        bool              active = seg.hovered || seg.pressed;
        float             sl     = (float) seg.rc.left;
        float             st     = (float) seg.rc.top;
        float             sw     = (float) (seg.rc.right  - seg.rc.left);
        float             sh     = (float) (seg.rc.bottom - seg.rc.top);



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
            bool   on    = InputSegSelected (i);

            painter.FillCircleApprox (ledCx, ledCy, (float) ledD * 0.5f,
                                      on ? theme.ledActive : theme.ledIdle);
        }

        {
            int   boxL = seg.rc.left + segPad + ledD + ledGap;
            int   boxT = seg.rc.top + ((seg.rc.bottom - seg.rc.top) - iconD) / 2;
            RECT  box  = { boxL, boxT, boxL + iconD, boxT + iconD };

            if (m_inputMonoline)
            {
                switch (i)
                {
                    case 0:  PaintJoystickMono (painter, box, theme.navItemText); break;
                    case 1:  PaintPaddleMono   (painter, box, theme.navItemText); break;
                    case 2:  PaintMouseMono    (painter, box, theme.navItemText); break;
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
                    case 0:  InputDeviceGlyphs::PaintJoystickGlyph (painter, box, m_inputSkeuo); break;
                    case 1:  InputDeviceGlyphs::PaintPaddleGlyph   (painter, box, m_inputSkeuo); break;
                    case 2:  InputDeviceGlyphs::PaintMouseGlyph    (painter, box, m_inputSkeuo); break;
                    default: break;
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::StrokeCircle + the monoline glyph painters
//
//  Monoline device glyphs in the Segoe MDL2 language the bar's other icons
//  speak: uniform stroke, dots for controls, no shading. Drawn rather than
//  taken from the font because MDL2 has no paddle, and one hand-drawn glyph
//  next to two font glyphs would mismatch stroke weight -- so all three are
//  drawn with the same pen. Geometry is in box fractions, so the set scales
//  together.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::StrokeCircle (IDxuiPainter & painter, float cx, float cy,
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
//  CommandToolbar::PaintJoystickMono
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintJoystickMono (IDxuiPainter & painter, const RECT & box, uint32_t ink)
{
    float  w         = (float) (box.right  - box.left);
    float  h         = (float) (box.bottom - box.top);
    float  stroke    = (std::max) (1.0f, w / 26.0f);
    float  cx        = (float) box.left + w * 0.5f;
    float  capHalf   = w * 0.085f;
    float  bodyHalf  = w * 0.155f;
    float  waistHalf = w * 0.045f;
    float  capY      = (float) box.top + h * 0.08f;
    float  shoulderY = (float) box.top + h * 0.16f;
    float  bodyY     = (float) box.top + h * 0.30f;
    float  waistY    = (float) box.top + h * 0.52f;
    float  baseT     = (float) box.top + h * 0.62f;
    float  baseH     = h * 0.26f;
    float  baseHalf  = w * 0.32f;



    // The handle's half-profile, top to bottom, outline only and symmetric
    // about the centerline: flat cap; shoulder angling out; the nearly-
    // cylindrical upper body; the slow taper in to the narrow waist; the
    // straight shaft down to the base; the base slab's border.
    painter.DrawLineApprox (cx - capHalf, capY, cx + capHalf, capY, stroke, ink);

    painter.DrawLineApprox (cx - capHalf,  capY,      cx - bodyHalf,  shoulderY, stroke, ink);
    painter.DrawLineApprox (cx + capHalf,  capY,      cx + bodyHalf,  shoulderY, stroke, ink);
    painter.DrawLineApprox (cx - bodyHalf, shoulderY, cx - bodyHalf,  bodyY,     stroke, ink);
    painter.DrawLineApprox (cx + bodyHalf, shoulderY, cx + bodyHalf,  bodyY,     stroke, ink);
    painter.DrawLineApprox (cx - bodyHalf, bodyY,     cx - waistHalf, waistY,    stroke, ink);
    painter.DrawLineApprox (cx + bodyHalf, bodyY,     cx + waistHalf, waistY,    stroke, ink);
    painter.DrawLineApprox (cx - waistHalf, waistY,   cx + waistHalf, waistY,    stroke, ink);

    painter.DrawLineApprox (cx, waistY, cx, baseT, stroke, ink);
    painter.OutlineRect    (cx - baseHalf, baseT, baseHalf * 2.0f, baseH, stroke, ink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PaintPaddleMono
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintPaddleMono (IDxuiPainter & painter, const RECT & box, uint32_t ink)
{
    constexpr int  s_kArcSegments = 16;



    float  w       = (float) (box.right  - box.left);
    float  h       = (float) (box.bottom - box.top);
    float  stroke  = (std::max) (1.0f, w / 26.0f);
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





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PaintMouseMono
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintMouseMono (IDxuiPainter & painter, const RECT & box, uint32_t ink)
{
    float  w      = (float) (box.right  - box.left);
    float  h      = (float) (box.bottom - box.top);
    float  stroke = (std::max) (1.0f, w / 26.0f);
    float  bodyL  = (float) box.left + w * 0.30f;
    float  bodyT  = (float) box.top + h * 0.20f;
    float  bodyW  = w * 0.40f;
    float  bodyH  = h * 0.62f;



    // The M0100 in outline: the box body's border, the single wide button's
    // split a third down, and the cable stub off the top.
    painter.OutlineRect    (bodyL, bodyT, bodyW, bodyH, stroke, ink);
    painter.DrawLineApprox (bodyL, bodyT + bodyH * 0.32f,
                            bodyL + bodyW, bodyT + bodyH * 0.32f, stroke, ink);
    painter.DrawLineApprox (bodyL + bodyW * 0.5f, bodyT,
                            bodyL + bodyW * 0.5f, bodyT - h * 0.10f, stroke, ink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PaintVolumeFlyout
//
//  A small panel hanging under the volume button: themed surface, hairline
//  border, and the vertical slider with its % readout under the track. The
//  panel background must be OPAQUE -- it floats over the live scene, and a
//  translucent flyout would read as a rendering artifact.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintVolumeFlyout (IDxuiPainter & painter, IDxuiTextRenderer & text,
                                        const CassoTheme & theme)
{
    float  fl = (float) m_flyoutRc.left;
    float  ft = (float) m_flyoutRc.top;
    float  fw = (float) (m_flyoutRc.right  - m_flyoutRc.left);
    float  fh = (float) (m_flyoutRc.bottom - m_flyoutRc.top);



    painter.FillRect (fl - 1.0f, ft - 1.0f, fw + 2.0f, fh + 2.0f, theme.buttonBorder);
    painter.FillRect (fl, ft, fw, fh, theme.navStrip);

    m_volumeSlider.SetEnabled (!m_muted);
    m_volumeSlider.Paint (painter, text, theme);
}

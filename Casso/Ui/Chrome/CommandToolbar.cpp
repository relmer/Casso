#include "Pch.h"
#include "Theme/DxuiColor.h"
#include "Theme/DxuiTheme.h"

#include "CassoTheme.h"
#include "CommandToolbar.h"
#include "Core/UnicodeSymbols.h"
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
static constexpr int      s_kBandDp         = 42;   // strip thickness
static constexpr float    s_kIconDip        = 15.0f;
static constexpr float    s_kFontDip        = 13.0f;
static constexpr float    s_kFallbackCharPx = 7.5f;

static constexpr const wchar_t * s_kFontFamily = DxuiTheme::kBodyFace;
static constexpr const wchar_t * s_kIconFamily = L"Segoe MDL2 Assets";

// Segoe MDL2 Assets codepoints.
static constexpr wchar_t  s_kGlyphSettings   = L'\uE713';   // gear
static constexpr wchar_t  s_kGlyphTheme      = L'\uE746';   // half-filled square: light / dark
static constexpr wchar_t  s_kGlyphScreenshot = L'\uE722';   // camera
static constexpr wchar_t  s_kGlyphReset      = L'\uE72C';   // refresh arrow
static constexpr wchar_t  s_kGlyphPower      = L'\uE7E8';   // power symbol
static constexpr wchar_t  s_kGlyphVolume     = L'\uE767';   // speaker
static constexpr wchar_t  s_kGlyphMuted      = L'\uE74F';   // muted speaker
static constexpr wchar_t  s_kGlyphPrint      = L'\uE749';   // printer (monoline, matches the set)
static constexpr wchar_t  s_kGlyphColor      = L'\uE790';   // artist's palette
static constexpr wchar_t  s_kGlyphFullscreen = L'\uE740';   // diagonal arrows, outward
static constexpr wchar_t  s_kGlyphRestore    = L'\uE73F';   // diagonal arrows, inward
static constexpr wchar_t  s_kGlyphMouse      = L'\uE962';   // mouse (the input cluster's one font glyph)

// Volume flyout (vertical slider + readout under the track).
static constexpr int      s_kFlyoutWidthDp    = 56;
static constexpr int      s_kFlyoutHeightDp   = 154;
static constexpr int      s_kFlyoutPadDp      = 8;
static constexpr int      s_kFlyoutDropDp     = 2;    // gap under the bar

// Input cluster: LED + glyph segments under one shared label. The glyph box
// is sized so its INK matches the MDL2 icons' (their 15 dip em draws about
// 15 dp of ink); a drawn glyph filling its box needs the smaller number.
static constexpr int      s_kSegIconDp        = 19;
static constexpr int      s_kSegPadXDp        = 5;
static constexpr int      s_kSegLedDp         = 7;    // LED diameter
static constexpr int      s_kSegLedGapDp      = 4;
static constexpr int      s_kSegGapDp         = 2;
static constexpr int      s_kInputLabelGapDp  = 8;    // label -> first segment

static constexpr const wchar_t * s_kInputLabel = L"Input";

// Contrast an unlit segment LED keeps against the strip -- the ratio
// DxuiTreeView tints a locked checkbox's fill with, so the two read alike.
static constexpr float    s_kOffLedContrast   = 1.6f;

static constexpr const wchar_t * s_kTipInput = L"Input devices";

// The monitor-color rows. Settings spells the monochrome ones out in full;
// on a strip this narrow the phosphor name alone carries it, and the button
// lights its screen in the color besides.
static constexpr const wchar_t * s_kMonitorColorRows[] =
{
    L"Color",
    L"Green",
    L"Amber",
    L"White",
};

// The input rows, worded as the segment tooltips name the modes.
static constexpr const wchar_t * s_kInputRows[] =
{
    L"Joystick (arrow keys)",
    L"Paddles (mouse)",
    L"Mouse",
};

static constexpr InputMappingMode s_kInputModes[3] =
{
    InputMappingMode::Joystick,
    InputMappingMode::Paddle,
    InputMappingMode::Mouse,
};

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
//  Fixed entry set, in strip order: Settings with the theme and monitor-color
//  pickers, Printer, the volume group, the input devices, then Fullscreen /
//  Screenshot / Reset / Power. Every command id is an existing IDM_* routed
//  through the menu's HandleCommand path.
//
//  The order is also the collapse order read backwards -- Power gives up its
//  label first, Settings last -- so it is the one place that decides both.
//
////////////////////////////////////////////////////////////////////////////////

CommandToolbar::CommandToolbar()
{
    m_focusable = false;

    m_buttons.resize ((size_t) Entry::Count);

    GetEntry (Entry::Settings)   = Button { Entry::Settings,   IDM_VIEW_SETTINGS,        s_kGlyphSettings,   L"Settings"   };
    GetEntry (Entry::Theme)      = Button { Entry::Theme,      0,                        s_kGlyphTheme,      L"Theme"      };
    GetEntry (Entry::Color)      = Button { Entry::Color,      0,                        s_kGlyphColor,      L"Color"      };
    GetEntry (Entry::Printer)    = Button { Entry::Printer,    IDM_PRINTER_PREVIEW,      s_kGlyphPrint,      L"Printer"    };
    GetEntry (Entry::Volume)     = Button { Entry::Volume,     0,                        s_kGlyphVolume,     L"Volume"     };
    GetEntry (Entry::Input)      = Button { Entry::Input,      0,                        0,                  s_kInputLabel };
    GetEntry (Entry::Fullscreen) = Button { Entry::Fullscreen, IDM_VIEW_FULLSCREEN,      s_kGlyphFullscreen, L"Full screen" };
    GetEntry (Entry::Screenshot) = Button { Entry::Screenshot, IDM_EDIT_COPY_SCREENSHOT, s_kGlyphScreenshot, L"Screenshot" };
    GetEntry (Entry::Reset)      = Button { Entry::Reset,      IDM_MACHINE_RESET,        s_kGlyphReset,      L"Reset"      };
    GetEntry (Entry::Power)      = Button { Entry::Power,      IDM_MACHINE_POWERCYCLE,   s_kGlyphPower,      L"Power"      };

    GetEntry (Entry::Printer).statusLed = true;

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

    WireMenus();
    RebuildActionTips();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::WireMenus
//
//  The two pickers run the same three-callback contract, which is what gives
//  them the Settings behavior: a highlight PREVIEWS (pointer or arrow key,
//  applied but not persisted), a pick COMMITS, and the close edge SETTLES.
//
//  DxuiPopupMenu hides BEFORE it reports a pick, and says on the way out
//  whether a pick is what closed it. That is the whole reason the settle can
//  be unconditional here: on a dismissal it puts the old value back, and on a
//  pick it stands aside for the commit that is about to arrive.
//
//  The input menu has no preview -- its rows are toggles, not a value -- so it
//  only needs the select callback.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::WireMenus()
{
    m_themeMenu.SetOnHighlightChange ([this] (int index)
    {
        m_themePreviewed = true;

        if (m_themePreview)
        {
            m_themePreview (index);
        }
    });

    m_themeMenu.SetOnSelect ([this] (int index)
    {
        m_themeIndex     = index;
        m_themePreviewed = false;

        if (m_themeCommit)
        {
            m_themeCommit (index);
        }
    });

    m_themeMenu.SetOnClosed ([this] (bool committed)
    {
        if (!committed && m_themePreviewed && m_themeIndex >= 0 && m_themePreview)
        {
            m_themePreview (m_themeIndex);
        }

        m_themePreviewed = false;
        m_menuClosedMs   = GetTickCount64();
    });

    m_colorMenu.SetOnHighlightChange ([this] (int index)
    {
        m_colorPreviewed = true;

        if (m_monitorPreview)
        {
            m_monitorPreview (index);
        }
    });

    m_colorMenu.SetOnSelect ([this] (int index)
    {
        m_colorIndex     = index;
        m_colorPreviewed = false;

        if (m_monitorCommit)
        {
            m_monitorCommit (index);
        }
    });

    m_colorMenu.SetOnClosed ([this] (bool committed)
    {
        if (!committed && m_colorPreviewed && m_monitorPreview)
        {
            m_monitorPreview (m_colorIndex);
        }

        m_colorPreviewed = false;
        m_menuClosedMs   = GetTickCount64();
    });

    m_inputMenu.SetOnSelect ([this] (int index)
    {
        if (m_inputSink && index >= 0 && index < InputSegCount())
        {
            m_inputSink (s_kInputModes[index]);
        }
    });

    m_inputMenu.SetOnClosed ([this] (bool committed)
    {
        UNREFERENCED_PARAMETER (committed);

        m_menuClosedMs = GetTickCount64();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::RebuildActionTips
//
//  Reset and Power say which machine they act on, so the tips are composed
//  rather than fixed. Reset's also carries the cold-boot chord, drawn with the
//  keycap symbol so it matches the key the reader is reaching for.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::RebuildActionTips()
{
    std::wstring  machine = m_machineName.empty() ? std::wstring (L"machine") : m_machineName;



    GetEntry (Entry::Reset).tip = L"Reset the " + machine + L". " +
                                  s_kpszOpenApple +
                                  L" open Apple (left Alt) + Reset to cold boot.";
    GetEntry (Entry::Power).tip = L"Power-cycle the " + machine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::SetMachineDisplayName
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::SetMachineDisplayName (const std::wstring & displayName)
{
    if (displayName != m_machineName)
    {
        m_machineName = displayName;
        RebuildActionTips();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::SetFullscreen
//
//  One button covers both directions, so the glyph and the label follow the
//  presentation the click would LEAVE, not the one it is in.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::SetFullscreen (bool fullscreen)
{
    Button &  btn = GetEntry (Entry::Fullscreen);



    m_fullscreen = fullscreen;
    btn.glyph    = m_fullscreen ? s_kGlyphRestore    : s_kGlyphFullscreen;
    btn.label    = m_fullscreen ? L"Exit full screen" : L"Full screen";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::SetThemes
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::SetThemes (const std::vector<std::wstring> & displayNames, int activeIndex)
{
    m_themeNames = displayNames;
    m_themeIndex = activeIndex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::SetThemeIndex / SetMonitorColorIndex
//
//  An OPEN menu is mid-preview and owns the value, so a sync from the shell is
//  dropped rather than fighting the highlight the user is moving -- and the
//  preview itself arrives back here as a shell sync.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::SetThemeIndex (int index)
{
    if (!m_themeMenu.IsVisible())
    {
        m_themeIndex = index;
    }
}


void CommandToolbar::SetMonitorColorIndex (int index)
{
    if (!m_colorMenu.IsVisible())
    {
        m_colorIndex = index;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::SetThemeSinks / SetMonitorSinks
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::SetThemeSinks (ChoiceFn preview, ChoiceFn commit)
{
    m_themePreview = std::move (preview);
    m_themeCommit  = std::move (commit);
}


void CommandToolbar::SetMonitorSinks (ChoiceFn preview, ChoiceFn commit)
{
    m_monitorPreview = std::move (preview);
    m_monitorCommit  = std::move (commit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::SetPopupHost
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::SetPopupHost (DxuiHwndSource * host)
{
    m_themeMenu.SetPopupHost (host);
    m_colorMenu.SetPopupHost (host);
    m_inputMenu.SetPopupHost (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::HideMenus / IsReopenSuppressed
//
//  A CLICK ON A PICKER BUTTON WHOSE MENU IS ALREADY UP MUST CLOSE IT, and
//  that is harder than it looks: the popup dismisses itself on a click
//  outside its own window, and whether that runs before or after the strip
//  sees the same click is not ours to decide. Either order leaves the menu
//  shut by the time the release arrives, so the release would cheerfully
//  open it again and the button would never appear to toggle.
//
//  So the release refuses to open a menu that closed a moment ago. The
//  window is short enough that a deliberate second click always lands
//  outside it, and it covers both orderings without either side having to
//  know about the other.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::HideMenus()
{
    m_themeMenu.Hide();
    m_colorMenu.Hide();
    m_inputMenu.Hide();
}


bool CommandToolbar::IsReopenSuppressed() const
{
    constexpr uint64_t  kGuardMs = 250;



    return GetTickCount64() - m_menuClosedMs < kGuardMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::IsMenuOpen / HandleKey
//
//  An open menu is modal in practice, so the shell hands it every keydown --
//  otherwise arrowing through the rows would also type into the guest.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::IsMenuOpen() const
{
    return m_themeMenu.IsVisible() || m_colorMenu.IsVisible() || m_inputMenu.IsVisible();
}


bool CommandToolbar::HandleKey (WPARAM vk)
{
    bool  handled = false;



    if (m_themeMenu.IsVisible())
    {
        handled = m_themeMenu.OnKey (vk);
    }
    else if (m_colorMenu.IsVisible())
    {
        handled = m_colorMenu.OnKey (vk);
    }
    else if (m_inputMenu.IsVisible())
    {
        handled = m_inputMenu.OnKey (vk);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::OpenMenuFor
//
//  Builds the rows for one picker and hangs its menu under that button. Every
//  row carries its checked state, which is how a collapsed picker still says
//  what it is set to.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::OpenMenuFor (Entry entry)
{
    std::vector<DxuiPopupMenu::Item>  items;
    const Button &                    btn   = GetEntry (entry);
    DxuiPopupMenu *                   menu  = nullptr;
    int                               i     = 0;



    if (m_textRenderer == nullptr)
    {
        return;
    }

    switch (entry)
    {
    case Entry::Theme:
        menu = &m_themeMenu;
        for (const std::wstring & name : m_themeNames)
        {
            items.push_back (DxuiPopupMenu::Item { name, i == m_themeIndex });
            i++;
        }

        break;

    case Entry::Color:
        menu = &m_colorMenu;
        for (const wchar_t * row : s_kMonitorColorRows)
        {
            items.push_back (DxuiPopupMenu::Item { row, i == m_colorIndex });
            i++;
        }

        break;

    case Entry::Input:
        menu = &m_inputMenu;
        for (i = 0; i < InputSegCount(); i++)
        {
            items.push_back (DxuiPopupMenu::Item { s_kInputRows[i], InputSegSelected (i) });
        }

        break;

    default:
        break;
    }

    if (menu != nullptr && !items.empty())
    {
        menu->Show (btn.rc.left, m_barRect.bottom, std::move (items), *m_textRenderer, m_hostClient);
    }
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
    GetEntry (Entry::Volume).glyph = m_muted ? s_kGlyphMuted : s_kGlyphVolume;
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
//  CommandToolbar::GetBandDp
//
////////////////////////////////////////////////////////////////////////////////

int CommandToolbar::GetBandDp() const
{
    return s_kBandDp;
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
//  A small status-light dot riding a glyph's corner (halo + core). core == 0
//  means unlit: paint nothing at all.
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
//  CommandToolbar::SetInputState
//
//  The mouse segment exists only when the machine has a mouse.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::SetInputState (bool arrowsJoystick, InputMappingMode pointer, bool mouseAvailable)
{
    bool  countChanged = (mouseAvailable != m_mouseAvailable);



    m_arrowsJoystick = arrowsJoystick;
    m_pointerMode    = pointer;
    m_mouseAvailable = mouseAvailable;

    // Whether the mouse exists decides HOW MANY segments there are, and the
    // segment rects belong to Layout -- so a state push that adds or drops the
    // mouse has to re-lay the entry or the new segment keeps the empty rect it
    // was left with and never paints. A machine switch does exactly that: it
    // reflows the chrome first and syncs this state after, which is how a //c
    // switched to at runtime showed the joystick and paddle but no mouse.
    // Re-laying here rather than fixing that one order keeps every caller --
    // the switch, the Hardware tab's mouse toggle -- from having to know.
    if (countChanged && m_barRect.right > m_barRect.left)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (m_dpi);
        Layout (m_barRect, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::InputSegSelected
//
//  The joystick segment lights from the arrows mapping, paddle and mouse from
//  the pointer mode.
//
////////////////////////////////////////////////////////////////////////////////

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
//  CommandToolbar::IsInputExpanded
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::IsInputExpanded() const
{
    return GetEntry (Entry::Input).labeled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::FlyoutKeepAliveRc
//
//  The union of the volume button and its flyout, so the pointer can travel
//  between them across the bar's margin without the flyout closing.
//
////////////////////////////////////////////////////////////////////////////////

RECT CommandToolbar::FlyoutKeepAliveRc() const
{
    RECT  rc = GetEntry (Entry::Volume).rc;



    rc.left   = (std::min) (rc.left,   m_flyoutRc.left);
    rc.right  = (std::max) (rc.right,  m_flyoutRc.right);
    rc.bottom = (std::max) (rc.bottom, m_flyoutRc.bottom);

    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::MeasureLabelPx
//
////////////////////////////////////////////////////////////////////////////////

int CommandToolbar::MeasureLabelPx (const wchar_t * text, float fontPx) const
{
    float    w         = 0.0f;
    float    h         = 0.0f;
    HRESULT  hrMeasure = E_FAIL;



    if (text == nullptr || text[0] == 0)
    {
        return 0;
    }

    if (m_textRenderer != nullptr)
    {
        hrMeasure = m_textRenderer->MeasureString (text, fontPx, s_kFontFamily, w, h);
    }

    if (SUCCEEDED (hrMeasure) && w > 0.0f)
    {
        return (int) (w + 0.5f);
    }

    return (int) ((float) wcslen (text) * s_kFallbackCharPx * fontPx / s_kFontDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::GetEntryWidthPx
//
//  One entry costs its icon plus padding, and its label when it can still
//  afford one. The input devices are the exception in both directions: their
//  full form is a shared label over a row of LED segments, and their
//  collapsed form is a single icon like everything else.
//
////////////////////////////////////////////////////////////////////////////////

int CommandToolbar::GetEntryWidthPx (const Button & btn, bool labeled, UINT dpi) const
{
    int    padX    = MulDiv (s_kBtnPadXDp, (int) dpi, s_kBaseDpi);
    int    iconGap = MulDiv (s_kIconGapDp, (int) dpi, s_kBaseDpi);
    float  fontPx  = s_kFontDip * (float) dpi / (float) s_kBaseDpi;
    int    iconW   = (int) (s_kIconDip * (float) dpi / (float) s_kBaseDpi + 0.5f);
    int    width   = 0;



    if (btn.entry == Entry::Input && labeled)
    {
        int  segW     = MulDiv (s_kSegPadXDp * 2 + s_kSegLedDp + s_kSegLedGapDp + s_kSegIconDp,
                                (int) dpi, s_kBaseDpi);
        int  segGap   = MulDiv (s_kSegGapDp,        (int) dpi, s_kBaseDpi);
        int  labelGap = MulDiv (s_kInputLabelGapDp, (int) dpi, s_kBaseDpi);

        // +3px slack over the measured width: DrawString wraps on a rect even
        // fractionally narrower than the layout width it measured.
        return MeasureLabelPx (s_kInputLabel, fontPx) + 3 + labelGap +
               InputSegCount() * segW + (InputSegCount() - 1) * segGap;
    }

    width = padX * 2 + iconW;

    if (labeled)
    {
        width += iconGap + MeasureLabelPx (btn.label, fontPx);
    }

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::GetTotalWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int CommandToolbar::GetTotalWidthPx (int labeledCount, UINT dpi) const
{
    int  barPad   = MulDiv (s_kBarPadXDp,  (int) dpi, s_kBaseDpi);
    int  btnGap   = MulDiv (s_kBtnGapDp,   (int) dpi, s_kBaseDpi);
    int  groupGap = MulDiv (s_kGroupGapDp, (int) dpi, s_kBaseDpi);
    int  width    = barPad * 2;
    int  index    = 0;



    for (const Button & btn : m_buttons)
    {
        width += GetEntryWidthPx (btn, index < labeledCount, dpi);

        if (index + 1 < (int) m_buttons.size())
        {
            width += btnGap;
        }

        if (btn.entry == Entry::Printer || btn.entry == Entry::Volume || btn.entry == Entry::Input)
        {
            width += groupGap - btnGap;
        }

        index++;
    }

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PlanForWidth
//
//  Drops one label at a time FROM THE RIGHT until the strip fits, so the
//  leftmost entries keep their names longest and nothing is ever pushed off
//  the end. The band thickness is fixed: everything stays on one row, which
//  is what makes a per-entry collapse legible in the first place.
//
//  Once every entry is down to its icon there are no moves left. That bar is
//  around 450 dp, so it takes a window narrower than anything the emulator
//  itself is usable in before the strip runs out of room again.
//
////////////////////////////////////////////////////////////////////////////////

int CommandToolbar::PlanForWidth (int clientWidthPx, const DxuiDpiScaler & scaler)
{
    UINT  dpi     = (scaler.GetDpi() == 0) ? (UINT) s_kBaseDpi : scaler.GetDpi();
    int   labeled = (int) m_buttons.size();



    while (labeled > 0 && GetTotalWidthPx (labeled, dpi) > clientWidthPx)
    {
        labeled--;
    }

    m_labeledCount = labeled;

    return s_kBandDp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::Layout
//
//  Places the entries left-to-right: [Settings] [Theme] [Monitor color]
//  [Printer] | [Volume] | [Input] | [Fullscreen] [Screenshot] [Reset] [Power].
//  The collapse is re-planned against this exact strip width so the plan and
//  the placement can never disagree.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    UINT  dpi      = 0;
    int   marginY  = 0;
    int   btnGap   = 0;
    int   groupGap = 0;
    int   barPad   = 0;
    int   x        = 0;
    int   top      = 0;
    int   bottom   = 0;
    int   index    = 0;



    PlanForWidth (boundsDip.right - boundsDip.left, scaler);

    dpi      = (scaler.GetDpi() == 0) ? (UINT) s_kBaseDpi : scaler.GetDpi();
    marginY  = MulDiv (s_kBtnMarginYDp, (int) dpi, s_kBaseDpi);
    btnGap   = MulDiv (s_kBtnGapDp,     (int) dpi, s_kBaseDpi);
    groupGap = MulDiv (s_kGroupGapDp,   (int) dpi, s_kBaseDpi);
    barPad   = MulDiv (s_kBarPadXDp,    (int) dpi, s_kBaseDpi);
    x        = boundsDip.left + barPad;
    top      = boundsDip.top + marginY;
    bottom   = boundsDip.bottom - marginY;

    m_dpi     = dpi;
    m_barRect = boundsDip;

    for (Button & btn : m_buttons)
    {
        int  width = 0;

        btn.labeled = index < m_labeledCount;
        width       = GetEntryWidthPx (btn, btn.labeled, dpi);
        btn.rc      = RECT { x, top, x + width, bottom };
        x          += width;

        if (index + 1 < (int) m_buttons.size())
        {
            x += btnGap;
        }

        if (btn.entry == Entry::Printer || btn.entry == Entry::Volume || btn.entry == Entry::Input)
        {
            x += groupGap - btnGap;
        }

        index++;
    }

    // The input segments live inside the input entry's own rect, so the
    // cluster travels with it instead of being placed a second time.
    {
        const Button &  input    = GetEntry (Entry::Input);
        int             segW     = MulDiv (s_kSegPadXDp * 2 + s_kSegLedDp + s_kSegLedGapDp + s_kSegIconDp,
                                           (int) dpi, s_kBaseDpi);
        int             segGap   = MulDiv (s_kSegGapDp,        (int) dpi, s_kBaseDpi);
        int             labelGap = MulDiv (s_kInputLabelGapDp, (int) dpi, s_kBaseDpi);
        int             ix       = input.rc.left;
        float           fontPx   = s_kFontDip * (float) dpi / (float) s_kBaseDpi;
        int             i        = 0;

        m_inputLabelRc = {};

        for (i = 0; i < 3; i++)
        {
            m_inputSegs[i].rc = {};
        }

        if (input.labeled)
        {
            int  labelW = MeasureLabelPx (s_kInputLabel, fontPx) + 3;

            m_inputLabelRc = RECT { ix, top, ix + labelW, bottom };
            ix += labelW + labelGap;

            for (i = 0; i < InputSegCount(); i++)
            {
                m_inputSegs[i].rc = RECT { ix, top, ix + segW, bottom };
                ix += segW + ((i + 1 < InputSegCount()) ? segGap : 0);
            }
        }
    }

    // The flyout hangs under the volume button, centered on it; the slider
    // fills it inside the padding, readout under the track.
    {
        const Button &  volume = GetEntry (Entry::Volume);
        int             flyW   = MulDiv (s_kFlyoutWidthDp,  (int) dpi, s_kBaseDpi);
        int             flyH   = MulDiv (s_kFlyoutHeightDp, (int) dpi, s_kBaseDpi);
        int             flyPad = MulDiv (s_kFlyoutPadDp,    (int) dpi, s_kBaseDpi);
        int             drop   = MulDiv (s_kFlyoutDropDp,   (int) dpi, s_kBaseDpi);
        int             fx     = volume.rc.left + ((volume.rc.right - volume.rc.left) - flyW) / 2;

        fx         = (std::max) (fx, (int) m_barRect.left);
        m_flyoutRc = RECT { fx, m_barRect.bottom + drop,
                            fx + flyW, m_barRect.bottom + drop + flyH };

        m_volumeSlider.SetRect (RECT { m_flyoutRc.left + flyPad,  m_flyoutRc.top + flyPad,
                                       m_flyoutRc.right - flyPad, m_flyoutRc.bottom - flyPad });
        m_volumeSlider.SetDpi  (dpi);
    }

    SetBounds (m_barRect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::GetTooltipAt
//
//  A collapsed entry has no label on the strip, so its name surfaces as a
//  tooltip (the shell owns the DxuiTooltip and its dwell timing).
//
//  A button carrying an EXPLICIT tip shows it in every form, collapsed or
//  not. Those tips say something the label cannot -- which machine Reset acts
//  on, the open-apple chord, what the theme name is a theme OF -- so
//  suppressing them wherever a label happens to be visible would hide the
//  only place that information appears.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CommandToolbar::GetTooltipAt (int x, int y, RECT & anchor) const
{
    const wchar_t *  tip = nullptr;



    // The input segments carry no labels in ANY form -- the shared label only
    // names the group -- so their tooltips always show and lead with the mode.
    if (IsInputExpanded())
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

    for (const Button & btn : m_buttons)
    {
        bool  over = tip == nullptr && btn.enabled && IsPointInRect (btn.rc, x, y);

        if (!over)
        {
            continue;
        }

        // The volume button's tip names the action it would take, not its
        // state; the collapsed input entry names the group it stands for.
        if (btn.entry == Entry::Volume && !btn.labeled)
        {
            anchor = btn.rc;
            tip    = m_muted ? L"Unmute" : L"Mute";
        }
        else if (btn.entry == Entry::Input && !btn.labeled)
        {
            anchor = btn.rc;
            tip    = s_kTipInput;
        }
        else if (!btn.tip.empty())
        {
            anchor = btn.rc;
            tip    = btn.tip.c_str();
        }
        else if (!btn.labeled)
        {
            anchor = btn.rc;
            tip    = btn.label;
        }
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::OnToolbarMouseMove
//
//  Shell-forwarded pointer motion. The slider gets first claim while it is
//  tracking a drag; otherwise hover states update per entry.
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

    // An expanded input entry is its segments, so the entry itself never
    // draws hover chrome around them.
    if (IsInputExpanded())
    {
        GetEntry (Entry::Input).hovered = false;

        for (int i = 0; i < InputSegCount(); i++)
        {
            m_inputSegs[i].hovered = IsPointInRect (m_inputSegs[i].rc, x, y);
            if (!m_inputSegs[i].hovered) { m_inputSegs[i].pressed = false; }
            over = over || m_inputSegs[i].hovered;
        }
    }

    // The flyout opens on hover over the volume button and stays while the
    // pointer remains in the button-flyout corridor -- the union rect, so
    // the travel across the bar's bottom margin cannot close it. A drag in
    // progress pins it open regardless (the pointer may leave the track).
    if (GetEntry (Entry::Volume).hovered)
    {
        m_flyoutOpen = true;
    }
    else if (m_flyoutOpen && !m_volumeSlider.IsDragging() &&
             !IsPointInRect (FlyoutKeepAliveRc(), x, y))
    {
        m_flyoutOpen = false;
    }

    return over || (m_flyoutOpen && IsPointInRect (m_flyoutRc, x, y)) ||
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

    for (InputSeg & seg : m_inputSegs)
    {
        seg.hovered = false;
        seg.pressed = false;
    }

    // The pointer left the window entirely; a drag can survive that (the
    // shell keeps forwarding while captured), so only close when idle. An
    // open menu is a separate window the pointer has just moved into, so
    // leaving the strip must not close that either.
    if (!m_volumeSlider.IsDragging())
    {
        m_flyoutOpen = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::OnToolbarLButtonDown
//
//  Press handling: arm an entry, start a slider drag, or eat the click.
//
//  AN OPEN MENU TAKES THE PRESS AND NOTHING ELSE DOES. Clicking anywhere on
//  the strip while a menu is up dismisses it, which is what makes the picker
//  buttons toggle instead of reopening the menu the same click just closed.
//
//  The volume slider gets first claim after that, but only while UNMUTED. A
//  muted slider is inert, so a press there should fall through to the bar
//  rather than starting a drag that changes a value nobody can hear.
//
//  A press only ARMS an entry; the command fires on release. That is what
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
    bool  handled = false;



    if (IsMenuOpen())
    {
        HideMenus();

        return true;
    }

    handled = m_flyoutOpen && !m_muted && m_volumeSlider.OnLButtonDown (x, y);

    for (Button & btn : m_buttons)
    {
        bool  expandedInput = btn.entry == Entry::Input && btn.labeled;

        if (!handled && !expandedInput && btn.enabled && IsPointInRect (btn.rc, x, y))
        {
            btn.pressed = true;
            handled     = true;
        }
    }

    if (IsInputExpanded())
    {
        for (int i = 0; !handled && i < InputSegCount(); i++)
        {
            if (IsPointInRect (m_inputSegs[i].rc, x, y))
            {
                m_inputSegs[i].pressed = true;
                handled                = true;
            }
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
//  Release handling: act on a completed click, and clear every pressed visual.
//
//  The loop clears EVERY entry's pressed state regardless of where the release
//  landed, because a press that ends elsewhere is a cancel and must leave
//  nothing stuck down. Only a press and release on the SAME entry acts.
//
//  What "act" means depends on the entry: a command dispatches, a picker opens
//  its menu, and mute is handled locally rather than dispatched, because it
//  owns state the slider reads back -- routing it through the command path
//  would put the toolbar's own model a round trip behind its own control.
//
//  Like the press, a release on dead space is consumed so it cannot reach the
//  emulator viewport underneath.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::OnToolbarLButtonUp (int x, int y)
{
    bool   handled = m_volumeSlider.OnLButtonUp (x, y);
    Entry  opening = Entry::Count;



    for (Button & btn : m_buttons)
    {
        bool  wasPressed = btn.pressed;

        btn.pressed = false;

        if (handled || !wasPressed || !btn.enabled || !IsPointInRect (btn.rc, x, y))
        {
            continue;
        }

        if (btn.entry == Entry::Volume)
        {
            SetVolume (m_volume01, !m_muted);

            if (m_volumeSink) { m_volumeSink (m_volume01, m_muted); }
        }
        else if (btn.entry == Entry::Theme || btn.entry == Entry::Color || btn.entry == Entry::Input)
        {
            // Deferred: Show() runs after the loop, so opening a menu cannot
            // disturb the pressed-state sweep that is still in progress.
            opening = btn.entry;
        }
        else if (m_dispatch)
        {
            m_dispatch (btn.id);
        }

        handled = true;
    }

    // Input segments: press-and-release on the same segment toggles its mode.
    for (int i = 0; i < 3; i++)
    {
        bool  segWasPressed = m_inputSegs[i].pressed;

        m_inputSegs[i].pressed = false;

        if (!handled && segWasPressed && i < InputSegCount() &&
            IsPointInRect (m_inputSegs[i].rc, x, y))
        {
            if (m_inputSink) { m_inputSink (s_kInputModes[i]); }
            handled = true;
        }
    }

    if (opening != Entry::Count && !IsReopenSuppressed())
    {
        OpenMenuFor (opening);
    }

    return handled || IsPointInRect (m_barRect, x, y) ||
           (m_flyoutOpen && IsPointInRect (m_flyoutRc, x, y));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PaintEntryIcon
//
//  Most icons are one Segoe MDL2 cell. The two that are not are drawn in the
//  same monoline pen as the input peripherals, because the set has no glyph
//  for what they mean: the monitor button lights its screen in the phosphor it
//  would switch to, which is the only way a one-cell button can state a color.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintEntryIcon (const Button & btn, IDxuiPainter & painter, IDxuiTextRenderer & text,
                                     float iconX, float iconTop, float iconDip, float rowH, uint32_t ink)
{
    HRESULT  hr       = S_OK;
    wchar_t  glyph[2] = { btn.glyph, 0 };
    RECT     box      = {};



    if (btn.glyph != 0)
    {
        hr = text.DrawString (glyph, iconX, iconTop, iconDip + 2.0f, rowH,
                              ink, iconDip, s_kIconFamily,
                              DxuiTextRenderer::HAlign::Left,
                              DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
    else
    {
        box.left   = (int) iconX;
        box.top    = (int) (iconTop + (rowH - iconDip) * 0.5f);
        box.right  = box.left + (int) iconDip;
        box.bottom = box.top  + (int) iconDip;

        // The input devices are the only drawn icon: MDL2 has no joystick, and
        // one hand-drawn glyph beside two from the font would mismatch stroke
        // weight, so the whole device set is drawn with the same pen.
        if (btn.entry == Entry::Input)
        {
            PaintJoystickMono (painter, box, ink);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PaintButton
//
//  Draws one entry: an icon, and its label beside it when it still has one.
//
//  Background chrome is drawn ONLY when hovered or pressed. An idle toolbar
//  shows bare icons on the bar, which is what keeps a row of ten buttons from
//  reading as ten boxes.
//
//  Disabled buttons dim the ink by rewriting its ALPHA rather than
//  substituting a theme color, so the disabled look follows whatever the
//  theme's foreground is instead of needing a matching swatch per theme.
//
//  The status LED is positioned relative to the ICON, not the button, so it
//  stays pinned to the glyph's corner regardless of how much label space the
//  entry's current form leaves around it.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintButton (Button & btn, IDxuiPainter & painter,
                                  IDxuiTextRenderer & text, const CassoTheme & theme)
{
    HRESULT           hr        = S_OK;
    bool              active    = btn.hovered || btn.pressed;
    float             bl        = (float) btn.rc.left;
    float             bt        = (float) btn.rc.top;
    float             bw        = (float) (btn.rc.right  - btn.rc.left);
    float             bh        = (float) (btn.rc.bottom - btn.rc.top);
    float             fontDip   = s_kFontDip * (float) m_dpi / (float) s_kBaseDpi;
    float             iconDip   = s_kIconDip * (float) m_dpi / (float) s_kBaseDpi;
    int               padX      = MulDiv (s_kBtnPadXDp, (int) m_dpi, s_kBaseDpi);
    int               iconGap   = MulDiv (s_kIconGapDp, (int) m_dpi, s_kBaseDpi);
    uint32_t          ink       = theme.navItemText;
    float             iconX     = 0.0f;
    float             textX     = 0.0f;
    const wchar_t *   labelText = btn.label;



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

    // A labeled entry keeps its icon left-padded with the label beside it; a
    // collapsed one centers the icon in what is left.
    iconX = btn.labeled ? bl + (float) padX : bl + (bw - iconDip) * 0.5f;

    PaintEntryIcon (btn, painter, text, iconX, bt, iconDip, bh, ink);

    if (btn.statusLed)
    {
        PaintStatusLed (painter, iconX + iconDip + 1.0f,
                        bt + bh * 0.5f - iconDip * 0.48f, m_dpi,
                        GetStatusCoreColor (m_printerStatus));
    }

    // The collapsed input entry borrows the same light to say that SOMETHING
    // is mapped; which device it is lives in the menu behind it.
    if (btn.entry == Entry::Input && !btn.labeled)
    {
        bool  anyMapped = m_arrowsJoystick || m_pointerMode != InputMappingMode::Off;

        PaintStatusLed (painter, iconX + iconDip + 1.0f,
                        bt + bh * 0.5f - iconDip * 0.48f, m_dpi,
                        anyMapped ? theme.ledActive : 0);
    }

    if (btn.labeled && labelText != nullptr && labelText[0] != 0)
    {
        textX = bl + (float) padX + iconDip + (float) iconGap;

        hr = text.DrawString (labelText, textX, bt,
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
//  A bottom hairline separates the strip from the emulator viewport; entries
//  paint over the window's existing chrome backdrop (frameless until hovered,
//  like the rest of the chrome).
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

    // The picker menus paint into their own popup windows, outside this call,
    // so they have to be handed the palette here -- without it PaintBody bails
    // and the menu comes up as an empty box.
    m_themeMenu.SetTheme (&dxuiTheme);
    m_colorMenu.SetTheme (&dxuiTheme);
    m_inputMenu.SetTheme (&dxuiTheme);

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
    GetEntry (Entry::Printer).enabled = m_printerPresent;

    for (Button & btn : m_buttons)
    {
        if (btn.entry == Entry::Input && btn.labeled)
        {
            PaintInputCluster (painter, text, theme);
        }
        else
        {
            PaintButton (btn, painter, text, theme);
        }
    }

    // The flyout paints LAST: it hangs below the bar over whatever chrome or
    // scene is there, and everything on the bar must be under it. The picker
    // menus are real popup windows and paint themselves.
    if (m_flyoutOpen)
    {
        PaintVolumeFlyout (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PaintInputCluster
//
//  The shared "Input" label (it names the group, it is not a button) and the
//  LED + glyph segments. Hover chrome matches the buttons'; the LED is the
//  state: an outline would read as focus, a lit LED reads as ON. Lit takes
//  the theme's LED color, so it matches the drive widgets' lights under every
//  preset; unlit does NOT take the drive bar's ledIdle, which is that color
//  darkened and reads as a black dot on the strip.
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
    uint32_t  offLed   = DxuiColor::ComputeTintForContrast (theme.navStrip, s_kOffLedContrast);
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

            if (m_inputMonoline)
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
                        wchar_t  glyph[2] = { s_kGlyphMouse, 0 };
                        float    emDip    = s_kIconDip * (float) m_dpi / (float) s_kBaseDpi;

                        hr = text.DrawString (glyph, (float) box.left, (float) box.top,
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
//  CommandToolbar::GetGlyphStroke
//
//  The pen every drawn device glyph uses. MDL2 draws roughly a fifteenth of
//  its em as stroke; the floor keeps the pen visible once the box is small
//  enough for that ratio to fall under a pixel.
//
////////////////////////////////////////////////////////////////////////////////

float CommandToolbar::GetGlyphStroke (float w)
{
    return (std::max) (1.15f, w / 15.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::StrokeCircle + the monoline glyph painters
//
//  Monoline device glyphs in the Segoe MDL2 language the bar's other icons
//  speak: uniform stroke, dots for controls, no shading. Drawn rather than
//  taken from the font because MDL2 has no joystick or paddle, and one
//  hand-drawn glyph next to two font glyphs would mismatch stroke weight --
//  so all three are drawn with the same pen. Geometry is in box fractions,
//  so the set scales together.
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
//  CommandToolbar::PaintPaddleMono
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintPaddleMono (IDxuiPainter & painter, const RECT & box, uint32_t ink)
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






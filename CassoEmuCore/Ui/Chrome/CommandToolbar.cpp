#include "Pch.h"
#include "Theme/DxuiColor.h"
#include "Theme/DxuiTheme.h"

#include "CassoTheme.h"
#include "CommandToolbar.h"
#include "Core/UnicodeSymbols.h"
#include "InputDeviceGlyphs.h"

#include "../../Resource.h"




// Layout metrics (DIP) the input cluster and the printer light still need
// on this side; the strip's own live in DxuiToolbar.
static constexpr int      s_kBaseDpi        = 96;
static constexpr int      s_kBtnPadXDp      = 10;   // inside a button, around content
static constexpr float    s_kIconDip        = 15.0f;
static constexpr float    s_kFontDip        = 13.0f;
static constexpr float    s_kFallbackCharPx = 7.5f;

static constexpr const wchar_t * s_kFontFamily = DxuiTheme::kBodyFace;
static constexpr const wchar_t * s_kIconFamily = L"Segoe MDL2 Assets";

// Segoe MDL2 Assets codepoints.
static constexpr const wchar_t * s_kGlyphSettings   = L"\uE713";   // gear
static constexpr const wchar_t * s_kGlyphTheme      = L"\uE746";   // half-filled square: light / dark
static constexpr const wchar_t * s_kGlyphScreenshot = L"\uE722";   // camera
static constexpr const wchar_t * s_kGlyphReset      = L"\uE72C";   // refresh arrow
static constexpr const wchar_t * s_kGlyphPower      = L"\uE7E8";   // power symbol
static constexpr const wchar_t * s_kGlyphVolume     = L"\uE767";   // speaker
static constexpr const wchar_t * s_kGlyphMuted      = L"\uE74F";   // muted speaker
static constexpr const wchar_t * s_kGlyphPrint      = L"\uE749";   // printer (monoline, matches the set)
static constexpr const wchar_t * s_kGlyphColor      = L"\uE790";   // artist's palette
static constexpr const wchar_t * s_kGlyphFullscreen = L"\uE740";   // diagonal arrows, outward
static constexpr const wchar_t * s_kGlyphRestore    = L"\uE73F";   // diagonal arrows, inward
static constexpr const wchar_t * s_kGlyphMouse      = L"\uE962";   // mouse (the input cluster's one font glyph)

// Volume flyout (vertical slider + readout under the track).
static constexpr int      s_kFlyoutWidthDp    = 56;
static constexpr int      s_kFlyoutHeightDp   = 154;

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

CommandToolbar::CommandToolbar() :
    m_inputCluster (*this),
    m_volumePanel  (*this)
{
    m_focusable = false;

    BuildCommands();
    BuildEntries();
    RebuildColorRows();
    RebuildInputRows();

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

    m_toolbar.SetFlyoutControl (s_kIdVolume, &m_volumePanel, SIZE { s_kFlyoutWidthDp, s_kFlyoutHeightDp });

    // The input menu has no preview -- its rows are toggles, not a value --
    // so it only needs the commit.
    m_toolbar.SetDropDownSinks (s_kIdInput, nullptr, [this] (int index)
    {
        if (m_inputSink && index >= 0 && index < InputSegCount())
        {
            m_inputSink (s_kInputModes[index]);
        }
    });

    RebuildActionTips();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::BuildCommands
//
//  One command per entry. The pickers label themselves with their PURPOSE,
//  not with the value they hold: the value is one click away in the menu,
//  and a label that changes with it moves every button to its right
//  whenever the setting changes.
//
//  The volume entry's full label is the ACTION its click takes, since that
//  is what its collapsed form shows as a tip; its short label is the name
//  the strip shows beside the icon.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::BuildCommands()
{
    struct Row
    {
        Entry            entry;
        int              id;
        const wchar_t *  glyph;
        const wchar_t *  label;
    };

    static constexpr Row  s_kRows[] =
    {
        { Entry::Settings,   IDM_VIEW_SETTINGS,        s_kGlyphSettings,   L"Settings"      },
        { Entry::Theme,      s_kIdTheme,               s_kGlyphTheme,      L"Theme"         },
        { Entry::Color,      s_kIdColor,               s_kGlyphColor,      L"Color"         },
        { Entry::Printer,    IDM_PRINTER_PREVIEW,      s_kGlyphPrint,      L"Printer"       },
        { Entry::Volume,     s_kIdVolume,              s_kGlyphVolume,     L"Mute"          },
        { Entry::Input,      s_kIdInput,               nullptr,            s_kTipInput      },
        { Entry::Fullscreen, IDM_VIEW_FULLSCREEN,      s_kGlyphFullscreen, L"Full screen"   },
        { Entry::Screenshot, IDM_EDIT_COPY_SCREENSHOT, s_kGlyphScreenshot, L"Screenshot"    },
        { Entry::Reset,      IDM_MACHINE_RESET,        s_kGlyphReset,      L"Reset"         },
        { Entry::Power,      IDM_MACHINE_POWERCYCLE,   s_kGlyphPower,      L"Power"         },
    };



    m_commands.resize ((size_t) Entry::Count);

    for (const Row & row : s_kRows)
    {
        DxuiCommand &  cmd = GetCommand (row.entry);

        cmd.id    = row.id;
        cmd.glyph = row.glyph;
        cmd.label = row.label;

        if (row.id > s_kIdInput)
        {
            cmd.dispatch = [this, id = row.id] ()
            {
                if (m_dispatch) { m_dispatch ((WORD) id); }
            };
        }
    }

    GetCommand (Entry::Volume).shortLabel = L"Volume";
    GetCommand (Entry::Volume).dispatch   = [this] () { ToggleMute(); };
    GetCommand (Entry::Input).shortLabel  = s_kInputLabel;

    // The printer button follows card presence: no card, no printer button.
    GetCommand (Entry::Printer).isEnabled = [this] () { return m_printerPresent; };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::BuildEntries
//
//  Groups: Settings through Printer, then Volume, then Input, then the four
//  machine actions; a change of group is a wider gap on the strip.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::BuildEntries()
{
    std::vector<DxuiToolbar::Entry>  entries;



    for (size_t i = 0; i < (size_t) Entry::Count; i++)
    {
        DxuiToolbar::Entry  e;
        Entry               entry = (Entry) i;

        e.command = &m_commands[i];
        e.kind    = DxuiToolbar::Kind::Command;
        e.group   = (entry <= Entry::Printer) ? 0
                  : (entry == Entry::Volume)  ? 1
                  : (entry == Entry::Input)   ? 2
                                              : 3;

        switch (entry)
        {
        case Entry::Theme:
        case Entry::Color:
            e.kind = DxuiToolbar::Kind::DropDown;
            break;

        case Entry::Volume:
            e.kind = DxuiToolbar::Kind::Flyout;
            break;

        case Entry::Input:
            e.kind   = DxuiToolbar::Kind::DropDown;
            e.custom = &m_inputCluster;
            break;

        case Entry::Printer:
            e.decoration = [this] (IDxuiPainter & painter, const IDxuiTheme & theme,
                                   const DxuiToolbarIconBox & icon, bool collapsed)
            {
                UNREFERENCED_PARAMETER (theme);
                UNREFERENCED_PARAMETER (collapsed);

                PaintPrinterLed (painter, icon);
            };
            break;

        default:
            break;
        }

        entries.push_back (std::move (e));
    }

    m_toolbar.SetEntries (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::RebuildThemeRows / RebuildColorRows / RebuildInputRows
//
//  Every picker row carries its checked state, which is how a collapsed
//  picker still says what it is set to. The rows are handed to the widget
//  again whenever they are rebuilt, since it holds them by pointer.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::RebuildThemeRows()
{
    std::vector<DxuiPopupMenuItem>  items;



    m_themeRows.clear();
    m_themeRows.resize (m_themeNames.size());

    for (size_t i = 0; i < m_themeNames.size(); i++)
    {
        m_themeRows[i].id        = (int) i;
        m_themeRows[i].label     = m_themeNames[i];
        m_themeRows[i].isChecked = [this, i] () { return (int) i == m_themeIndex; };

        items.push_back (DxuiPopupMenuItem::ForCommand (&m_themeRows[i]));
    }

    m_toolbar.SetDropDownItems (s_kIdTheme, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::RebuildColorRows
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::RebuildColorRows()
{
    std::vector<DxuiPopupMenuItem>  items;



    m_colorRows.clear();
    m_colorRows.resize (_countof (s_kMonitorColorRows));

    for (size_t i = 0; i < m_colorRows.size(); i++)
    {
        m_colorRows[i].id        = (int) i;
        m_colorRows[i].label     = s_kMonitorColorRows[i];
        m_colorRows[i].isChecked = [this, i] () { return (int) i == m_colorIndex; };

        items.push_back (DxuiPopupMenuItem::ForCommand (&m_colorRows[i]));
    }

    m_toolbar.SetDropDownItems (s_kIdColor, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::RebuildInputRows
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::RebuildInputRows()
{
    std::vector<DxuiPopupMenuItem>  items;



    m_inputRows.clear();
    m_inputRows.resize ((size_t) InputSegCount());

    for (size_t i = 0; i < m_inputRows.size(); i++)
    {
        m_inputRows[i].id        = (int) i;
        m_inputRows[i].label     = s_kInputRows[i];
        m_inputRows[i].isChecked = [this, i] () { return InputSegSelected ((int) i); };

        items.push_back (DxuiPopupMenuItem::ForCommand (&m_inputRows[i]));
    }

    m_toolbar.SetDropDownItems (s_kIdInput, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::RebuildActionTips
//
//  Reset and Power say which machine they act on, so the tips are composed
//  rather than fixed. Reset's also carries the reboot chord on a line of its
//  own, written as the two keycaps and nothing else: a chord is a picture of
//  what to hold. Which host key stands in for the apple is the keyboard map's
//  job, and repeating it in every tip that mentions the key made each one wrap.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::RebuildActionTips()
{
    std::wstring  machine = m_machineName.empty() ? std::wstring (L"machine") : m_machineName;
    std::wstring  apple   = DxuiTextRenderer::HasSymbolFont()
                                ? std::wstring (s_kpszOpenApple)
                                : std::wstring (L"Open Apple");



    GetCommand (Entry::Reset).tip = L"Reset the " + machine + L".\n" +
                                    apple + L" + Reset to reboot.";
    GetCommand (Entry::Power).tip = L"Power-cycle the " + machine;
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
    DxuiCommand &  cmd = GetCommand (Entry::Fullscreen);



    m_fullscreen = fullscreen;
    cmd.glyph    = m_fullscreen ? s_kGlyphRestore    : s_kGlyphFullscreen;
    cmd.label    = m_fullscreen ? L"Exit full screen" : L"Full screen";
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

    RebuildThemeRows();
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
    if (!m_toolbar.IsMenuOpen())
    {
        m_themeIndex = index;
    }
}


void CommandToolbar::SetMonitorColorIndex (int index)
{
    if (!m_toolbar.IsMenuOpen())
    {
        m_colorIndex = index;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::SetThemeSinks / SetMonitorSinks
//
//  A pick lands in the toolbar's own copy of the index before the shell hears
//  of it, so the row the NEXT open starts on is the one just chosen even if
//  the shell's sync is late.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::SetThemeSinks (ChoiceFn preview, ChoiceFn commit)
{
    m_toolbar.SetDropDownSinks (s_kIdTheme, std::move (preview), [this, commit] (int index)
    {
        m_themeIndex = index;

        if (commit) { commit (index); }
    });
}


void CommandToolbar::SetMonitorSinks (ChoiceFn preview, ChoiceFn commit)
{
    m_toolbar.SetDropDownSinks (s_kIdColor, std::move (preview), [this, commit] (int index)
    {
        m_colorIndex = index;

        if (commit) { commit (index); }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::SetVolume / ToggleMute
//
//  Mute is handled locally rather than dispatched, because it owns state the
//  slider reads back -- routing it through the command path would put the
//  toolbar's own model a round trip behind its own control.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::SetVolume (float volume01, bool muted)
{
    DxuiCommand &  cmd = GetCommand (Entry::Volume);



    m_volume01 = std::clamp (volume01, 0.0f, 1.0f);
    m_muted    = muted;

    // Muted DISPLAYS as silence -- slider at the bottom, 0% -- while the
    // stored level survives underneath, so unmuting restores exactly what
    // the user had. The slider is disabled while muted, so the zeroed
    // display can never be dragged into becoming the stored value.
    m_volumeSlider.SetValue   (m_muted ? 0.0f : m_volume01 * 100.0f);
    m_volumeSlider.SetEnabled (!m_muted);

    // The button's tip names the action it would take, not its state.
    cmd.glyph = m_muted ? s_kGlyphMuted : s_kGlyphVolume;
    cmd.label = m_muted ? L"Unmute"     : L"Mute";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::ToggleMute
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::ToggleMute()
{
    SetVolume (m_volume01, !m_muted);

    if (m_volumeSink) { m_volumeSink (m_volume01, m_muted); }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::IsPointInRect
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::IsPointInRect (const RECT & rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PlanForWidth / GetBandDp / GetTooltipAt / OnToolbarMouseMove
//
////////////////////////////////////////////////////////////////////////////////

int CommandToolbar::PlanForWidth (int clientWidthPx, const DxuiDpiScaler & scaler)
{
    return m_toolbar.PlanForWidth (clientWidthPx, scaler);
}


int CommandToolbar::GetBandDp() const
{
    return m_toolbar.GetBandDp();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::GetTooltipAt
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CommandToolbar::GetTooltipAt (int x, int y, RECT & anchor) const
{
    return m_toolbar.GetTooltipAt (x, y, anchor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::OnToolbarMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::OnToolbarMouseMove (int x, int y, bool leftDown)
{
    UNREFERENCED_PARAMETER (leftDown);

    return m_toolbar.OnToolbarMouseMove (x, y);
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
//  CommandToolbar::PaintStatusLed
//
//  A small status-light dot riding a glyph's corner (halo + core). core == 0
//  means unlit: paint nothing at all.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintStatusLed (IDxuiPainter & painter, float cx, float cy, UINT dpi, uint32_t core)
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
//  CommandToolbar::PaintPrinterLed
//
//  The light rides the glyph's top-right corner, pinned to the ICON rather
//  than the button so it stays put whether or not the entry has its label.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintPrinterLed (IDxuiPainter & painter, const DxuiToolbarIconBox & icon)
{
    PaintStatusLed (painter, icon.x + icon.size + 1.0f,
                    icon.top + icon.rowH * 0.5f - icon.size * 0.48f, m_dpi,
                    GetStatusCoreColor (m_printerStatus));
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
    RECT  bounds       = m_toolbar.GetBounds();



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
    if (countChanged)
    {
        RebuildInputRows();

        if (bounds.right > bounds.left)
        {
            DxuiDpiScaler  scaler;

            scaler.SetDpi (m_dpi);
            Layout (bounds, scaler);
        }
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
//  CommandToolbar::GetInputWidthPx
//
//  The input devices' full form is a shared label over a row of LED
//  segments; their collapsed form is a single icon like everything else.
//
////////////////////////////////////////////////////////////////////////////////

int CommandToolbar::GetInputWidthPx (bool labeled, UINT dpi) const
{
    int    padX   = MulDiv (s_kBtnPadXDp, (int) dpi, s_kBaseDpi);
    float  fontPx = s_kFontDip * (float) dpi / (float) s_kBaseDpi;
    int    iconW  = (int) (s_kIconDip * (float) dpi / (float) s_kBaseDpi + 0.5f);



    if (labeled)
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

    return padX * 2 + iconW;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::LayoutInput
//
//  The input segments live inside the input entry's own rect, so the cluster
//  travels with it instead of being placed a second time. A collapsed entry
//  has no segments, and their empty rects are what keeps their tips and
//  clicks silent.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::LayoutInput (const RECT & rc, bool labeled, UINT dpi)
{
    int    segW     = MulDiv (s_kSegPadXDp * 2 + s_kSegLedDp + s_kSegLedGapDp + s_kSegIconDp,
                              (int) dpi, s_kBaseDpi);
    int    segGap   = MulDiv (s_kSegGapDp,        (int) dpi, s_kBaseDpi);
    int    labelGap = MulDiv (s_kInputLabelGapDp, (int) dpi, s_kBaseDpi);
    int    ix       = rc.left;
    float  fontPx   = s_kFontDip * (float) dpi / (float) s_kBaseDpi;
    int    i        = 0;



    m_inputRc      = rc;
    m_inputLabelRc = {};

    for (i = 0; i < 3; i++)
    {
        m_inputSegs[i].rc = {};
    }

    if (labeled)
    {
        int  labelW = MeasureLabelPx (s_kInputLabel, fontPx) + 3;

        m_inputLabelRc = RECT { ix, rc.top, ix + labelW, rc.bottom };
        ix += labelW + labelGap;

        for (i = 0; i < InputSegCount(); i++)
        {
            m_inputSegs[i].rc = RECT { ix, rc.top, ix + segW, rc.bottom };
            ix += segW + ((i + 1 < InputSegCount()) ? segGap : 0);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_dpi = (scaler.GetDpi() == 0) ? (UINT) s_kBaseDpi : scaler.GetDpi();

    m_toolbar.Layout (boundsDip, scaler);
    SetBounds (m_toolbar.GetBounds());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::Paint
//
//  The strip continues the menu bar's themed surface (navStrip), so the two
//  chrome rows above the emulator read as one block, and its labels take the
//  bar's ink; neither is a color the generic theme mapping carries, so both
//  are pushed to the widget here, per paint, from the palette in hand.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & dxuiTheme)
{
    const CassoTheme &  theme = static_cast<const CassoTheme &> (dxuiTheme);



    _ASSERTE (dynamic_cast<const CassoTheme *> (&dxuiTheme) != nullptr);

    m_toolbar.SetStripColors (theme.navStrip, theme.navItemText);
    m_toolbar.Paint (painter, text, dxuiTheme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::PaintInputCollapsed
//
//  The collapsed input entry: the joystick drawn in the same monoline pen as
//  the segments, centered where a font glyph would sit, and the same light
//  the printer carries to say that SOMETHING is mapped; which device it is
//  lives in the menu behind it.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::PaintInputCollapsed (IDxuiPainter & painter, const CassoTheme & theme, const RECT & rc)
{
    float  bl        = (float) rc.left;
    float  bt        = (float) rc.top;
    float  bw        = (float) (rc.right  - rc.left);
    float  bh        = (float) (rc.bottom - rc.top);
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

    PaintStatusLed (painter, iconX + iconDip + 1.0f,
                    bt + bh * 0.5f - iconDip * 0.48f, m_dpi,
                    anyMapped ? theme.ledActive : 0);
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
                                        const CassoTheme & theme, const RECT & rc)
{
    HRESULT   hr       = S_OK;
    float     fontDip  = s_kFontDip * (float) m_dpi / (float) s_kBaseDpi;
    int       ledD     = MulDiv (s_kSegLedDp,    (int) m_dpi, s_kBaseDpi);
    int       ledGap   = MulDiv (s_kSegLedGapDp, (int) m_dpi, s_kBaseDpi);
    int       segPad   = MulDiv (s_kSegPadXDp,   (int) m_dpi, s_kBaseDpi);
    int       iconD    = MulDiv (s_kSegIconDp,   (int) m_dpi, s_kBaseDpi);
    uint32_t  offLed   = DxuiColor::ComputeTintForContrast (theme.navStrip, s_kOffLedContrast);
    uint32_t  labelInk = theme.navItemText;   // same ink as the button labels



    UNREFERENCED_PARAMETER (rc);

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
//  CommandToolbar::InputCluster::GetWidthPx / Layout / Paint
//
////////////////////////////////////////////////////////////////////////////////

int CommandToolbar::InputCluster::GetWidthPx (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const
{
    UNREFERENCED_PARAMETER (text);

    return m_owner.GetInputWidthPx (labeled, scaler.GetDpi());
}


void CommandToolbar::InputCluster::Layout (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler)
{
    m_owner.LayoutInput (rc, labeled, scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::InputCluster::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::InputCluster::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme,
                                          bool hovered, bool pressed, bool labeled)
{
    const CassoTheme &  casso = static_cast<const CassoTheme &> (theme);



    UNREFERENCED_PARAMETER (hovered);
    UNREFERENCED_PARAMETER (pressed);

    if (labeled)
    {
        m_owner.PaintInputCluster (painter, text, casso, m_owner.m_inputRc);
    }
    else
    {
        m_owner.PaintInputCollapsed (painter, casso, m_owner.m_inputRc);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::InputCluster::GetTooltipAt
//
//  The input segments carry no labels in ANY form -- the shared label only
//  names the group -- so their tooltips always show and lead with the mode.
//  Collapsed, the segments have no rects, and the entry's own name serves.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CommandToolbar::InputCluster::GetTooltipAt (int x, int y, RECT & anchor) const
{
    static constexpr const wchar_t * s_kSegTips[3] =
        { s_kTipJoystickSeg, s_kTipPaddleSeg, s_kTipMouseSeg };



    for (int i = 0; i < m_owner.InputSegCount(); i++)
    {
        if (IsPointInRect (m_owner.m_inputSegs[i].rc, x, y))
        {
            anchor = m_owner.m_inputSegs[i].rc;
            return s_kSegTips[i];
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::InputCluster::OnMouseMove / OnMouseLeave / OnLButtonDown / OnClick
//
//  Press-and-release on the same segment toggles its mode; a click that
//  lands on no segment is the entry's, which collapsed means its menu.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::InputCluster::OnMouseMove (int x, int y)
{
    bool  over = false;



    for (int i = 0; i < m_owner.InputSegCount(); i++)
    {
        InputSeg &  seg = m_owner.m_inputSegs[i];

        seg.hovered = IsPointInRect (seg.rc, x, y);
        if (!seg.hovered) { seg.pressed = false; }
        over = over || seg.hovered;
    }

    return over;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::InputCluster::OnMouseLeave
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::InputCluster::OnMouseLeave()
{
    for (InputSeg & seg : m_owner.m_inputSegs)
    {
        seg.hovered = false;
        seg.pressed = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::InputCluster::OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::InputCluster::OnLButtonDown (int x, int y)
{
    for (int i = 0; i < m_owner.InputSegCount(); i++)
    {
        if (IsPointInRect (m_owner.m_inputSegs[i].rc, x, y))
        {
            m_owner.m_inputSegs[i].pressed = true;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::InputCluster::OnClick
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::InputCluster::OnClick (int x, int y)
{
    bool  consumed = false;



    for (int i = 0; i < 3; i++)
    {
        InputSeg &  seg        = m_owner.m_inputSegs[i];
        bool        wasPressed = seg.pressed;

        seg.pressed = false;

        if (!consumed && wasPressed && i < m_owner.InputSegCount() && IsPointInRect (seg.rc, x, y))
        {
            if (m_owner.m_inputSink) { m_owner.m_inputSink (s_kInputModes[i]); }
            consumed = true;
        }
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::VolumePanel::Layout / Paint / OnMouse
//
//  The slider gets first claim on motion while it is tracking a drag, and
//  only starts one while UNMUTED: a muted slider is inert, so a press there
//  falls through to the bar rather than starting a drag that changes a value
//  nobody can hear.
//
////////////////////////////////////////////////////////////////////////////////

void CommandToolbar::VolumePanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_owner.m_volumeSlider.SetRect (boundsDip);
    m_owner.m_volumeSlider.SetDpi  (scaler.GetDpi());
    SetBounds (boundsDip);
}


void CommandToolbar::VolumePanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    m_owner.m_volumeSlider.SetEnabled (!m_owner.m_muted);
    m_owner.m_volumeSlider.Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar::VolumePanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CommandToolbar::VolumePanel::OnMouse (const DxuiMouseEvent & ev)
{
    DxuiSlider &  slider  = m_owner.m_volumeSlider;
    bool          handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        handled = slider.OnMouseMove (ev.positionDip.x, ev.positionDip.y);
        slider.SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        break;

    case DxuiMouseEventKind::Down:
        handled = !m_owner.m_muted && slider.OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        break;

    case DxuiMouseEventKind::Up:
        handled = slider.OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        break;

    default:
        break;
    }

    return handled;
}
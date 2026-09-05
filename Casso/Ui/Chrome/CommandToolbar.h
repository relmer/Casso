#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"
#include "Devices/Printer/PrinterStatusModel.h"   // PrinterStatus
#include "UiCommandTypes.h"                       // InputMappingMode
#include "Widgets/DxuiPopupMenu.h"
#include "Widgets/DxuiSlider.h"


class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar
//
//  The main window's command toolbar (spec 015 DCR-2): a chrome strip below
//  the menu bar carrying the most-used commands as icon + label buttons --
//  Settings, the theme and monitor-color pickers, Printer (with its status
//  LED, replacing the retired standalone printer indicator), the master
//  Volume slider + Mute, the input devices, Fullscreen, Screenshot, Reset,
//  and Power. Buttons are frameless until hovered / pressed (matching
//  the drive widgets) and dispatch their existing IDM_* command through
//  the same HandleCommand path as the menu, so the toolbar adds no new
//  command semantics.
//
//  RESPONSIVE BEHAVIOR: every entry has a full form (icon + label, and for
//  the input devices a row of LED segments) and a collapsed form that is a
//  single icon. When the strip runs out of room entries collapse ONE AT A
//  TIME FROM THE RIGHT, so the leftmost keep their names longest and no
//  entry ever falls off the end. The three entries that are pickers rather
//  than commands -- theme, monitor color, input -- open a checkable popup
//  menu, which is what lets them collapse to one icon without losing
//  anything.
//
//  Icons are Segoe MDL2 Assets glyphs (the repo's established icon face)
//  except for the input devices, which the set has no joystick for and which
//  are drawn in a monoline pen matching it. The Printer button carries a
//  status-LED dot on its glyph's corner. That light is EVENT-ONLY
//  -- unlit while idle (no light = no problem): bright green = receiving a
//  print, bright amber = a finished page is waiting, bright red = delivery
//  error.
//
//  Input is hand-routed by EmulatorShell (like the joystick button): the
//  shell forwards mouse events to OnMouseMove / OnLButtonDown / OnLButtonUp,
//  which also drive the embedded DxuiSlider. Volume changes surface through
//  the VolumeFn sink as (volume01, muted).
//
////////////////////////////////////////////////////////////////////////////////

class CommandToolbar : public IDxuiControl
{
public:
    using DispatchFn = std::function<void (WORD)>;
    using VolumeFn   = std::function<void (float, bool)>;
    using InputFn    = std::function<void (InputMappingMode)>;
    using ChoiceFn   = std::function<void (int index)>;

    CommandToolbar  ();
    ~CommandToolbar () override = default;

    // Decides how many entries can still afford their label at this width and
    // returns the band thickness (dp) the strip needs. The shell calls this
    // BEFORE docking the chrome bands.
    int   PlanForWidth     (int clientWidthPx, const DxuiDpiScaler & scaler);
    int   GetBandDp        () const;

    // The hovered entry's tooltip, and the rect to anchor it on. Returns
    // nullptr when nothing should show.
    const wchar_t *  GetTooltipAt (int x, int y, RECT & anchor) const;

    // The DWrite renderer used to measure labels during Layout (the shell's
    // chrome text renderer; must outlive this control).
    void  SetTextRenderer   (IDxuiTextRenderer * text)   { m_textRenderer = text; }

    void  SetDispatch       (DispatchFn fn)              { m_dispatch = std::move (fn); }
    void  SetVolumeSink     (VolumeFn fn)                { m_volumeSink = std::move (fn); }

    // The machine the Reset / Power tooltips talk about ("Apple //e"), and
    // the presentation the fullscreen button offers to leave or enter.
    void  SetMachineDisplayName (const std::wstring & displayName);
    void  SetFullscreen         (bool fullscreen);

    // Theme picker: display names in the order the shell holds their ids,
    // and the row the active theme sits on. Monitor color is the fixed
    // Color / Green / Amber / White set the Settings picture list carries.
    void  SetThemes            (const std::vector<std::wstring> & displayNames, int activeIndex);
    void  SetThemeIndex        (int index);
    void  SetMonitorColorIndex (int index);

    // Both pickers preview while their menu is open and settle when it
    // closes. Preview applies without persisting, because a highlight is
    // not a choice; commit is the user's pick. A dismissed menu replays
    // preview with the row it opened on, which is the snap-back.
    void  SetThemeSinks        (ChoiceFn preview, ChoiceFn commit);
    void  SetMonitorSinks      (ChoiceFn preview, ChoiceFn commit);

    // Routes the picker menus through the host's popup-window pool so they
    // escape the strip and hang over the emulator viewport, and supplies the
    // client rect they are kept inside.
    void  SetPopupHost         (DxuiHwndSource * host);
    void  SetHostClientRect    (const RECT & clientRect) { m_hostClient = clientRect; }

    // An open menu owns the keyboard: the shell hands it every keydown so
    // arrowing through the rows previews instead of typing into the guest.
    bool  IsMenuOpen           () const;
    bool  HandleKey            (WPARAM vk);

    // Input-mode entry: one "Input" label over three LED + glyph segments
    // (joystick / paddle / mouse) while it has room, and a single icon with
    // the same three as a checkable menu once it does not. Clicking either
    // reports the mode to toggle; state arrives per frame.
    void  SetInputSink       (InputFn fn)                { m_inputSink = std::move (fn); }
    void  SetInputState      (bool arrowsJoystick, InputMappingMode pointer, bool mouseAvailable);
    void  SetInputSkeuoStyle (bool skeuo)                { m_inputSkeuo = skeuo; }

    // Monoline icon experiment: strokes-and-dots device glyphs matching the
    // Segoe MDL2 language of the bar's other icons, instead of the shaded
    // skeuomorphic drawings. On by default so the bar reads as one icon set;
    // flip off to compare against the peripheral renderings.
    void  SetInputMonoline   (bool monoline)             { m_inputMonoline = monoline; }

    // The volume flyout (vertical slider + readout) opens on hover over the
    // volume button and closes when the pointer leaves button + flyout.
    // Exposed so the shell can keep presenting frames while it is up.
    bool  IsVolumeFlyoutOpen () const                    { return m_flyoutOpen; }

    // Seed the volume controls from persisted prefs (no sink callback).
    void  SetVolume         (float volume01, bool muted);
    float GetVolume         () const                     { return m_volume01; }
    bool  IsMuted           () const                     { return m_muted; }

    void  SetPrinterStatus  (PrinterStatus status)       { m_printerStatus = status; }
    void  SetPrinterPresent (bool present)               { m_printerPresent = present; }

    // Shell-forwarded mouse input. Return true when the event was consumed
    // (over a button, or the slider is tracking a drag).
    bool  OnToolbarMouseMove   (int x, int y, bool leftDown);
    void  OnToolbarMouseLeave  ();
    bool  OnToolbarLButtonDown (int x, int y);
    bool  OnToolbarLButtonUp   (int x, int y);

    bool  HitTest           (int x, int y) const;

    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

private:
    // The entries, in the order they sit on the strip. The order is also the
    // COLLAPSE order read backwards: the last entry gives up its label first.
    enum class Entry
    {
        Settings   = 0,
        Theme,
        Color,
        Printer,
        Volume,
        Input,
        Fullscreen,
        Screenshot,
        Reset,
        Power,
        Count,
    };

    // One entry on the strip. `glyph` is a Segoe MDL2 codepoint, or 0 when
    // the icon is drawn instead (the input devices). The pickers label
    // themselves with their PURPOSE, not with the value they hold: the value
    // is one click away in the menu, and a label that changes with it moves
    // every button to its right whenever the setting changes.
    struct Button
    {
        Entry            entry     = Entry::Settings;
        WORD             id        = 0;         // 0 => not a dispatch
        wchar_t          glyph     = 0;         // 0 => drawn, see PaintEntryIcon
        const wchar_t *  label     = nullptr;
        std::wstring     tip;                   // shown in EVERY form; see GetTooltipAt
        bool             statusLed = false;
        RECT             rc        = {};
        bool             hovered   = false;
        bool             pressed   = false;
        bool             enabled   = true;
        bool             labeled   = true;      // set by the collapse pass
    };

    // One input segment: LED + peripheral glyph, no label of its own (the
    // entry's shared label + per-segment tooltips carry the names).
    struct InputSeg
    {
        RECT  rc      = {};
        bool  hovered = false;
        bool  pressed = false;
    };

    static bool      IsPointInRect      (const RECT & rc, int x, int y);
    static uint32_t  GetStatusCoreColor (PrinterStatus status);

    Button       &  GetEntry           (Entry entry)       { return m_buttons[(size_t) entry]; }
    const Button &  GetEntry           (Entry entry) const { return m_buttons[(size_t) entry]; }

    int   MeasureLabelPx  (const wchar_t * text, float fontPx) const;
    int   GetEntryWidthPx (const Button & btn, bool labeled, UINT dpi) const;
    int   GetTotalWidthPx (int labeledCount, UINT dpi) const;

    void  WireMenus           ();
    void  RebuildActionTips   ();
    void  OpenMenuFor         (Entry entry);
    void  HideMenus           ();
    bool  IsReopenSuppressed  () const;

    void  PaintButton      (Button & btn, IDxuiPainter & painter,
                            IDxuiTextRenderer & text, const struct CassoTheme & theme);
    void  PaintEntryIcon   (const Button & btn, IDxuiPainter & painter, IDxuiTextRenderer & text,
                            float iconX, float iconTop, float iconDip, float rowH, uint32_t ink);
    void  PaintInputCluster (IDxuiPainter & painter, IDxuiTextRenderer & text,
                             const struct CassoTheme & theme);

    // A circle outline as line segments -- the painter has filled circles
    // and lines, but no arcs or outlined circles.
    static void      StrokeCircle      (IDxuiPainter & painter, float cx, float cy,
                                        float r, float stroke, uint32_t ink);

    static void      PaintJoystickMono (IDxuiPainter & painter, const RECT & box, uint32_t ink);
    static void      PaintPaddleMono   (IDxuiPainter & painter, const RECT & box, uint32_t ink);
    static void      PaintMouseMono    (IDxuiPainter & painter, const RECT & box, uint32_t ink);
    void             PaintVolumeFlyout (IDxuiPainter & painter, IDxuiTextRenderer & text,
                                        const struct CassoTheme & theme);

    int              InputSegCount     () const { return m_mouseAvailable ? 3 : 2; }
    bool             InputSegSelected  (int index) const;
    bool             IsInputExpanded   () const;
    RECT             FlyoutKeepAliveRc () const;

    std::vector<Button>   m_buttons;        // indexed by Entry, in visual order
    DxuiSlider            m_volumeSlider;   // vertical, lives in the flyout

    InputSeg              m_inputSegs[3];   // joystick, paddle, mouse
    RECT                  m_inputLabelRc   = {};
    InputFn               m_inputSink;
    bool                  m_arrowsJoystick = false;
    InputMappingMode      m_pointerMode    = InputMappingMode::Off;
    bool                  m_mouseAvailable = false;
    bool                  m_inputSkeuo     = true;
    bool                  m_inputMonoline  = true;

    DxuiPopupMenu         m_themeMenu;
    DxuiPopupMenu         m_colorMenu;
    DxuiPopupMenu         m_inputMenu;
    ChoiceFn              m_themePreview;
    ChoiceFn              m_themeCommit;
    ChoiceFn              m_monitorPreview;
    ChoiceFn              m_monitorCommit;
    bool                  m_themePreviewed   = false;   // a highlight moved off the open row
    bool                  m_colorPreviewed   = false;
    uint64_t              m_menuClosedMs     = 0;       // see IsReopenSuppressed

    std::vector<std::wstring>  m_themeNames;
    int                        m_themeIndex = -1;
    int                        m_colorIndex = 0;

    std::wstring          m_machineName;
    bool                  m_fullscreen     = false;
    RECT                  m_hostClient     = {};

    bool                  m_flyoutOpen     = false;
    RECT                  m_flyoutRc       = {};

    IDxuiTextRenderer *   m_textRenderer   = nullptr;
    DispatchFn            m_dispatch;
    VolumeFn              m_volumeSink;

    RECT                  m_barRect        = {};
    UINT                  m_dpi            = 96;
    int                   m_labeledCount   = (int) Entry::Count;
    float                 m_volume01       = 1.0f;
    bool                  m_muted          = false;
    PrinterStatus         m_printerStatus  = PrinterStatus::Idle;
    bool                  m_printerPresent = false;
};

#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"
#include "Devices/Printer/PrinterStatusModel.h"   // PrinterStatus
#include "UiCommandTypes.h"                       // InputMappingMode
#include "Widgets/DxuiSlider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandToolbar
//
//  The main window's command toolbar (spec 015 DCR-2): a chrome strip below
//  the menu bar carrying the most-used commands as icon + label buttons --
//  Settings, Printer (with its status LED, replacing the retired standalone
//  printer indicator), the master Volume slider + Mute, Screenshot, Reset,
//  and Power. Buttons are frameless until hovered / pressed (matching
//  JoystickToggleButton) and dispatch their existing IDM_* command through
//  the same HandleCommand path as the menu, so the toolbar adds no new
//  command semantics.
//
//  Icons are Segoe MDL2 Assets glyphs (the repo's established icon face);
//  the Printer button additionally carries a status-LED dot on its glyph's
//  corner. The light is EVENT-ONLY -- unlit while idle (no light = no
//  problem): bright green = receiving a print, bright amber = a finished
//  page is waiting in the printer, bright red = delivery error.
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

    // Responsive presentation, chosen from the window width (widest first):
    // icon + label to the right, icon with the label stacked BELOW (ribbon
    // style), then icon-only -- where tooltips carry the labels.
    enum class Mode
    {
        LabelRight,
        LabelBelow,
        IconOnly,
    };

    CommandToolbar  ();
    ~CommandToolbar () override = default;

    // Pick the presentation mode for a client width and return the band
    // thickness (dp) it needs -- the shell calls this BEFORE docking the
    // chrome bands, since the stacked mode needs a taller strip.
    int   PlanForWidth   (int clientWidthPx, const DxuiDpiScaler & scaler);
    int   GetBandDp      () const { return m_bandDp; }
    Mode  GetCurrentMode () const { return m_mode; }

    // The hovered button's label for the shell's tooltip (icon-only mode has
    // no labels, so tooltips are required there). Returns nullptr when no
    // tooltip should show; fills `anchor` with the button rect otherwise.
    const wchar_t *  GetTooltipAt (int x, int y, RECT & anchor) const;

    // The DWrite renderer used to measure labels during Layout (the shell's
    // chrome text renderer; must outlive this control).
    void  SetTextRenderer   (IDxuiTextRenderer * text)   { m_textRenderer = text; }

    void  SetDispatch       (DispatchFn fn)              { m_dispatch = std::move (fn); }
    void  SetVolumeSink     (VolumeFn fn)                { m_volumeSink = std::move (fn); }

    // Input-mode cluster: one "Input" label over three LED + glyph segments
    // (joystick / paddle / mouse), the toolbar home of what used to be the
    // drive-band device selector -- the stacked desk left that row nowhere
    // to live. Clicking a segment reports the mode to toggle; state arrives
    // per frame exactly as the selector's did.
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
    // One icon + label command button. `glyph` is a Segoe MDL2 codepoint
    // (monoline, matching the set); the printer button additionally sets
    // `statusLed` so a status-light dot rides its glyph's corner.
    struct Button
    {
        WORD             id        = 0;
        wchar_t          glyph     = 0;
        const wchar_t *  label     = nullptr;
        bool             statusLed = false;
        RECT             rc        = {};
        bool             hovered   = false;
        bool             pressed   = false;
        bool             enabled   = true;
    };

    // One input segment: LED + peripheral glyph, no label of its own (the
    // cluster's shared label + per-segment tooltips carry the names).
    struct InputSeg
    {
        RECT  rc      = {};
        bool  hovered = false;
        bool  pressed = false;
    };

    static bool      IsPointInRect      (const RECT & rc, int x, int y);
    static uint32_t  GetStatusCoreColor (PrinterStatus status);

    void             PaintButton        (Button & btn, IDxuiPainter & painter,
                                     IDxuiTextRenderer & text, const struct CassoTheme & theme);
    void             PaintInputCluster (IDxuiPainter & painter, IDxuiTextRenderer & text,
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
    RECT             FlyoutKeepAliveRc () const;

    std::vector<Button>   m_buttons;        // command buttons in visual order
    Button                m_muteButton;     // toggles mute (not a dispatch id)
    DxuiSlider            m_volumeSlider;   // vertical, lives in the flyout

    InputSeg              m_inputSegs[3];   // joystick, paddle, mouse
    RECT                  m_inputLabelRc   = {};
    InputFn               m_inputSink;
    bool                  m_arrowsJoystick = false;
    InputMappingMode      m_pointerMode    = InputMappingMode::Off;
    bool                  m_mouseAvailable = false;
    bool                  m_inputSkeuo     = true;
    bool                  m_inputMonoline  = true;

    bool                  m_flyoutOpen     = false;
    RECT                  m_flyoutRc       = {};

    IDxuiTextRenderer *   m_textRenderer   = nullptr;
    DispatchFn            m_dispatch;
    VolumeFn              m_volumeSink;

    RECT                  m_barRect        = {};
    UINT                  m_dpi            = 96;
    Mode                  m_mode           = Mode::LabelRight;
    int                   m_bandDp         = 42;      // strip thickness for the current mode
    float                 m_volume01       = 1.0f;
    bool                  m_muted          = false;
    PrinterStatus         m_printerStatus  = PrinterStatus::Idle;
    bool                  m_printerPresent = false;
};

#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"
#include "Devices/Printer/PrinterStatusModel.h"   // PrinterStatus
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
//  Icons are Segoe MDL2 Assets glyphs (the repo's established icon face),
//  except the Printer button, which draws the miniature skeuomorphic
//  ImageWriter II (lifted from PrinterIndicator) so the printer keeps its
//  distinctive glyph; its front-panel light carries the PrinterStatus colour:
//  dim green = powered + idle, bright green = receiving a print, amber = a
//  finished page is waiting in the printer, red = delivery error.
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
    int   PlanForWidth (int clientWidthPx, const DxuiDpiScaler & scaler);
    int   BandDp       () const { return m_bandDp; }
    Mode  CurrentMode  () const { return m_mode; }

    // The hovered button's label for the shell's tooltip (icon-only mode has
    // no labels, so tooltips are required there). Returns nullptr when no
    // tooltip should show; fills `anchor` with the button rect otherwise.
    const wchar_t *  TooltipAt (int x, int y, RECT & anchor) const;

    // The DWrite renderer used to measure labels during Layout (the shell's
    // chrome text renderer; must outlive this control).
    void  SetTextRenderer   (IDxuiTextRenderer * text)   { m_textRenderer = text; }

    void  SetDispatch       (DispatchFn fn)              { m_dispatch = std::move (fn); }
    void  SetVolumeSink     (VolumeFn fn)                { m_volumeSink = std::move (fn); }

    // Seed the volume controls from persisted prefs (no sink callback).
    void  SetVolume         (float volume01, bool muted);
    float Volume            () const                     { return m_volume01; }
    bool  IsMuted           () const                     { return m_muted; }

    void  SetPrinterStatus  (PrinterStatus status)       { m_printerStatus = status; }
    void  SetPrinterPresent (bool present)               { m_printerPresent = present; }

    // Machine-colored chrome: the strip paints this vertical case-color
    // gradient (Disk ][ beige for the II family, platinum for //c-era
    // machines) with dark ink for contrast. 0 falls back to the theme
    // background with theme ink.
    void  SetMachineTint    (uint32_t topArgb, uint32_t botArgb)
    {
        m_tintTop = topArgb;
        m_tintBot = botArgb;
    }

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

    static bool      PointIn        (const RECT & rc, int x, int y);
    static uint32_t  StatusCore     (PrinterStatus status);

    void             PaintButton    (Button & btn, IDxuiPainter & painter,
                                     IDxuiTextRenderer & text, const struct CassoTheme & theme);

    std::vector<Button>   m_buttons;        // command buttons in visual order
    Button                m_muteButton;     // toggles mute (not a dispatch id)
    DxuiSlider            m_volumeSlider;

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
    uint32_t              m_tintTop        = 0;       // machine case tint (0 = theme background)
    uint32_t              m_tintBot        = 0;
};

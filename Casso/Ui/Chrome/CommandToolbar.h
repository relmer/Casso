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

    CommandToolbar  ();
    ~CommandToolbar () override = default;

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
    // One icon + label command button. `glyph` is a Segoe MDL2 codepoint;
    // the printer button sets `printerGlyph` instead and paints the mini
    // ImageWriter II with the status light.
    struct Button
    {
        WORD             id           = 0;
        wchar_t          glyph        = 0;
        const wchar_t *  label        = nullptr;
        bool             printerGlyph = false;
        RECT             rc           = {};
        bool             hovered      = false;
        bool             pressed      = false;
        bool             enabled      = true;
    };

    static bool      PointIn        (const RECT & rc, int x, int y);
    static uint32_t  StatusCore     (PrinterStatus status);

    void             PaintButton    (Button & btn, IDxuiPainter & painter,
                                     IDxuiTextRenderer & text, const struct CassoTheme & theme);
    void             PaintMiniPrinter (const RECT & rc, IDxuiPainter & painter);

    std::vector<Button>   m_buttons;        // command buttons in visual order
    Button                m_muteButton;     // toggles mute (not a dispatch id)
    DxuiSlider            m_volumeSlider;

    IDxuiTextRenderer *   m_textRenderer   = nullptr;
    DispatchFn            m_dispatch;
    VolumeFn              m_volumeSink;

    RECT                  m_barRect        = {};
    UINT                  m_dpi            = 96;
    float                 m_volume01       = 1.0f;
    bool                  m_muted          = false;
    PrinterStatus         m_printerStatus  = PrinterStatus::Idle;
    bool                  m_printerPresent = false;
};

#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPreviewController
//
//  Owns the live-preview fade state machine for the Settings panel.
//  While a slider is being dragged or a dropdown is open, the renderer
//  reveals the emulator under the settings window. Keyboard-driven
//  changes auto-end the preview a short time after the last keystroke.
//
//  The controller is pure state -- it never touches the renderer or
//  any widgets. SettingsPanel feeds it interaction events via
//  StartPreview / EndPreview and advances the fade animation each
//  frame via Tick(); the panel reads back the current focus and
//  alpha values to drive its own paint pipeline.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsPreviewController
{
public:
    enum class Focus
    {
        None                = 0,
        BrightnessSlider    = 1,
        ContrastSlider      = 2,
        MonitorDropdown     = 3,
        ScanlinesSlider     = 4,
        BloomRadiusSlider   = 5,
        BloomStrengthSlider = 6,
        ColorBleedSlider    = 7,
        GammaSlider         = 8,
        PersistenceSlider   = 9,
    };

    void   StartPreview   (Focus focus, bool keyboardMode);
    void   EndPreview     ();
    void   Reset          ();
    void   Tick           (int64_t nowMs);

    bool   IsActive       () const { return m_focus != Focus::None; }
    Focus  FocusedControl () const { return m_focus; }
    int    FocusedId      () const { return (int) m_focus; }
    float  PanelAlpha     () const { return m_panelAlpha; }
    float  FocusedAlpha   () const { return m_focusedAlpha; }


private:
    Focus    m_focus            = Focus::None;
    bool     m_keyboard         = false;     // true => auto-end via idle timer
    int64_t  m_lastInteractMs   = 0;
    int64_t  m_lastFrameMs      = 0;
    float    m_panelAlpha       = 1.0f;
    float    m_focusedAlpha     = 1.0f;
};

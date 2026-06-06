#include "Pch.h"

#include "SettingsPreviewController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StartPreview
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPreviewController::StartPreview (Focus focus, bool keyboardMode)
{
    m_focus            = focus;
    m_keyboard         = keyboardMode;
    m_lastInteractMs   = (int64_t) GetTickCount64();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndPreview
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPreviewController::EndPreview ()
{
    m_focus     = Focus::None;
    m_keyboard  = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  Wipes alphas + timers along with focus. Called when the panel is
//  shown or cancelled so a previous session's mid-drag interaction
//  state can't bleed into the next one.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPreviewController::Reset ()
{
    m_focus            = Focus::None;
    m_keyboard         = false;
    m_lastInteractMs   = 0;
    m_lastFrameMs      = 0;
    m_panelAlpha       = 1.0f;
    m_focusedAlpha     = 1.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Advances the fade animation. Targets 0% panel / 90% focused-control
//  alpha while a preview is active, snapping back to fully opaque
//  otherwise. Linear ramp lands on target after `s_kFadeDurationMs`.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPreviewController::Tick (int64_t nowMs)
{
    constexpr int64_t  s_kKeyboardIdleMs = 500;     // auto-end 0.5s after last keystroke
    constexpr float    s_kFadeDurationMs = 180.0f;  // panel/scrim fade-in/out time

    float    targetPanel   = 1.0f;
    float    targetFocused = 1.0f;
    int64_t  dtMs          = 0;
    float    maxStep       = 0.0f;



    if (m_lastFrameMs == 0)
    {
        m_lastFrameMs = nowMs;
    }
    dtMs = nowMs - m_lastFrameMs;
    m_lastFrameMs = nowMs;

    // Keyboard idle timeout: auto-end the preview once the user stops
    // tapping arrow keys. Mouse-drag preview ends explicitly on
    // mouse-up via EndPreview so this check is keyboard-only.
    if (m_focus != Focus::None && m_keyboard &&
        (nowMs - m_lastInteractMs) >= s_kKeyboardIdleMs)
    {
        EndPreview();
    }

    if (m_focus != Focus::None)
    {
        targetPanel   = 0.0f;
        targetFocused = 0.9f;
    }

    if (dtMs <= 0 || s_kFadeDurationMs <= 0.0f)
    {
        m_panelAlpha   = targetPanel;
        m_focusedAlpha = targetFocused;
        return;
    }

    // Linear ramp toward target at 1.0/duration per ms. Lands exactly
    // on the target after the configured fade duration.
    maxStep = (float) dtMs / s_kFadeDurationMs;

    if (targetPanel > m_panelAlpha)
    {
        m_panelAlpha = std::min (targetPanel, m_panelAlpha + maxStep);
    }
    else
    {
        m_panelAlpha = std::max (targetPanel, m_panelAlpha - maxStep);
    }

    if (targetFocused > m_focusedAlpha)
    {
        m_focusedAlpha = std::min (targetFocused, m_focusedAlpha + maxStep);
    }
    else
    {
        m_focusedAlpha = std::max (targetFocused, m_focusedAlpha - maxStep);
    }
}

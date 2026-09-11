#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"
#include "Widgets/DxuiSlider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeFlyout
//
//  The master volume as the toolbar's flyout hosts it: a vertical slider
//  with its readout under the track, and the mute state the slider displays
//  as silence while the stored level survives underneath. Volume changes
//  surface through the VolumeFn sink as (volume01, muted); the toolbar's
//  volume entry toggles mute through ToggleMute.
//
//  A muted slider is inert, so a press on it falls through to the bar rather
//  than starting a drag that changes a value nobody can hear.
//
////////////////////////////////////////////////////////////////////////////////

class VolumeFlyout : public IDxuiControl
{
public:
    using VolumeFn = std::function<void (float volume01, bool muted)>;

    static constexpr SIZE  kPanelDp = { 56, 154 };

    VolumeFlyout ();

    void   SetSink     (VolumeFn fn)                  { m_sink = std::move (fn); }

    // Seed from persisted prefs (no sink callback).
    void   SetVolume   (float volume01, bool muted);
    void   ToggleMute  ();
    float  GetVolume   () const                       { return m_volume01; }
    bool   IsMuted     () const                       { return m_muted; }

    void   Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void   Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool   OnMouse        (const DxuiMouseEvent & ev) override;

    // Keyboard: the toolbar hands the panel focus when Enter opens it, and
    // the arrows, Home, End and the page keys that follow move the level.
    bool   OnKey          (const DxuiKeyEvent & ev) override     { return !m_muted && m_slider.OnKey (ev); }
    void   OnFocusChanged (bool focused) override                { m_slider.SetFocused (focused); }

private:
    DxuiSlider  m_slider;
    VolumeFn    m_sink;
    float       m_volume01 = 1.0f;
    bool        m_muted    = false;
};

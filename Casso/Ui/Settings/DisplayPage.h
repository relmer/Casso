#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"
#include "../Widgets/Dropdown.h"
#include "../Widgets/Label.h"
#include "../Widgets/Slider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage
//
//  Visual-output settings: monitor type (color / green mono / amber
//  mono / white mono), CRT brightness, CRT contrast. Monitor type
//  routes through SettingsPanelState (per-machine prefs); brightness
//  and contrast route through the panel's pending CRT staging (global
//  prefs) so the host can revert them on Cancel.
//
////////////////////////////////////////////////////////////////////////////////

class DisplayPage
{
public:
    using BrightnessFn = std::function<void (float value)>;
    using ContrastFn   = std::function<void (float value)>;

    void  SetState              (SettingsPanelState * state);
    void  SetInitialCrt         (float brightness, float contrast);
    void  SetOnBrightnessChange (BrightnessFn fn) { m_onBrightness = std::move (fn); }
    void  SetOnContrastChange   (ContrastFn   fn) { m_onContrast   = std::move (fn); }

    void  Layout    (const RECT & rect, const DpiScaler & scaler);
    void  Rebuild   ();

    void  OnLButtonDown (int x, int y);
    void  OnLButtonUp   (int x, int y);
    void  OnMouseMove   (int x, int y);
    void  OnMouseHover  (int x, int y);
    bool  OnKey         (WPARAM vk);

    void  Paint (DxUiPainter & painter, DwriteTextRenderer & text) const;

    void  CollectFocusables (std::vector<std::function<void (bool)>> & out);
    bool  AnyDropdownOpen   () const { return m_monitor.IsOpen(); }

    // Test accessors.
    const Dropdown & MonitorDropdown    () const { return m_monitor;    }
    const Slider   & BrightnessSlider   () const { return m_brightness; }
    const Slider   & ContrastSlider     () const { return m_contrast;   }

private:
    SettingsPanelState  * m_state = nullptr;
    BrightnessFn          m_onBrightness;
    ContrastFn            m_onContrast;

    Label                 m_monitorLabel;
    Label                 m_brightnessLabel;
    Label                 m_contrastLabel;

    Dropdown              m_monitor;
    Slider                m_brightness;
    Slider                m_contrast;
};

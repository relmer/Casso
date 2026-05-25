#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"
#include "../Widgets/Dropdown.h"
#include "../Widgets/Label.h"
#include "../Widgets/Slider.h"
#include "../Widgets/Toggle.h"





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
    using BrightnessFn    = std::function<void (float value)>;
    using ContrastFn      = std::function<void (float value)>;
    using MonitorFn       = std::function<void (int colorModeIndex)>;
    using ScanlinesEnFn   = std::function<void (bool enabled)>;
    using ScanlinesIntFn  = std::function<void (float intensity)>;
    using BloomEnFn       = std::function<void (bool enabled)>;
    using BloomRadiusFn   = std::function<void (float radius)>;
    using BloomStrengthFn = std::function<void (float strength)>;
    using ColorBleedEnFn  = std::function<void (bool enabled)>;
    using ColorBleedWFn   = std::function<void (float width)>;
    using PreviewFn       = std::function<void (int controlId, bool start, bool keyboardMode)>;

    // Control ids used by SetOnPreview to identify which control is
    // being interacted with. Match the SettingsPanel::PreviewFocus
    // enum offsets so the panel can cast directly.
    static constexpr int  kControlBrightness     = 1;
    static constexpr int  kControlContrast       = 2;
    static constexpr int  kControlMonitor        = 3;
    static constexpr int  kControlScanlinesInt   = 4;
    static constexpr int  kControlBloomRadius    = 5;
    static constexpr int  kControlBloomStrength  = 6;
    static constexpr int  kControlColorBleedW    = 7;

    void  SetState              (SettingsPanelState * state);
    void  SetInitialCrt         (const struct GlobalUserPrefsCrtSnapshot & snap);
    void  SetOnBrightnessChange     (BrightnessFn    fn) { m_onBrightness    = std::move (fn); }
    void  SetOnContrastChange       (ContrastFn      fn) { m_onContrast      = std::move (fn); }
    void  SetOnMonitorChange        (MonitorFn       fn) { m_onMonitor       = std::move (fn); }
    void  SetOnScanlinesEnChange    (ScanlinesEnFn   fn) { m_onScanlinesEn   = std::move (fn); }
    void  SetOnScanlinesIntChange   (ScanlinesIntFn  fn) { m_onScanlinesInt  = std::move (fn); }
    void  SetOnBloomEnChange        (BloomEnFn       fn) { m_onBloomEn       = std::move (fn); }
    void  SetOnBloomRadiusChange    (BloomRadiusFn   fn) { m_onBloomRadius   = std::move (fn); }
    void  SetOnBloomStrengthChange  (BloomStrengthFn fn) { m_onBloomStrength = std::move (fn); }
    void  SetOnColorBleedEnChange   (ColorBleedEnFn  fn) { m_onColorBleedEn  = std::move (fn); }
    void  SetOnColorBleedWChange    (ColorBleedWFn   fn) { m_onColorBleedW   = std::move (fn); }
    void  SetOnPreview              (PreviewFn       fn) { m_onPreview       = std::move (fn); }

    void  Layout    (const RECT & rect, const DpiScaler & scaler);
    void  Rebuild   ();

    void  OnLButtonDown (int x, int y);
    void  OnLButtonUp   (int x, int y);
    void  OnMouseMove   (int x, int y);
    void  OnMouseHover  (int x, int y);
    bool  OnKey         (WPARAM vk);

    // Render. When focusedControlId is -1 every control paints at
    // `nonFocusedAlpha`; otherwise the matching control paints at
    // `focusedAlpha` (used by the panel's live-preview fade so the
    // user can see the slider / dropdown they're interacting with
    // while the rest of the UI fades out).
    void  Paint (DxUiPainter & painter, DwriteTextRenderer & text,
                 int focusedControlId = -1,
                 float nonFocusedAlpha = 1.0f,
                 float focusedAlpha    = 1.0f) const;

    void  CollectFocusables (std::vector<std::function<void (bool)>> & out);
    bool  AnyDropdownOpen   () const { return m_monitor.IsOpen(); }

    // Test accessors.
    const Dropdown & MonitorDropdown    () const { return m_monitor;          }
    const Slider   & BrightnessSlider   () const { return m_brightness;       }
    const Slider   & ContrastSlider     () const { return m_contrast;         }
    const Toggle   & ScanlinesToggle    () const { return m_scanlinesEn;      }
    const Slider   & ScanlinesSlider    () const { return m_scanlinesInt;     }
    const Toggle   & BloomToggle        () const { return m_bloomEn;          }
    const Slider   & BloomRadiusSlider  () const { return m_bloomRadius;      }
    const Slider   & BloomStrengthSlider() const { return m_bloomStrength;    }
    const Toggle   & ColorBleedToggle   () const { return m_colorBleedEn;     }
    const Slider   & ColorBleedSlider   () const { return m_colorBleedW;      }

private:
    SettingsPanelState  * m_state = nullptr;
    BrightnessFn          m_onBrightness;
    ContrastFn            m_onContrast;
    MonitorFn             m_onMonitor;
    ScanlinesEnFn         m_onScanlinesEn;
    ScanlinesIntFn        m_onScanlinesInt;
    BloomEnFn             m_onBloomEn;
    BloomRadiusFn         m_onBloomRadius;
    BloomStrengthFn       m_onBloomStrength;
    ColorBleedEnFn        m_onColorBleedEn;
    ColorBleedWFn         m_onColorBleedW;
    PreviewFn             m_onPreview;

    Label                 m_monitorLabel;
    Label                 m_brightnessLabel;
    Label                 m_contrastLabel;
    Label                 m_scanlinesIntLabel;
    Label                 m_bloomRadiusLabel;
    Label                 m_bloomStrengthLabel;
    Label                 m_colorBleedWLabel;

    Dropdown              m_monitor;
    Slider                m_brightness;
    Slider                m_contrast;
    Toggle                m_scanlinesEn;
    Slider                m_scanlinesInt;
    Toggle                m_bloomEn;
    Slider                m_bloomRadius;
    Slider                m_bloomStrength;
    Toggle                m_colorBleedEn;
    Slider                m_colorBleedW;

    RECT                  m_monitorRowRect       = {};
    RECT                  m_brightnessRowRect    = {};
    RECT                  m_contrastRowRect      = {};
    RECT                  m_scanlinesEnRowRect   = {};
    RECT                  m_scanlinesIntRowRect  = {};
    RECT                  m_bloomEnRowRect       = {};
    RECT                  m_bloomRadiusRowRect   = {};
    RECT                  m_bloomStrengthRowRect = {};
    RECT                  m_colorBleedEnRowRect  = {};
    RECT                  m_colorBleedWRowRect   = {};
};


// Snapshot struct used by SettingsPanel to seed DisplayPage with the
// full set of CRT values at Show() time. Mirrors GlobalUserPrefs::Crt
// minus the userOverride bookkeeping field.
struct GlobalUserPrefsCrtSnapshot
{
    float    brightness          = 1.0f;
    float    contrast            = 1.0f;
    bool     scanlinesEnabled    = false;
    float    scanlinesIntensity  = 0.5f;
    bool     bloomEnabled        = false;
    float    bloomRadius         = 1.0f;
    float    bloomStrength       = 0.5f;
    bool     colorBleedEnabled   = false;
    float    colorBleedWidth     = 1.0f;
};

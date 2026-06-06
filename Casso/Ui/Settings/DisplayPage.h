#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiSlider.h"
#include "Widgets/DxuiToggle.h"


class IDxuiTheme;





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

// Snapshot struct used by SettingsPanel to seed DisplayPage with the
// full set of CRT values at Show() time. Mirrors GlobalUserPrefs::Crt
// minus the userOverride bookkeeping field.
struct GlobalUserPrefsCrtSnapshot
{
    float    brightness          = 1.0f;
    float    contrast            = 1.0f;
    float    gamma               = 1.0f;
    float    persistence         = 0.0f;
    bool     scanlinesEnabled    = false;
    float    scanlinesIntensity  = 0.5f;
    bool     bloomEnabled        = false;
    float    bloomRadius         = 1.0f;
    float    bloomStrength       = 0.5f;
    bool     colorBleedEnabled   = false;
    float    colorBleedWidth     = 1.0f;
};


// Per-control "what is the current default" snapshot. Each value is
// the resolved default for this monitor (theme override if the active
// theme defines that field-group, else the monitor preset). The
// *FromTheme flags say which source won, so DisplayPage can render
// "(theme default)" vs "(monitor default)" next to controls whose
// current value matches the default. Theme schema does NOT carry
// gamma or persistence -- those are always monitor-owned.
struct DisplayDefaultsHint
{
    GlobalUserPrefsCrtSnapshot  values;
    bool  brightnessFromTheme    = false;
    bool  contrastFromTheme      = false;
    bool  scanlinesFromTheme     = false;
    bool  bloomFromTheme         = false;
    bool  colorBleedFromTheme    = false;
};


class DisplayPage
{
public:
    using BrightnessFn    = std::function<void (float value)>;
    using ContrastFn      = std::function<void (float value)>;
    using GammaFn         = std::function<void (float value)>;
    using PersistenceFn   = std::function<void (float value)>;
    using MonitorFn       = std::function<void (int colorModeIndex)>;
    using ScanlinesEnFn   = std::function<void (bool enabled)>;
    using ScanlinesIntFn  = std::function<void (float intensity)>;
    using BloomEnFn       = std::function<void (bool enabled)>;
    using BloomRadiusFn   = std::function<void (float radius)>;
    using BloomStrengthFn = std::function<void (float strength)>;
    using ColorBleedEnFn  = std::function<void (bool enabled)>;
    using ColorBleedWFn   = std::function<void (float width)>;
    using PreviewFn       = std::function<void (int controlId, bool start, bool keyboardMode)>;
    using RestoreFn       = std::function<void ()>;

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
    static constexpr int  kControlGamma          = 8;
    static constexpr int  kControlPersistence    = 9;

    void  SetState              (SettingsPanelState * state);
    void  SetInitialCrt         (const struct GlobalUserPrefsCrtSnapshot & snap);
    void  SetDefaultsHint       (const struct DisplayDefaultsHint        & hint);
    void  SetOnBrightnessChange     (BrightnessFn    fn) { m_onBrightness    = std::move (fn); }
    void  SetOnContrastChange       (ContrastFn      fn) { m_onContrast      = std::move (fn); }
    void  SetOnGammaChange          (GammaFn         fn) { m_onGamma         = std::move (fn); }
    void  SetOnPersistenceChange    (PersistenceFn   fn) { m_onPersistence   = std::move (fn); }
    void  SetOnMonitorChange        (MonitorFn       fn) { m_onMonitor       = std::move (fn); }
    void  SetOnScanlinesEnChange    (ScanlinesEnFn   fn) { m_onScanlinesEn   = std::move (fn); }
    void  SetOnScanlinesIntChange   (ScanlinesIntFn  fn) { m_onScanlinesInt  = std::move (fn); }
    void  SetOnBloomEnChange        (BloomEnFn       fn) { m_onBloomEn       = std::move (fn); }
    void  SetOnBloomRadiusChange    (BloomRadiusFn   fn) { m_onBloomRadius   = std::move (fn); }
    void  SetOnBloomStrengthChange  (BloomStrengthFn fn) { m_onBloomStrength = std::move (fn); }
    void  SetOnColorBleedEnChange   (ColorBleedEnFn  fn) { m_onColorBleedEn  = std::move (fn); }
    void  SetOnColorBleedWChange    (ColorBleedWFn   fn) { m_onColorBleedW   = std::move (fn); }
    void  SetOnPreview              (PreviewFn       fn) { m_onPreview       = std::move (fn); }
    void  SetOnRestoreDefaults      (RestoreFn       fn) { m_onRestore       = std::move (fn); }

    void  Layout    (const RECT & rect, const DxuiDpiScaler & scaler);
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
    //
    void  Paint (DxuiPainter & painter, DxuiTextRenderer & text,
                 const IDxuiTheme & theme,
                 int          focusedControlId = -1,
                 float        nonFocusedAlpha  = 1.0f,
                 float        focusedAlpha     = 1.0f) const;

    void  CollectFocusables (std::vector<std::function<void (bool)>> & out);
    bool  AnyDropdownOpen   () const { return m_monitor.IsOpen(); }
    RECT  FocusedControlRect (int controlId) const;

    // Test accessors.
    const DxuiDropdown & MonitorDropdown    () const { return m_monitor;          }
    const DxuiSlider   & BrightnessSlider   () const { return m_brightness;       }
    const DxuiSlider   & ContrastSlider     () const { return m_contrast;         }
    const DxuiSlider   & GammaSlider        () const { return m_gamma;            }
    const DxuiSlider   & PersistenceSlider  () const { return m_persistence;      }
    const DxuiToggle   & ScanlinesToggle    () const { return m_scanlinesEn;      }
    const DxuiSlider   & ScanlinesSlider    () const { return m_scanlinesInt;     }
    const DxuiToggle   & BloomToggle        () const { return m_bloomEn;          }
    const DxuiSlider   & BloomRadiusSlider  () const { return m_bloomRadius;      }
    const DxuiSlider   & BloomStrengthSlider() const { return m_bloomStrength;    }
    const DxuiToggle   & ColorBleedToggle   () const { return m_colorBleedEn;     }
    const DxuiSlider   & ColorBleedSlider   () const { return m_colorBleedW;      }
    const DxuiButton   & RestoreButton      () const { return m_restore;          }

private:
    SettingsPanelState  * m_state = nullptr;
    DisplayDefaultsHint   m_hint  = {};
    DxuiDpiScaler             m_scaler;
    BrightnessFn          m_onBrightness;
    ContrastFn            m_onContrast;
    GammaFn               m_onGamma;
    PersistenceFn         m_onPersistence;
    MonitorFn             m_onMonitor;
    ScanlinesEnFn         m_onScanlinesEn;
    ScanlinesIntFn        m_onScanlinesInt;
    BloomEnFn             m_onBloomEn;
    BloomRadiusFn         m_onBloomRadius;
    BloomStrengthFn       m_onBloomStrength;
    ColorBleedEnFn        m_onColorBleedEn;
    ColorBleedWFn         m_onColorBleedW;
    PreviewFn             m_onPreview;
    RestoreFn             m_onRestore;

    DxuiLabel                 m_monitorLabel;
    DxuiLabel                 m_brightnessLabel;
    DxuiLabel                 m_contrastLabel;
    DxuiLabel                 m_gammaLabel;
    DxuiLabel                 m_persistenceLabel;
    // Left-column labels for the toggle rows -- the toggle widget itself
    // paints just the On/Off state indicator in the value column.
    DxuiLabel                 m_scanlinesLabel;
    DxuiLabel                 m_bloomLabel;
    DxuiLabel                 m_colorBleedLabel;
    DxuiLabel                 m_scanlinesIntLabel;
    DxuiLabel                 m_bloomRadiusLabel;
    DxuiLabel                 m_bloomStrengthLabel;
    DxuiLabel                 m_colorBleedWLabel;

    DxuiDropdown              m_monitor;
    DxuiSlider                m_brightness;
    DxuiSlider                m_contrast;
    DxuiSlider                m_gamma;
    DxuiSlider                m_persistence;
    DxuiToggle                m_scanlinesEn;
    DxuiSlider                m_scanlinesInt;
    DxuiToggle                m_bloomEn;
    DxuiSlider                m_bloomRadius;
    DxuiSlider                m_bloomStrength;
    DxuiToggle                m_colorBleedEn;
    DxuiSlider                m_colorBleedW;
    DxuiButton                m_restore;

    RECT                  m_monitorRowRect       = {};
    RECT                  m_brightnessRowRect    = {};
    RECT                  m_contrastRowRect      = {};
    RECT                  m_gammaRowRect         = {};
    RECT                  m_persistenceRowRect   = {};
    RECT                  m_scanlinesEnRowRect   = {};
    RECT                  m_scanlinesIntRowRect  = {};
    RECT                  m_bloomEnRowRect       = {};
    RECT                  m_bloomRadiusRowRect   = {};
    RECT                  m_bloomStrengthRowRect = {};
    RECT                  m_colorBleedEnRowRect  = {};
    RECT                  m_colorBleedWRowRect   = {};
    RECT                  m_restoreRowRect       = {};
    int                   m_indicatorX           = 0;
};

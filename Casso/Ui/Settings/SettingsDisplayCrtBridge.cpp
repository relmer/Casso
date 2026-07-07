#include "Pch.h"

#include "SettingsDisplayCrtBridge.h"

#include "DisplayPage.h"
#include "../ColorUtil.h"
#include "../ThemeManager.h"
#include "../../EmulatorShell.h"
#include "../../Config/CrtPresets.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Bind
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDisplayCrtBridge::Bind (
    GlobalUserPrefs    * prefs,
    ThemeManager       * themes,
    SettingsPanelState * state,
    DisplayPage        * displayPage,
    EmulatorShell      * emuShell)
{
    m_prefs       = prefs;
    m_themes      = themes;
    m_state       = state;
    m_displayPage = displayPage;
    m_emuShell    = emuShell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActiveModeIdx
//
//  Returns the currently-selected monitor type as an index into
//  GlobalUserPrefs::crtByMode. Reads SettingsPanelState because the
//  monitor dropdown writes there as the source of truth; the live
//  shell state can lag by a frame.
//
////////////////////////////////////////////////////////////////////////////////

int SettingsDisplayCrtBridge::ActiveModeIdx () const
{
    int  idx = 0;



    if (m_state == nullptr)
    {
        return 0;
    }

    idx = (int) m_state->Prefs().colorMode;
    if (idx < 0 || idx >= (int) GlobalUserPrefs::kCrtModeCount)
    {
        return 0;
    }
    return idx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReseedFromActiveMode
//
//  Pushes the currently-active monitor's CRT block into the DisplayPage
//  sliders. Called at panel Show, after a monitor change, and after
//  "Restore defaults" so the slider widgets reflect whatever
//  MakeCrtParams will produce on the next frame.
//
//  When the active block has userOverride=false we read the resolved
//  preset values (CrtPresets::ForMode) so the user sees "what the
//  defaults are" rather than the still-zero in-struct defaults.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDisplayCrtBridge::ReseedFromActiveMode ()
{
    GlobalUserPrefsCrtSnapshot  snap;
    int                         idx = ActiveModeIdx();



    if (m_displayPage == nullptr)
    {
        return;
    }

    if (m_prefs == nullptr)
    {
        m_displayPage->SetInitialCrt (snap);
        return;
    }

    const auto &              blk           = m_prefs->crtByMode[idx];
    const auto &              preset        = CrtPresets::ForMode ((size_t) idx);
    const ThemeCrtDefaults *  themeDefaults = nullptr;
    if (m_themes != nullptr)
    {
        const LoadedTheme *  active = m_themes->GetActiveTheme();
        if (active != nullptr)
        {
            themeDefaults = &active->crtDefaults;
        }
    }

    if (blk.userOverride)
    {
        snap.brightness         = blk.brightness;
        snap.contrast           = blk.contrast;
        snap.gamma              = blk.gamma;
        snap.persistence        = blk.persistence;
        snap.scanlinesEnabled   = blk.scanlinesEnabled;
        snap.scanlinesIntensity = blk.scanlinesIntensity;
        snap.bloomEnabled       = blk.bloomEnabled;
        snap.bloomRadius        = blk.bloomRadius;
        snap.bloomStrength      = blk.bloomStrength;
        snap.colorBleedEnabled  = blk.colorBleedEnabled;
        snap.colorBleedWidth    = blk.colorBleedWidth;
    }
    else
    {
        // No user override: mirror MakeCrtParams's resolution chain
        // (preset, with theme overrides on top). Otherwise the sliders
        // would show preset values while the renderer was actually
        // applying theme-overridden values, and the visual would
        // appear to jump the moment the user touched any slider.
        snap.brightness         = preset.brightness;
        snap.contrast           = preset.contrast;
        snap.gamma              = preset.gamma;
        snap.persistence        = preset.persistence;
        snap.scanlinesEnabled   = preset.scanlinesEnabled;
        snap.scanlinesIntensity = preset.scanlinesIntensity;
        snap.bloomEnabled       = preset.bloomEnabled;
        snap.bloomRadius        = preset.bloomRadius;
        snap.bloomStrength      = preset.bloomStrength;
        snap.colorBleedEnabled  = preset.colorBleedEnabled;
        snap.colorBleedWidth    = preset.colorBleedWidth;
        if (themeDefaults != nullptr)
        {
            if (themeDefaults->hasBrightness) { snap.brightness = themeDefaults->brightness; }
            if (themeDefaults->hasContrast)   { snap.contrast   = themeDefaults->contrast;   }
            if (themeDefaults->hasScanlines)
            {
                snap.scanlinesEnabled   = themeDefaults->scanlinesEnabled;
                snap.scanlinesIntensity = themeDefaults->scanlinesIntensity;
            }
            if (themeDefaults->hasBloom)
            {
                snap.bloomEnabled  = themeDefaults->bloomEnabled;
                snap.bloomRadius   = themeDefaults->bloomRadius;
                snap.bloomStrength = themeDefaults->bloomStrength;
            }
            if (themeDefaults->hasColorBleed)
            {
                snap.colorBleedEnabled = themeDefaults->colorBleedEnabled;
                snap.colorBleedWidth   = themeDefaults->colorBleedWidth;
            }
        }
    }
    m_displayPage->SetInitialCrt (snap);

    // Re-publish the per-control defaults hint so DisplayPage knows
    // which value counts as "the default" and can render the
    // (theme default) / (monitor default) badge in each row.
    PublishDefaultsHint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishDefaultsHint
//
//  Computes the resolved default value (theme override layered on
//  monitor preset) for each Display-page control and pushes the
//  snapshot to DisplayPage. Theme schema doesn't carry gamma or
//  persistence, so those are always reported as monitor-owned.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDisplayCrtBridge::PublishDefaultsHint ()
{
    DisplayDefaultsHint  hint;



    if (m_displayPage == nullptr)
    {
        return;
    }

    if (m_prefs == nullptr)
    {
        m_displayPage->SetDefaultsHint (hint);
        return;
    }

    int                       idx           = ActiveModeIdx();
    const auto &              preset        = CrtPresets::ForMode ((size_t) idx);
    const ThemeCrtDefaults *  themeDefaults = nullptr;
    if (m_themes != nullptr)
    {
        const LoadedTheme *  active = m_themes->GetActiveTheme();
        if (active != nullptr)
        {
            themeDefaults = &active->crtDefaults;
        }
    }

    // Start from the monitor preset for every field.
    hint.values.brightness         = preset.brightness;
    hint.values.contrast           = preset.contrast;
    hint.values.gamma              = preset.gamma;
    hint.values.persistence        = preset.persistence;
    hint.values.scanlinesEnabled   = preset.scanlinesEnabled;
    hint.values.scanlinesIntensity = preset.scanlinesIntensity;
    hint.values.bloomEnabled       = preset.bloomEnabled;
    hint.values.bloomRadius        = preset.bloomRadius;
    hint.values.bloomStrength      = preset.bloomStrength;
    hint.values.colorBleedEnabled  = preset.colorBleedEnabled;
    hint.values.colorBleedWidth    = preset.colorBleedWidth;

    // Layer theme overrides ONLY for the field-groups the theme
    // actually declares -- otherwise an unset group's struct-default
    // (scanlinesEnabled=false etc.) would silently overwrite the
    // monitor preset's correct value.
    if (themeDefaults != nullptr)
    {
        if (themeDefaults->hasBrightness)
        {
            hint.values.brightness    = themeDefaults->brightness;
            hint.brightnessFromTheme  = true;
        }
        if (themeDefaults->hasContrast)
        {
            hint.values.contrast    = themeDefaults->contrast;
            hint.contrastFromTheme  = true;
        }
        if (themeDefaults->hasScanlines)
        {
            hint.values.scanlinesEnabled   = themeDefaults->scanlinesEnabled;
            hint.values.scanlinesIntensity = themeDefaults->scanlinesIntensity;
            hint.scanlinesFromTheme        = true;
        }
        if (themeDefaults->hasBloom)
        {
            hint.values.bloomEnabled  = themeDefaults->bloomEnabled;
            hint.values.bloomRadius   = themeDefaults->bloomRadius;
            hint.values.bloomStrength = themeDefaults->bloomStrength;
            hint.bloomFromTheme       = true;
        }
        if (themeDefaults->hasColorBleed)
        {
            hint.values.colorBleedEnabled = themeDefaults->colorBleedEnabled;
            hint.values.colorBleedWidth   = themeDefaults->colorBleedWidth;
            hint.colorBleedFromTheme      = true;
        }
    }

    m_displayPage->SetDefaultsHint (hint);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromoteActiveToOverride
//
//  First time the user touches any CRT control on an untouched monitor,
//  copy the resolved preset values into the active block before flipping
//  userOverride=true. Without this, the slider's lambda writes only the
//  one changed field and leaves every other field at the default-
//  constructed Crt{} zeros (scanlinesEnabled=false, bloomEnabled=false,
//  brightness=1.0, etc.), which silently turns off whatever the preset
//  had enabled the moment userOverride flips on.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDisplayCrtBridge::PromoteActiveToOverride ()
{
    if (m_prefs == nullptr)
    {
        return;
    }
    if (m_prefs->crtByMode[ActiveModeIdx()].userOverride)
    {
        return;
    }
    ResetActiveToDefaults();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetActiveToDefaults
//
//  Unconditionally replace the active monitor's CRT block with the
//  resolved defaults (monitor preset, with active theme overrides
//  layered on top), then flip userOverride=true. Restore Defaults on
//  the Display page calls this; PromoteActiveToOverride defers to it
//  only when userOverride is still false.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDisplayCrtBridge::ResetActiveToDefaults ()
{
    if (m_prefs == nullptr)
    {
        return;
    }

    auto &                    blk           = m_prefs->crtByMode[ActiveModeIdx()];
    const auto &              preset        = CrtPresets::ForMode ((size_t) ActiveModeIdx());
    const ThemeCrtDefaults *  themeDefaults = nullptr;
    if (m_themes != nullptr)
    {
        const LoadedTheme *  active = m_themes->GetActiveTheme();
        if (active != nullptr)
        {
            themeDefaults = &active->crtDefaults;
        }
    }

    // Seed the block with the SAME values MakeCrtParams would produce
    // right now: preset for this monitor, with the active theme's
    // crtDefaults layered on top. Without the theme layer, a clean
    // theme (e.g. contrast=1.0, scanlines off) would silently flip
    // to the raw preset values (contrast=0.9, scanlines on, bloom on)
    // the instant the user nudged any slider, which looks like a bug.
    blk = preset;
    if (themeDefaults != nullptr)
    {
        if (themeDefaults->hasBrightness) { blk.brightness = themeDefaults->brightness; }
        if (themeDefaults->hasContrast)   { blk.contrast   = themeDefaults->contrast;   }
        if (themeDefaults->hasScanlines)
        {
            blk.scanlinesEnabled   = themeDefaults->scanlinesEnabled;
            blk.scanlinesIntensity = themeDefaults->scanlinesIntensity;
        }
        if (themeDefaults->hasBloom)
        {
            blk.bloomEnabled  = themeDefaults->bloomEnabled;
            blk.bloomRadius   = themeDefaults->bloomRadius;
            blk.bloomStrength = themeDefaults->bloomStrength;
        }
        if (themeDefaults->hasColorBleed)
        {
            blk.colorBleedEnabled = themeDefaults->colorBleedEnabled;
            blk.colorBleedWidth   = themeDefaults->colorBleedWidth;
        }
    }
    blk.userOverride = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WireDisplayPageCallbacks
//
//  Installs the live-edit + restore-defaults callbacks on the bound
//  DisplayPage. Every slider / toggle change funnels through
//  PromoteActiveToOverride so the user's first edit on an untouched
//  monitor inherits the resolved preset values; Restore Defaults
//  short-circuits to ResetActiveToDefaults and re-seeds the slider
//  widgets via ReseedFromActiveMode.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDisplayCrtBridge::WireDisplayPageCallbacks ()
{
    if (m_displayPage == nullptr)
    {
        return;
    }

    // Brightness / contrast / gamma / persistence sliders write LIVE
    // to the currently-active monitor's CRT block. ActiveModeIdx()
    // reads from SettingsPanelState so every edit lands on whichever
    // monitor type the user has selected in the dropdown.
    m_displayPage->SetOnBrightnessChange ([this] (float pct)
    {
        if (m_prefs != nullptr)
        {
            PromoteActiveToOverride();
            m_prefs->crtByMode[ActiveModeIdx()].brightness = pct / 100.0f;     // slider 0..200% -> shader 0..2.0
        }
    });
    m_displayPage->SetOnContrastChange ([this] (float pct)
    {
        if (m_prefs != nullptr)
        {
            PromoteActiveToOverride();
            m_prefs->crtByMode[ActiveModeIdx()].contrast = pct / 100.0f;
        }
    });
    m_displayPage->SetOnGammaChange ([this] (float g)
    {
        if (m_prefs != nullptr)
        {
            PromoteActiveToOverride();
            m_prefs->crtByMode[ActiveModeIdx()].gamma = g;
        }
    });
    m_displayPage->SetOnPersistenceChange ([this] (float pct)
    {
        if (m_prefs != nullptr)
        {
            PromoteActiveToOverride();
            m_prefs->crtByMode[ActiveModeIdx()].persistence = pct / 100.0f;
        }
    });

    // Monitor dropdown updates both palette AND active mode index so
    // the live render AND the slider widgets reflect the hovered /
    // selected monitor's full CRT settings. State gets reverted from
    // PreparePreviewFrame's dropdown-close detector if the user
    // cancels the dropdown without committing.
    m_displayPage->SetOnMonitorChange ([this] (int idx)
    {
        if (m_state != nullptr)
        {
            m_state->SetColorMode ((SettingsColorMode) idx);
        }
        if (m_emuShell != nullptr)
        {
            m_emuShell->SetColorModeLive (idx);
        }
        ReseedFromActiveMode();
    });

    // Per-effect toggles + parameter sliders write LIVE to the active
    // monitor's CRT block.
    m_displayPage->SetOnScanlinesEnChange ([this] (bool on)
    {
        if (m_prefs != nullptr) { PromoteActiveToOverride(); m_prefs->crtByMode[ActiveModeIdx()].scanlinesEnabled  = on; }
    });
    m_displayPage->SetOnScanlinesIntChange ([this] (float pct)
    {
        if (m_prefs != nullptr) { PromoteActiveToOverride(); m_prefs->crtByMode[ActiveModeIdx()].scanlinesIntensity = pct / 100.0f; }
    });
    m_displayPage->SetOnBloomEnChange ([this] (bool on)
    {
        if (m_prefs != nullptr) { PromoteActiveToOverride(); m_prefs->crtByMode[ActiveModeIdx()].bloomEnabled       = on; }
    });
    m_displayPage->SetOnBloomRadiusChange ([this] (float px)
    {
        if (m_prefs != nullptr) { PromoteActiveToOverride(); m_prefs->crtByMode[ActiveModeIdx()].bloomRadius        = px; }
    });
    m_displayPage->SetOnBloomStrengthChange ([this] (float pct)
    {
        if (m_prefs != nullptr) { PromoteActiveToOverride(); m_prefs->crtByMode[ActiveModeIdx()].bloomStrength      = pct / 100.0f; }
    });
    m_displayPage->SetOnColorBleedEnChange ([this] (bool on)
    {
        if (m_prefs != nullptr) { PromoteActiveToOverride(); m_prefs->crtByMode[ActiveModeIdx()].colorBleedEnabled  = on; }
    });
    m_displayPage->SetOnColorBleedWChange ([this] (float px)
    {
        if (m_prefs != nullptr) { PromoteActiveToOverride(); m_prefs->crtByMode[ActiveModeIdx()].colorBleedWidth    = px; }
    });

    // Restore Defaults gives the user the RESOLVED defaults (theme
    // override layered on monitor preset) -- the same values the
    // "(theme default)" / "(monitor default)" badges refer to.
    m_displayPage->SetOnRestoreDefaults ([this] ()
    {
        ResetActiveToDefaults();
        ReseedFromActiveMode();

        // The CRT block above doesn't know about the Color-monitor text
        // colour, so also revert it to its White default across all three
        // views -- staged pref, the dropdown control, and the live emulator
        // -- otherwise a previously-picked colour survives the reset and the
        // controls and the emulator disagree (#8 follow-up).
        uint32_t  custom = (m_prefs != nullptr) ? m_prefs->colorMonitorTextCustomArgb
                                                : 0xFFFFFFFFu;
        if (m_prefs != nullptr)
        {
            m_prefs->colorMonitorTextMode = ColorMonitorTextMode::White;
        }
        if (m_displayPage != nullptr)
        {
            m_displayPage->SetTextColor (ColorMonitorTextMode::White, custom);
        }
        if (m_emuShell != nullptr)
        {
            m_emuShell->SetColorMonitorTextArgbLive (
                ColorUtil::ResolveColorMonitorTextArgb (ColorMonitorTextMode::White, custom));
        }
    });
}

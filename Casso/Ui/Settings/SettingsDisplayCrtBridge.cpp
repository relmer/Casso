#include "Pch.h"

#include "SettingsDisplayCrtBridge.h"

#include "DisplayPage.h"
#include "../ColorUtil.h"
#include "../ThemeManager.h"
#include "../../EmulatorShell.h"
#include "../../Config/CrtPresets.h"
#include "../../Config/CrtResolver.h"





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
//  GetActiveModeIdx
//
//  Returns the currently-selected monitor type as an index into
//  GlobalUserPrefs::crtByMode. Reads SettingsPanelState because the
//  monitor dropdown writes there as the source of truth; the live
//  shell state can lag by a frame.
//
////////////////////////////////////////////////////////////////////////////////

int SettingsDisplayCrtBridge::GetActiveModeIdx() const
{
    int  idx = (m_state != nullptr) ? (int) m_state->GetPrefs().colorMode : 0;



    // 0 is a real mode AND the fallback, so an out-of-range prefs value (a
    // config from a build with more monitor types) lands on the first one.
    if (idx < 0 || idx >= (int) kCrtModeCount)
    {
        idx = 0;
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
//  preset values (CrtPresets::GetPreset) so the user sees "what the
//  defaults are" rather than the still-zero in-struct defaults.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDisplayCrtBridge::ReseedFromActiveMode()
{
    GlobalUserPrefsCrtSnapshot  snap;
    int                         idx = GetActiveModeIdx();



    // With no prefs the page still gets a snapshot -- the default-constructed
    // one -- so its sliders are seeded rather than left at whatever they held.
    if (m_displayPage != nullptr && m_prefs == nullptr)
    {
        m_displayPage->SetInitialCrt (snap);
    }
    else if (m_displayPage != nullptr)
    {
        CrtResolved  r = ResolveActive();

        snap.brightness         = r.values.brightness;
        snap.contrast           = r.values.contrast;
        snap.gamma              = r.values.gamma;
        snap.persistence        = r.values.persistence;
        snap.scanlinesEnabled   = r.values.scanlinesEnabled;
        snap.scanlinesIntensity = r.values.scanlinesIntensity;
        snap.bloomEnabled       = r.values.bloomEnabled;
        snap.bloomRadius        = r.values.bloomRadius;
        snap.bloomStrength      = r.values.bloomStrength;
        snap.colorBleedEnabled  = r.values.colorBleedEnabled;
        snap.colorBleedWidth    = r.values.colorBleedWidth;

        m_displayPage->SetInitialCrt (snap);

        // Re-publish the per-control defaults hint so DisplayPage knows
        // which value counts as "the default" and can render the
        // (theme default) / (monitor default) badge in each row.
        PublishDefaultsHint();
    }
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

void SettingsDisplayCrtBridge::PublishDefaultsHint()
{
    DisplayDefaultsHint  hint;



    // Same shape as ReseedFromActiveMode: with no prefs the page still gets a
    // hint, just the default-constructed one.
    if (m_displayPage != nullptr && m_prefs == nullptr)
    {
        m_displayPage->SetDefaultsHint (hint);
    }
    else if (m_displayPage != nullptr)
    {
        int                       idx           = GetActiveModeIdx();
        const ThemeCrtDefaults *  themeDefaults = nullptr;
        CrtResolved               r;

        if (m_themes != nullptr && m_themes->GetActiveTheme() != nullptr)
        {
            // Resolved, like the renderer's: the base theme drops the
            // machine variant, so the badges would name values the picture
            // never used.
            themeDefaults = &m_themes->ActiveCrtDefaults();
        }

        // Deliberately resolved with NO user layer. This hint describes what
        // the user would get back by resetting, so it must not include what
        // they have set.
        r = CrtResolver::Resolve (CrtPresets::GetPreset ((size_t) idx), themeDefaults, CrtOverrides {});

        hint.values.brightness         = r.values.brightness;
        hint.values.contrast           = r.values.contrast;
        hint.values.gamma              = r.values.gamma;
        hint.values.persistence        = r.values.persistence;
        hint.values.scanlinesEnabled   = r.values.scanlinesEnabled;
        hint.values.scanlinesIntensity = r.values.scanlinesIntensity;
        hint.values.bloomEnabled       = r.values.bloomEnabled;
        hint.values.bloomRadius        = r.values.bloomRadius;
        hint.values.bloomStrength      = r.values.bloomStrength;
        hint.values.colorBleedEnabled  = r.values.colorBleedEnabled;
        hint.values.colorBleedWidth    = r.values.colorBleedWidth;

        hint.brightnessFromTheme = (r.source[(size_t) CrtField::Brightness]         == CrtSource::Theme);
        hint.contrastFromTheme   = (r.source[(size_t) CrtField::Contrast]           == CrtSource::Theme);
        hint.scanlinesFromTheme  = (r.source[(size_t) CrtField::ScanlinesIntensity] == CrtSource::Theme);
        hint.bloomFromTheme      = (r.source[(size_t) CrtField::BloomRadius]        == CrtSource::Theme);
        hint.colorBleedFromTheme = (r.source[(size_t) CrtField::ColorBleedWidth]    == CrtSource::Theme);

        m_displayPage->SetDefaultsHint (hint);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdoptThemeDefaults
//
//  A theme's `crtDefaults` are defaults, and picking a theme adopts them.
//  Every monitor's override flag is cleared, which hands the whole chain
//  back to MakeCrtParams: monitor preset, the new theme's layer on top.
//  The Display page is then reseeded so its sliders and its "(theme
//  default)" badges say what the picture is actually doing.
//
//  CLEARING THE FLAG, NOT COPYING VALUES. A block seeded by value would be
//  a user override wearing the theme's numbers, and the NEXT theme change
//  would find an override and decline to touch it -- which is the bug this
//  fixes, one theme later. The stale values left in the block are harmless:
//  PromoteActiveToOverride re-seeds from the resolved defaults before the
//  first edit lands, so a slider touched after a theme change starts from
//  the new theme, not from what the last one left behind.
//
//  Cancel gets this too, because reverting to the baseline theme raises the
//  same hook -- the CRT settings go back with the chrome rather than
//  stranding the reverted theme's picture under the abandoned one's numbers.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDisplayCrtBridge::AdoptThemeDefaults()
{
    // No write side. A theme change discards nothing the user set: their
    // per-field overrides simply sit on top of whatever the new theme
    // declares. This used to clear a flag on all four blocks, which threw
    // away every adjustment they had made.
    //
    // The sliders first, then the badges: both read the resolved chain, and
    // the page is only correct once they agree with it.
    ReseedFromActiveMode();
    PublishDefaultsHint();
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
//
//  ActiveOverrideKey
//
//  The key the active monitor and mode file their overrides under.
//
//  Taken from the shell's cache rather than resolved here, because resolving
//  a monitor re-reads and re-parses the machine JSON and this runs on every
//  slider drag.
//
////////////////////////////////////////////////////////////////////////////////

std::string SettingsDisplayCrtBridge::ActiveOverrideKey() const
{
    std::string  key;



    if (m_emuShell != nullptr)
    {
        key = m_emuShell->m_crtOverrideKeys[(size_t) GetActiveModeIdx()];
    }

    return key;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ResolveActive
//
//  What the active monitor and mode resolve to right now, including the
//  user's own overrides.
//
////////////////////////////////////////////////////////////////////////////////

CrtResolved SettingsDisplayCrtBridge::ResolveActive() const
{
    const ThemeCrtDefaults *  themeDefaults = nullptr;
    int                       idx           = GetActiveModeIdx();
    std::string               key           = ActiveOverrideKey();
    CrtOverrides              overrides;



    if (m_prefs != nullptr)
    {
        auto  found = m_prefs->crtOverrides.find (key);

        if (found != m_prefs->crtOverrides.end())
        {
            overrides = found->second;
        }
    }

    if (m_themes != nullptr && m_themes->GetActiveTheme() != nullptr)
    {
        themeDefaults = &m_themes->ActiveCrtDefaults();
    }

    return CrtResolver::Resolve (CrtPresets::GetPreset ((size_t) idx), themeDefaults, overrides);
}




////////////////////////////////////////////////////////////////////////////////
//
//  ResetActiveToDefaults
//
//  Restore Defaults: erase this monitor and mode's overrides so every field
//  follows the preset and theme chain again.
//
//  Nothing is recorded to express that they were removed. The old version
//  seeded the block with resolved values and then raised a flag, so restoring
//  defaults left the monitor marked as user-set and frozen against every
//  later theme change.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsDisplayCrtBridge::ResetActiveToDefaults()
{
    std::string  key = ActiveOverrideKey();



    if (m_prefs != nullptr)
    {
        m_prefs->crtOverrides.erase (key);
    }
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

void SettingsDisplayCrtBridge::WireDisplayPageCallbacks()
{
    if (m_displayPage == nullptr)
    {
        return;
    }

    // Brightness / contrast / gamma / persistence sliders write LIVE
    // to the currently-active monitor's CRT block. GetActiveModeIdx()
    // reads from SettingsPanelState so every edit lands on whichever
    // monitor type the user has selected in the dropdown.
    m_displayPage->SetOnBrightnessChange ([this] (float pct)
    {
        SetOverride (&CrtOverrides::brightness, pct / 100.0f);     // slider 0..200% -> shader 0..2.0
    });
    m_displayPage->SetOnContrastChange ([this] (float pct)
    {
        SetOverride (&CrtOverrides::contrast, pct / 100.0f);
    });
    m_displayPage->SetOnGammaChange ([this] (float g)
    {
        SetOverride (&CrtOverrides::gamma, g);
    });
    m_displayPage->SetOnPersistenceChange ([this] (float pct)
    {
        SetOverride (&CrtOverrides::persistence, pct / 100.0f);
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
        SetOverride (&CrtOverrides::scanlinesEnabled, on);
    });
    m_displayPage->SetOnScanlinesIntChange ([this] (float pct)
    {
        SetOverride (&CrtOverrides::scanlinesIntensity, pct / 100.0f);
    });
    m_displayPage->SetOnBloomEnChange ([this] (bool on)
    {
        SetOverride (&CrtOverrides::bloomEnabled, on);
    });
    m_displayPage->SetOnBloomRadiusChange ([this] (float px)
    {
        SetOverride (&CrtOverrides::bloomRadius, px);
    });
    m_displayPage->SetOnBloomStrengthChange ([this] (float pct)
    {
        SetOverride (&CrtOverrides::bloomStrength, pct / 100.0f);
    });
    m_displayPage->SetOnColorBleedEnChange ([this] (bool on)
    {
        SetOverride (&CrtOverrides::colorBleedEnabled, on);
    });
    m_displayPage->SetOnColorBleedWChange ([this] (float px)
    {
        SetOverride (&CrtOverrides::colorBleedWidth, px);
    });

    // Restore Defaults gives the user the RESOLVED defaults (theme
    // override layered on monitor preset) -- the same values the
    // "(theme default)" / "(monitor default)" badges refer to.
    m_displayPage->SetOnRestoreDefaults ([this] ()
    {
        ResetActiveToDefaults();
        ReseedFromActiveMode();

        // The CRT block above doesn't know about the Color-monitor text
        // color, so also revert it to its White default across all three
        // views -- staged pref, the dropdown control, and the live emulator
        // -- otherwise a previously-picked color survives the reset and the
        // controls and the emulator disagree (#8 follow-up).
        uint32_t  custom = (m_prefs != nullptr) ? m_prefs->colorMonitorTextCustomArgb
                                                : 0xFFFFFFFFu;
        if (m_prefs != nullptr)
        {
            m_prefs->ResetColorMonitorTextToDefault();
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

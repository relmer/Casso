#include "Pch.h"

#include "CrtResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-local constants
//
////////////////////////////////////////////////////////////////////////////////

// The second segment of an override key, indexed by SettingsColorMode. These
// are the same four tokens v1 files already carry as their crt sub-object
// keys, reused rather than re-invented so a migrated key reads the way the
// old one did.
static constexpr std::string_view  s_kModeTokens[kCrtModeCount] = {
    "color", "green", "amber", "white"
};


// Separates the two segments. Below every lowercase letter in ASCII, which is
// why every "AppleMonitorII/..." key sorts ahead of every
// "AppleMonitorIIc/..." key despite the shared prefix.
static constexpr char  s_kKeySeparator = '/';





////////////////////////////////////////////////////////////////////////////////
//
//  CrtResolver::ModeToken
//
//  The token a mode index contributes to an override key.
//
////////////////////////////////////////////////////////////////////////////////

std::string_view CrtResolver::ModeToken (size_t modeIndex)
{
    size_t  clamped = (modeIndex < kCrtModeCount) ? modeIndex : 0;



    return s_kModeTokens[clamped];
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrtResolver::MakeKey
//
//  Joins a resolved monitor identifier and a mode into the key its overrides
//  are stored under.
//
////////////////////////////////////////////////////////////////////////////////

std::string CrtResolver::MakeKey (
    std::string_view  monitorConfigName,
    size_t            modeIndex)
{
    std::string       key;
    std::string_view  token = ModeToken (modeIndex);



    key.reserve (monitorConfigName.size() + 1 + token.size());
    key.append (monitorConfigName);
    key.push_back (s_kKeySeparator);
    key.append (token);

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrtResolver::Resolve
//
//  Layers the three tiers into one set of values, recording which tier
//  supplied each one.
//
//  Per field, and independently: the preset, then the theme where the theme
//  declares that field's GROUP, then the user where the user has set that
//  FIELD. The asymmetry is deliberate and is what lets a theme's intent
//  survive one adjustment: a theme fills a whole group at once because
//  ThemeLoader reads it from one JSON object, while a user override never
//  touches a field it was not applied to.
//
//  Gamma and persistence have no theme group at all, so they only ever
//  report Preset or User. A caller that offers a theme-default label for
//  either is describing a tier that cannot exist.
//
////////////////////////////////////////////////////////////////////////////////

CrtResolved CrtResolver::Resolve (
    const CrtValues        &  preset,
    const ThemeCrtDefaults *  themeDefaults,
    const CrtOverrides     &  overrides)
{
    CrtResolved  resolved;



    resolved.values = preset;

    for (size_t i = 0; i < (size_t) CrtField::Count; i++)
    {
        resolved.source[i] = CrtSource::Preset;
    }

    // Theme tier, per group, only where the theme actually declared one.
    if (themeDefaults != nullptr)
    {
        if (themeDefaults->hasBrightness)
        {
            resolved.values.brightness = themeDefaults->brightness;
            resolved.source[(size_t) CrtField::Brightness] = CrtSource::Theme;
        }

        if (themeDefaults->hasContrast)
        {
            resolved.values.contrast = themeDefaults->contrast;
            resolved.source[(size_t) CrtField::Contrast] = CrtSource::Theme;
        }

        if (themeDefaults->hasScanlines)
        {
            resolved.values.scanlinesEnabled   = themeDefaults->scanlinesEnabled;
            resolved.values.scanlinesIntensity = themeDefaults->scanlinesIntensity;
            resolved.source[(size_t) CrtField::ScanlinesEnabled]   = CrtSource::Theme;
            resolved.source[(size_t) CrtField::ScanlinesIntensity] = CrtSource::Theme;
        }

        if (themeDefaults->hasBloom)
        {
            resolved.values.bloomEnabled  = themeDefaults->bloomEnabled;
            resolved.values.bloomRadius   = themeDefaults->bloomRadius;
            resolved.values.bloomStrength = themeDefaults->bloomStrength;
            resolved.source[(size_t) CrtField::BloomEnabled]  = CrtSource::Theme;
            resolved.source[(size_t) CrtField::BloomRadius]   = CrtSource::Theme;
            resolved.source[(size_t) CrtField::BloomStrength] = CrtSource::Theme;
        }

        if (themeDefaults->hasColorBleed)
        {
            resolved.values.colorBleedEnabled = themeDefaults->colorBleedEnabled;
            resolved.values.colorBleedWidth   = themeDefaults->colorBleedWidth;
            resolved.source[(size_t) CrtField::ColorBleedEnabled] = CrtSource::Theme;
            resolved.source[(size_t) CrtField::ColorBleedWidth]   = CrtSource::Theme;
        }
    }

    // User tier, per field. Never a group, and never a seeded sibling.
    if (overrides.brightness.has_value())
    {
        resolved.values.brightness = overrides.brightness.value();
        resolved.source[(size_t) CrtField::Brightness] = CrtSource::User;
    }

    if (overrides.contrast.has_value())
    {
        resolved.values.contrast = overrides.contrast.value();
        resolved.source[(size_t) CrtField::Contrast] = CrtSource::User;
    }

    if (overrides.gamma.has_value())
    {
        resolved.values.gamma = overrides.gamma.value();
        resolved.source[(size_t) CrtField::Gamma] = CrtSource::User;
    }

    if (overrides.scanlinesEnabled.has_value())
    {
        resolved.values.scanlinesEnabled = overrides.scanlinesEnabled.value();
        resolved.source[(size_t) CrtField::ScanlinesEnabled] = CrtSource::User;
    }

    if (overrides.scanlinesIntensity.has_value())
    {
        resolved.values.scanlinesIntensity = overrides.scanlinesIntensity.value();
        resolved.source[(size_t) CrtField::ScanlinesIntensity] = CrtSource::User;
    }

    if (overrides.bloomEnabled.has_value())
    {
        resolved.values.bloomEnabled = overrides.bloomEnabled.value();
        resolved.source[(size_t) CrtField::BloomEnabled] = CrtSource::User;
    }

    if (overrides.bloomRadius.has_value())
    {
        resolved.values.bloomRadius = overrides.bloomRadius.value();
        resolved.source[(size_t) CrtField::BloomRadius] = CrtSource::User;
    }

    if (overrides.bloomStrength.has_value())
    {
        resolved.values.bloomStrength = overrides.bloomStrength.value();
        resolved.source[(size_t) CrtField::BloomStrength] = CrtSource::User;
    }

    if (overrides.colorBleedEnabled.has_value())
    {
        resolved.values.colorBleedEnabled = overrides.colorBleedEnabled.value();
        resolved.source[(size_t) CrtField::ColorBleedEnabled] = CrtSource::User;
    }

    if (overrides.colorBleedWidth.has_value())
    {
        resolved.values.colorBleedWidth = overrides.colorBleedWidth.value();
        resolved.source[(size_t) CrtField::ColorBleedWidth] = CrtSource::User;
    }

    if (overrides.persistence.has_value())
    {
        resolved.values.persistence = overrides.persistence.value();
        resolved.source[(size_t) CrtField::Persistence] = CrtSource::User;
    }

    return resolved;
}

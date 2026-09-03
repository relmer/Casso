#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CrtTypes
//
//  The CRT picture vocabulary, shared by every tier that has an opinion about
//  the picture: the per-monitor preset, a theme's declared defaults, and the
//  user's own adjustments.
//
//  This header carries types and nothing else. It includes only Pch.h, so a
//  prefs header and a settings widget header can both take it without either
//  dragging in the other, and without pulling the theme loader into Config.
//
//  Why the user layer is SPARSE. It used to be one boolean per monitor: touch
//  any control and the whole block was snapshotted and applied verbatim, so a
//  user who nudged brightness stopped receiving every later theme change and
//  preset retune for that monitor, on all eleven values, with nothing to say
//  so. An absent optional here means "no opinion", which is not the same as
//  any value, and it is what lets one adjustment sit on top of a chain that
//  keeps moving underneath it.
//
//  Note the asymmetry with the theme side, which is deliberate. A theme group
//  applies as a unit, because ThemeLoader fills a whole group from one JSON
//  object. A user override is per field and never atomic. That is what lets a
//  theme's intent survive a single adjustment to one of its values.
//
////////////////////////////////////////////////////////////////////////////////

// A complete set of CRT picture values. The element type of the preset table
// and the resolved output of the layering chain.
struct CrtValues
{
    float    brightness          = 1.0f;           // 0.0 .. 2.0
    float    contrast            = 1.0f;           // 0.0 .. 2.0
    float    gamma               = 1.0f;           // 0.5 .. 2.5 (final pow(rgb, 1/gamma)); 1.0 = bypass
    bool     scanlinesEnabled    = false;
    float    scanlinesIntensity  = 0.5f;           // 0.0 .. 1.0
    bool     bloomEnabled        = false;
    float    bloomRadius         = 1.0f;           // 0.0 .. 4.0 (emulated pixels)
    float    bloomStrength       = 0.5f;           // 0.0 .. 1.0
    bool     colorBleedEnabled   = false;
    float    colorBleedWidth     = 1.0f;           // 0.0 .. 8.0 (emulated pixels)
    float    persistence         = 0.0f;           // 0.0 .. 0.99 (phosphor decay factor)
};


// Which value a per-field answer is about. Provenance is reported per field
// and the Display page walks its rows in this order, so both need one shared
// index rather than a private numbering each.
enum class CrtField : uint8_t
{
    Brightness = 0,
    Contrast,
    Gamma,
    ScanlinesEnabled,
    ScanlinesIntensity,
    BloomEnabled,
    BloomRadius,
    BloomStrength,
    ColorBleedEnabled,
    ColorBleedWidth,
    Persistence,
    Count
};


// Which tier supplied a resolved value. This is what the value came FROM,
// never a comparison of numbers: a user's value that happens to equal the
// preset still reports User, which is the case the old value-comparison
// badge got wrong.
enum class CrtSource : uint8_t
{
    Preset,
    Theme,
    User
};


// What the user has deliberately changed for one monitor and color mode.
// Absent means no opinion, so that field keeps following the preset and
// theme tiers.
struct CrtOverrides
{
    std::optional<float>  brightness;
    std::optional<float>  contrast;
    std::optional<float>  gamma;
    std::optional<bool>   scanlinesEnabled;
    std::optional<float>  scanlinesIntensity;
    std::optional<bool>   bloomEnabled;
    std::optional<float>  bloomRadius;
    std::optional<float>  bloomStrength;
    std::optional<bool>   colorBleedEnabled;
    std::optional<float>  colorBleedWidth;
    std::optional<float>  persistence;

    // Required rather than decorative. Cancel restores by comparing a
    // snapshot map against the live one, and std::map's equality needs the
    // mapped type to have its own.
    bool  operator== (const CrtOverrides &) const = default;

    bool  IsEmpty () const
    {
        return !brightness.has_value()
            && !contrast.has_value()
            && !gamma.has_value()
            && !scanlinesEnabled.has_value()
            && !scanlinesIntensity.has_value()
            && !bloomEnabled.has_value()
            && !bloomRadius.has_value()
            && !bloomStrength.has_value()
            && !colorBleedEnabled.has_value()
            && !colorBleedWidth.has_value()
            && !persistence.has_value();
    }
};


// Resolved values with the tier each one came from, indexed by CrtField.
struct CrtResolved
{
    CrtValues  values;
    CrtSource  source[(size_t) CrtField::Count] = {};
};

#pragma once

#include "Pch.h"

#include "CrtTypes.h"

#include "../Ui/ThemeLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CrtResolver
//
//  THE ONE PLACE the preset, theme and user tiers combine. Every consumer
//  calls this; no consumer restates it.
//
//  That is the whole point of the file. The layering chain used to be written
//  four times: once in CrtPostProcess, which the UnitTest project compiles,
//  and three more times in the settings bridge, which it does not. The three
//  untested copies drifted from the tested one twice, and both drifts shipped.
//
//  The preset arrives already selected, as a value rather than a mode index,
//  so the resolver never consults the preset table. That is what keeps the
//  later work giving MonitorSpec its own CRT parameters a caller change: when
//  a preset becomes a composition of tube geometry and phosphor chemistry,
//  nothing here moves.
//
//  The resolver takes no geometry. Output size, pixel scale and the picture
//  rectangle are properties of the frame rather than preferences, and
//  CrtPostProcess adds them when it projects a CrtResolved into the shader
//  constant buffer.
//
////////////////////////////////////////////////////////////////////////////////

namespace CrtResolver
{

    // Resolve one monitor and mode's picture, reporting where each value
    // came from. `themeDefaults` may be null, which reads as a theme that
    // declares nothing.
    CrtResolved  Resolve (const CrtValues        &  preset,
                          const ThemeCrtDefaults *  themeDefaults,
                          const CrtOverrides     &  overrides);


    // The key a monitor and mode's overrides are stored under. The format is
    // a contract rather than a formatting detail, so it lives here where a
    // test reaches it: it carries the freeze on shipped monitor identifiers,
    // the sorted order the prefs file depends on, and the guarantee that no
    // two monitor and mode pairs collide.
    //
    // `monitorConfigName` must be an already-resolved MonitorSpec::configName
    // and never a raw string out of a machine's JSON, because ByName recovers
    // an unrecognized name to the default without saying so.
    //
    // `modeIndex` is a SettingsColorMode ordinal (Color=0, Green=1, Amber=2,
    // White=3). Taken as an index rather than the enum so this header stays
    // free of UI-layer dependencies, the same way GlobalUserPrefs does it.
    std::string  MakeKey (std::string_view monitorConfigName, size_t modeIndex);


    // The mode token a key's second segment carries, for callers that build
    // or read keys. Out-of-range yields the Color token.
    std::string_view  ModeToken (size_t modeIndex);
}

#pragma once

#include "Pch.h"

#include "GlobalUserPrefs.h"




////////////////////////////////////////////////////////////////////////////////
//
//  CrtPresets
//
//  Per-monitor-type "looks like the real thing" baseline values.
//  Picked from photographs / hardware references for each phosphor
//  type so the first time a user switches to (say) Amber they get a
//  visually correct P3 phosphor preset instead of identity values
//  layered over a color-tuned default.
//
//  Layering: when GlobalUserPrefs::crtByMode[N].userOverride is false,
//  CrtPostProcess::MakeCrtParams seeds from CrtPresets::ForMode(N)
//  before applying any theme variant overrides on top.
//
////////////////////////////////////////////////////////////////////////////////

namespace CrtPresets
{
    // Index by SettingsColorMode (Color=0, Green=1, Amber=2, White=3).
    // Each entry mirrors GlobalUserPrefs::Crt minus the userOverride
    // flag (presets are by definition NOT user-overridden).
    inline const GlobalUserPrefs::Crt &  ForMode (size_t modeIndex)
    {
        // Color: NTSC composite color, modest bloom + the chroma bleed
        //   that defines the Apple II color-mode look. No scanlines /
        //   persistence -- color CRTs used P22 triads with ~30us decay.
        // Green (P1): short decay, faint green afterglow, distinct
        //   scanline visibility -- the classic computer-terminal vibe.
        // Amber (P3): slow decay -> high persistence; visible scanlines
        //   + soft bloom + warmer gamma. The look of a 1980s amber
        //   monitor in a dim office.
        // White (P4): blend of blue + yellow phosphors, ~60ms decay;
        //   subtler look than the mono colors, slight bloom.
        static const GlobalUserPrefs::Crt  s_kPresets[GlobalUserPrefs::kCrtModeCount] = {
            // Color
            {
                /* brightness         */ 1.00f,
                /* contrast           */ 1.00f,
                /* gamma              */ 2.20f,
                /* scanlinesEnabled   */ false,
                /* scanlinesIntensity */ 0.30f,
                /* bloomEnabled       */ true,
                /* bloomRadius        */ 2.0f,
                /* bloomStrength      */ 0.30f,
                /* colorBleedEnabled  */ true,
                /* colorBleedWidth    */ 3.0f,
                /* persistence        */ 0.00f,
                /* userOverride       */ false,
            },
            // Green (P1)
            {
                /* brightness         */ 1.05f,
                /* contrast           */ 0.95f,
                /* gamma              */ 1.80f,
                /* scanlinesEnabled   */ true,
                /* scanlinesIntensity */ 0.40f,
                /* bloomEnabled       */ true,
                /* bloomRadius        */ 3.0f,
                /* bloomStrength      */ 0.40f,
                /* colorBleedEnabled  */ false,
                /* colorBleedWidth    */ 0.0f,
                /* persistence        */ 0.55f,
                /* userOverride       */ false,
            },
            // Amber (P3)
            {
                /* brightness         */ 1.05f,
                /* contrast           */ 0.90f,
                /* gamma              */ 1.80f,
                /* scanlinesEnabled   */ true,
                /* scanlinesIntensity */ 0.50f,
                /* bloomEnabled       */ true,
                /* bloomRadius        */ 2.0f,
                /* bloomStrength      */ 0.35f,
                /* colorBleedEnabled  */ false,
                /* colorBleedWidth    */ 0.0f,
                /* persistence        */ 0.80f,
                /* userOverride       */ false,
            },
            // White (P4)
            {
                /* brightness         */ 1.00f,
                /* contrast           */ 1.00f,
                /* gamma              */ 1.90f,
                /* scanlinesEnabled   */ true,
                /* scanlinesIntensity */ 0.35f,
                /* bloomEnabled       */ true,
                /* bloomRadius        */ 1.0f,
                /* bloomStrength      */ 0.20f,
                /* colorBleedEnabled  */ false,
                /* colorBleedWidth    */ 0.0f,
                /* persistence        */ 0.30f,
                /* userOverride       */ false,
            },
        };

        size_t  clamped = (modeIndex < GlobalUserPrefs::kCrtModeCount) ? modeIndex : 0;
        return s_kPresets[clamped];
    }
}

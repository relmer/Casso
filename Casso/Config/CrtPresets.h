#pragma once

#include "Pch.h"

#include "CrtTypes.h"





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
//  Layering: CrtResolver::Resolve starts from GetPreset(N), lays the
//  active theme's declared groups over it, and lets the user's own
//  per-field overrides win last.
//
////////////////////////////////////////////////////////////////////////////////

namespace CrtPresets
{
    // Index by SettingsColorMode (Color=0, Green=1, Amber=2, White=3).
    inline const CrtValues &  GetPreset (size_t modeIndex)
    {
        // Color: NTSC composite color, modest bloom + the chroma bleed
        //   that defines the Apple II color-mode look. No scanlines /
        //   persistence -- color CRTs used P22 triads with ~30us decay.
        // Green (P1): short decay, faint green afterglow, distinct
        //   scanline visibility -- the classic computer-terminal vibe.
        // Amber (P3): slow decay -> high persistence; visible scanlines
        //   + soft bloom. The look of a 1980s amber monitor in a dim
        //   office.
        // White (P4): blend of blue + yellow phosphors, ~60ms decay;
        //   subtler look than the mono colors, slight bloom.
        //
        // Gamma defaults to 1.0 (true bypass) for every mode. Physical
        // CRT gamma is ~2.2-2.5 regardless of phosphor type, set by
        // the electron gun -- so it isn't what distinguished an amber
        // tube from a green tube from a color CRT in 1985. The things
        // that DID distinguish them (hue, persistence, scanlines, bloom)
        // each have their own knob below. Layering an opinionated
        // gamma on top of the host display's already-calibrated curve
        // just produces a stack the user never asked for; treat the
        // gamma slider as a user-customization lever instead.
        static const CrtValues  s_kPresets[kCrtModeCount] = {
            // Color
            {
                /* brightness         */ 1.00f,
                /* contrast           */ 1.00f,
                /* gamma              */ 1.00f,
                /* scanlinesEnabled   */ true,
                /* scanlinesIntensity */ 0.20f,
                /* bloomEnabled       */ true,
                /* bloomRadius        */ 1.0f,
                /* bloomStrength      */ 0.20f,
                /* colorBleedEnabled  */ true,
                /* colorBleedWidth    */ 3.0f,
                /* persistence        */ 0.00f,
            },
            // Green (P1)
            {
                /* brightness         */ 1.05f,
                /* contrast           */ 1.00f,
                /* gamma              */ 1.00f,
                /* scanlinesEnabled   */ true,
                /* scanlinesIntensity */ 0.40f,
                /* bloomEnabled       */ true,
                /* bloomRadius        */ 3.0f,
                /* bloomStrength      */ 0.40f,
                /* colorBleedEnabled  */ false,
                /* colorBleedWidth    */ 0.0f,
                /* persistence        */ 0.55f,
            },
            // Amber (P3)
            {
                /* brightness         */ 1.05f,
                /* contrast           */ 1.00f,
                /* gamma              */ 1.00f,
                /* scanlinesEnabled   */ true,
                /* scanlinesIntensity */ 0.50f,
                /* bloomEnabled       */ true,
                /* bloomRadius        */ 2.0f,
                /* bloomStrength      */ 0.35f,
                /* colorBleedEnabled  */ false,
                /* colorBleedWidth    */ 0.0f,
                /* persistence        */ 0.80f,
            },
            // White (P4)
            {
                /* brightness         */ 1.00f,
                /* contrast           */ 1.00f,
                /* gamma              */ 1.00f,
                /* scanlinesEnabled   */ true,
                /* scanlinesIntensity */ 0.35f,
                /* bloomEnabled       */ true,
                /* bloomRadius        */ 1.0f,
                /* bloomStrength      */ 0.20f,
                /* colorBleedEnabled  */ false,
                /* colorBleedWidth    */ 0.0f,
                /* persistence        */ 0.30f,
            },
        };

        size_t  clamped = (modeIndex < kCrtModeCount) ? modeIndex : 0;
        return s_kPresets[clamped];
    }
}

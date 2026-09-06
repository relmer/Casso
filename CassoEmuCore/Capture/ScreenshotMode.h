#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotMode
//
//  What a screenshot contains. Three different jobs -- showing off the desk
//  scene, reporting a CRT render fault, and extracting pixel-exact artwork --
//  want three different pictures, and no one of them serves the other two.
//
//  Scene   the scene viewport as rendered: desk, glass, monitor, drives.
//  Crt     the picture with the CRT chain applied and nothing around it.
//  Raw     the emulated framebuffer at 560x384, no CRT processing. The
//          monitor tint IS present -- it is applied in the framebuffer, so
//          "raw" means no CRT chain, not no monitor character.
//
////////////////////////////////////////////////////////////////////////////////

enum class ScreenshotMode
{
    Scene = 0,
    Crt,
    Raw,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotModeToken
//
//  The persisted spelling of a ScreenshotMode.
//
//  A MODE IS STORED AS A TOKEN, NEVER AS AN INDEX. The stored value outlives
//  the order the modes happen to be declared in, and an index would silently
//  repoint every existing user's setting the first time that order changed,
//  with nothing to diagnose. Same reason printDotStyle stores "ink" and
//  audioDownloadConsent stores "ask".
//
//  A SHIPPED TOKEN IS FROZEN. It is a value already written into users' prefs
//  files, so renaming one orphans their setting.
//
////////////////////////////////////////////////////////////////////////////////

class ScreenshotModeToken
{
public:
    // An unrecognized or empty token yields the default (Scene) rather than
    // failing: a prefs file written by a newer build naming a mode this one
    // does not have is forward compatibility, not user error.
    static ScreenshotMode  Parse  (const string & token);

    // Round-trips Parse exactly. Never returns nullptr.
    static const char *    Format (ScreenshotMode mode);

    static constexpr ScreenshotMode  kDefault = ScreenshotMode::Scene;
};

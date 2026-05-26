#pragma once

#include "Pch.h"

#include "IFileSystem.h"

#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs
//
//  Singleton (in the lifetime sense, not the global-state sense) of all
//  non-machine-specific user preferences. Mirrors the JSON shape defined
//  by `specs/007-ui-overhaul/contracts/global-user-prefs.schema.json`.
//
//  File location:  <assetBaseDir>/GlobalUserPrefs.json
//
//  Missing-field tolerance: any absent leaf falls back to its in-struct
//  default. Unknown fields are preserved and re-emitted on Save so user
//  edits made by a newer Casso build aren't silently dropped by an
//  older build.
//
////////////////////////////////////////////////////////////////////////////////

struct GlobalUserPrefs
{
    int          version = 1;                          // $cassoGlobalPrefsVersion

    std::string  activeTheme         = "Skeuomorphic"; // FR-030 default
    std::string  lastSelectedMachine;                  // empty == none

    // `userOverride` gates whether theme CRT defaults still apply.
    struct Crt
    {
        float    brightness          = 1.0f;           // 0.0 .. 2.0
        float    contrast            = 1.0f;           // 0.0 .. 2.0
        bool     scanlinesEnabled    = false;
        float    scanlinesIntensity  = 0.5f;           // 0.0 .. 1.0
        bool     bloomEnabled        = false;
        float    bloomRadius         = 1.0f;           // 0.0 .. 10.0 (output pixels)
        float    bloomStrength       = 0.5f;           // 0.0 .. 1.0
        bool     colorBleedEnabled   = false;
        float    colorBleedWidth     = 1.0f;           // 0.0 .. 8.0 (output pixels)
        bool     userOverride        = false;
    };

    Crt          crt;

    struct
    {
        bool     fHaveLastBounds = false;
        int      x               = 0;
        int      y               = 0;
        int      w               = 1024;
        int      h               = 768;
        bool     fullscreen      = false;
    } window;

    // Unknown JSON keys round-trip back to disk untouched.
    std::vector<std::pair<std::string, JsonValue>>  unknownPassthrough;

    // ---- Persistence ---------------------------------------------------

    HRESULT     Load     (const std::wstring & baseDir,
                          IFileSystem        & fs);
    HRESULT     Save     (const std::wstring & baseDir,
                          IFileSystem        & fs) const;

    // ---- Serialization (exposed for tests) -----------------------------

    JsonValue   ToJson   () const;
    HRESULT     FromJson (const JsonValue & v);

    static std::wstring  FilePath (const std::wstring & baseDir);
};

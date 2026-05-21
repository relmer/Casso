#pragma once

#include "Pch.h"

#include "IFileSystem.h"

#include "../../CassoEmuCore/Core/JsonValue.h"





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

    struct
    {
        float    brightness          = 1.0f;           // 0.0 .. 2.0
        bool     scanlinesEnabled    = false;
        float    scanlinesIntensity  = 0.5f;           // 0.0 .. 1.0
        bool     bloomEnabled        = false;
        float    bloomRadius         = 1.0f;           // 0.0 .. 4.0
        float    bloomStrength       = 0.5f;           // 0.0 .. 1.0
        bool     colorBleedEnabled   = false;
        float    colorBleedWidth     = 1.0f;           // 0.0 .. 4.0
    } crt;

    struct
    {
        bool     fHaveLastBounds     = false;
        int      x                   = 0;
        int      y                   = 0;
        int      w                   = 1024;
        int      h                   = 768;
        bool     fullscreen          = false;
    } window;

    // Unknown JSON keys we should round-trip back to disk untouched.
    // Stored as a parsed JsonValue per key.
    std::vector<std::pair<std::string, JsonValue>>  unknownPassthrough;

    // ---- Persistence ---------------------------------------------------

    // Read GlobalUserPrefs.json under `baseDir`. If absent, leaves
    // `*this` at struct defaults and returns S_FALSE (caller may
    // treat as "first run").
    HRESULT Load (
        const std::wstring  & baseDir,
        IFileSystem         & fs);

    // Atomically write GlobalUserPrefs.json under `baseDir`.
    HRESULT Save (
        const std::wstring  & baseDir,
        IFileSystem         & fs) const;

    // ---- Serialization (exposed for tests) -----------------------------

    JsonValue   ToJson () const;
    HRESULT     FromJson (const JsonValue & v);

    static std::wstring  FilePath (const std::wstring & baseDir);
};

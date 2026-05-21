#pragma once

#include "Pch.h"

#include "Config/IFileSystem.h"

#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeLoader
//
//  Pure-logic theme parser + validator. Operates exclusively through
//  IFileSystem so the unit test harness can hand it a synthetic
//  Themes/ tree.
//
//  Two responsibilities:
//      1. EnumerateCandidateDirs (themesBaseDir) — returns every
//         sub-directory of `themesBaseDir` that contains a `theme.json`.
//         Subdirs without one (e.g. `_shared/`) are skipped silently.
//      2. Load (dir) — parse + validate `<dir>/theme.json`, resolve
//         per-element `.rml` paths (entryDocuments fall back to
//         `Themes/_shared/<entry>.rml`), and return a `LoadedTheme`
//         struct OR a structured `ThemeLoadError` with the failing
//         file path + a human-readable message. NO C++ exceptions
//         cross the API boundary.
//
//  `ThemeLoader` knows nothing about RmlUi or Mica. Activation hooks
//  live one layer up in `ThemeManager`.
//
////////////////////////////////////////////////////////////////////////////////

enum class ThemeLoadResult
{
    Success,
    MetadataMissing,           // theme.json does not exist
    MetadataInvalid,           // theme.json failed to parse / missing required fields
    DocumentMissing,           // an entryDocuments path resolves to a missing file
    VersionTooNew,             // $cassoThemeVersion > kCurrentThemeSchemaVersion
    UnknownError
};


struct ThemeCrtDefaults
{
    float    brightness          = 1.0f;
    bool     scanlinesEnabled    = false;
    float    scanlinesIntensity  = 0.5f;
    bool     bloomEnabled        = false;
    float    bloomRadius         = 1.0f;
    float    bloomStrength       = 0.5f;
    bool     colorBleedEnabled   = false;
    float    colorBleedWidth     = 1.0f;
};


struct ThemeEntryDocuments
{
    // Absolute paths, resolved against either the theme directory or
    // `Themes/_shared/`. Empty wstring == not declared and no shared
    // fallback was provided either.
    std::wstring  titleBar;
    std::wstring  navLayer;
    std::wstring  settings;
    std::wstring  driveWidgets;
};


struct LoadedTheme
{
    std::string         name;
    std::string         author;
    std::string         description;
    int                 version              = 0;   // $cassoThemeVersion
    bool                isBuiltIn            = false; // $cassoBuiltIn
    bool                useMicaBackdrop      = false;

    std::wstring        directoryPath;               // absolute, no trailing sep
    ThemeEntryDocuments entryDocs;
    ThemeCrtDefaults    crtDefaults;
};


struct ThemeLoadError
{
    ThemeLoadResult  code           = ThemeLoadResult::UnknownError;
    std::wstring     themeDir;
    std::wstring     offendingPath; // empty unless DocumentMissing / file-level
    std::string      message;
    int              jsonLine       = 0;
    int              jsonColumn     = 0;
};


class ThemeLoader
{
public:
    // Bump when the schema gains a new required field. Themes whose
    // $cassoThemeVersion is *lower* than this are subject to in-place
    // upgrade by AssetBootstrap::EnsureThemes (built-ins) or by the
    // ThemeManager (user-authored, future work). Themes whose
    // $cassoThemeVersion is *higher* are rejected so an older Casso
    // build doesn't silently mis-render a newer theme.
    static constexpr int  kCurrentThemeSchemaVersion = 1;


    static HRESULT      EnumerateCandidateDirs (IFileSystem                & fs,
                                                const std::wstring         & themesBaseDir,
                                                std::vector<std::wstring>  & outNames);
    static HRESULT      Load                   (IFileSystem                & fs,
                                                const std::wstring         & themeDir,
                                                const std::wstring         & sharedDir,
                                                LoadedTheme                & outTheme,
                                                ThemeLoadError             & outError);


    // ---- Pure helpers (exposed for testing) ----------------------------

    static HRESULT      ParseMetadata          (const std::string          & jsonText,
                                                LoadedTheme                & outTheme,
                                                ThemeLoadError             & outError);
    static HRESULT      ResolveEntryDocs       (IFileSystem                & fs,
                                                const std::wstring         & themeDir,
                                                const std::wstring         & sharedDir,
                                                LoadedTheme                & ioTheme,
                                                ThemeLoadError             & outError);
    static std::wstring JoinPath               (const std::wstring         & dir,
                                                const std::wstring         & leaf);
};

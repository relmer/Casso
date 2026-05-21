// Contract: Casso/Ui/ThemeManager.h
//
// This file documents the public surface of the ThemeManager. The
// implementation lives in Casso/Ui/ThemeManager.{h,cpp} and MUST satisfy
// every signature and invariant declared here.

#pragma once

// In real code, all includes route through Pch.h.

namespace Casso::Ui
{
    struct ThemeInfo
    {
        std::string  name;            // matches directory name
        std::string  author;
        std::string  description;
        int          version;         // $cassoThemeVersion
        std::string  directoryPath;   // absolute, resolved at Discover()
        bool         useMicaBackdrop;
    };

    enum class ThemeLoadResult
    {
        Success,
        MetadataMissing,
        MetadataInvalid,
        DocumentMissing,
        StylesheetParseError,
        AssetMissing,
        VersionTooNew,
        UnknownError
    };


    //
    // Discovery + activation
    //

    class ThemeManager
    {
    public:
        // Construct with an injectable IFileSystem so tests can use an
        // in-memory FS and skip Themes/ scanning on disk.
        ThemeManager (Config::IFileSystem & fs, Rml::Context * pRmlContext);

        // Scan the Themes/ directory; populate the cached list of valid
        // ThemeInfos. Invalid themes are logged + skipped (FR-036) and
        // EXCLUDED from the returned list. Safe to call repeatedly
        // (cheap; supports FR-035 "discover without restart" via the
        // Settings panel's Refresh button).
        HRESULT Discover ();

        // Returns the current discovery snapshot.
        const std::vector<ThemeInfo> & GetAvailableThemes () const;

        // Activate the named theme. On success:
        //   - The previous theme's documents and stylesheets are unloaded.
        //   - The new theme's entryDocuments are loaded into pRmlContext.
        //   - OnThemeChanged listeners are notified.
        //   - The theme's crtDefaults are applied to GlobalUserPrefs only
        //     for fields the user has not explicitly overridden.
        // Returns S_FALSE if the name doesn't match any discovered theme.
        // Returns failure HRESULT (with structured error logged) if the
        // load itself fails, in which case the previous theme remains
        // active (FR-036 — no crash, no corruption).
        HRESULT Activate (const std::string & themeName);

        // Reload the currently active theme from disk (dev-iteration aid).
        HRESULT ReloadCurrent ();

        // Returns the name of the currently active theme, or empty string
        // if no theme is active yet.
        const std::string & GetActiveThemeName () const;

        // Event hook. Listeners are called on the UI thread immediately
        // after a successful Activate() or ReloadCurrent(). Listeners
        // MUST NOT block.
        using ChangeListener = std::function<void (const ThemeInfo &)>;
        void  AddChangeListener (ChangeListener listener);

    private:
        // ... implementation details ...
    };
}


//
// INVARIANTS
//
// * The active theme is ALWAYS valid. If activation fails, the previous
//   valid theme remains in place.
// * No file I/O bypasses IFileSystem. (Constitution §II.)
// * No method throws C++ exceptions across the API boundary. Internal
//   exceptions from RmlUi MUST be caught and converted to HRESULT.
// * Activate() is callable from the UI thread only. The CPU/emulation
//   thread MUST NOT call into ThemeManager.
// * ReloadCurrent() is a dev-only convenience; in retail builds it is
//   still safe to call but its triggering UI may be conditionally compiled.

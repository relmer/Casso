#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"
#include "IFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs
//
//  Singleton (in the lifetime sense, not the global-state sense) of all
//  non-machine-specific user preferences. Mirrors the global section of
//  the unified user preferences JSON persisted by UserConfigStore.
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

    // Disk II audio asset download consent. Tri-state string:
    //   "ask"     -- user has never been prompted (default)
    //   "allow"   -- silently re-fetch missing audio assets
    //   "decline" -- skip the prompt, leave audio assets missing
    // AssetBootstrap::CheckAndFetchDiskAudio reads + writes this.
    std::string  audioDownloadConsent  = "ask";

    // When true, the host arrow keys drive ONLY the emulated game-port
    // joystick (no //e keyboard latch), so a held direction can't flood
    // $C000 and starve a joystick-mode game's paddle reads. When false,
    // arrows drive the keyboard as usual. Toggled via the Machine menu's
    // "Map Arrows to Joystick" item; only meaningful on //e machines.
    bool         mapArrowsToJoystick   = false;

    // CRT state per monitor type. Each monitor (Color / Green / Amber /
    // White) has its own block so the user can dial in different
    // brightness, gamma, scanlines, etc. for each and the values stick
    // independently. `userOverride` gates whether the block's values
    // are applied verbatim or whether the layered preset / theme chain
    // is consulted instead (see CrtPostProcess::MakeCrtParams).
    struct Crt
    {
        float    brightness          = 1.0f;           // 0.0 .. 2.0
        float    contrast            = 1.0f;           // 0.0 .. 2.0
        float    gamma               = 1.0f;           // 0.5 .. 2.5 (final pow(rgb, 1/gamma)); 1.0 = bypass
        bool     scanlinesEnabled    = false;
        float    scanlinesIntensity  = 0.5f;           // 0.0 .. 1.0
        bool     bloomEnabled        = false;
        float    bloomRadius         = 1.0f;           // 0.0 .. 10.0 (output pixels)
        float    bloomStrength       = 0.5f;           // 0.0 .. 1.0
        bool     colorBleedEnabled   = false;
        float    colorBleedWidth     = 1.0f;           // 0.0 .. 8.0 (output pixels)
        float    persistence         = 0.0f;           // 0.0 .. 0.99 (phosphor decay factor)
        bool     userOverride        = false;
    };

    // Index by SettingsColorMode (Color=0, Green=1, Amber=2, White=3).
    // The matching ColorMode enum lives in UiCommandTypes.h; we don't
    // pull that include in here so the GlobalUserPrefs header stays
    // free of UI-layer dependencies.
    static constexpr size_t  kCrtModeCount = 4;
    Crt          crtByMode[kCrtModeCount];

    struct WindowBounds
    {
        int  x = 0;
        int  y = 0;
        int  w = 0;
        int  h = 0;
    };

    struct
    {
        bool        fullscreen = false;

        // Per-monitor-topology window placement. Key is the topology
        // hash from WindowPlacementProfile::BuildTopologyKey so a
        // single-monitor laptop layout and a docked multi-monitor
        // layout each get their own remembered bounds.
        std::map<std::string, WindowBounds>  placements;
    } window;

    // Most-recently-used disk image absolute paths, most-recent-first,
    // capped at 16 entries. Populated by AssetBootstrap / DiskManager /
    // BootDiskPicker; consumed by the themed boot-disk picker. Malformed
    // entries (non-string or empty) are dropped silently on load.
    std::vector<std::string>  recentDisks;

    // Unknown JSON keys round-trip back to disk untouched.
    std::vector<std::pair<std::string, JsonValue>>  unknownPassthrough;


    HRESULT     Load     (const std::wstring & baseDir,
                          IFileSystem        & fs);
    HRESULT     Save     (const std::wstring & baseDir,
                          IFileSystem        & fs) const;

    JsonValue   ToJson   () const;
    HRESULT     FromJson (const JsonValue & v);

    static std::wstring  FilePath (const std::wstring & baseDir);


private:
    static bool         GetBoolOpt   (const JsonValue   & obj,
                                      const std::string & key,
                                      bool                fallback);
    static double       GetNumberOpt (const JsonValue   & obj,
                                      const std::string & key,
                                      double              fallback);
    static int          GetIntOpt    (const JsonValue   & obj,
                                      const std::string & key,
                                      int                 fallback);
    static std::string  GetStringOpt (const JsonValue   & obj,
                                      const std::string & key,
                                      const std::string & fallback);

    static JsonValue    CrtToJson         (const Crt & c);
    static JsonValue    PlacementsToJson  (const std::map<std::string, WindowBounds> & placements);
    static JsonValue    RecentDisksToJson (const std::vector<std::string> & recentDisks);

    static void         CrtModeFromJson     (const JsonValue & modeObj, Crt & c);
    static void         PlacementsFromJson  (const JsonValue                     & placementsObj,
                                             std::map<std::string, WindowBounds> & placements);
    static void         RecentDisksFromJson (const JsonValue          & recentArr,
                                             std::vector<std::string> & recentDisks);
};

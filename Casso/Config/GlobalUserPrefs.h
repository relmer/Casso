#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"
#include "IFileSystem.h"
#include "../UiCommandTypes.h"
#include "../Ui/ColorUtil.h"





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

    // How host arrow / pointer input maps onto the emulated game port:
    // Off leaves the keys as ordinary //e keystrokes; Joystick maps the
    // arrow keys (plus Z / X) onto the paddle axes / fire buttons with a
    // spring return to center; Paddle captures the mouse for an absolute,
    // held dial. Cycled via the Machine menu's "Cycle Input Mode" item,
    // Ctrl+Shift+J, and the drive-bar widget; only meaningful on machines with a
    // game port. Migrated from the legacy bool "mapArrowsToJoystick".
    InputMappingMode  inputMappingMode = InputMappingMode::Off;   // legacy combined (kept in sync for downgrade compat)

    // Split input model: Keys (arrows->joystick) x Pointer
    // (Off/Paddle/Mouse). Migrated from the legacy single mode on load.
    bool              arrowsToJoystick = false;
    InputMappingMode  pointerMapping   = InputMappingMode::Off;

    // Text color used when the Color monitor is active (the monochrome
    // monitors derive their text from the phosphor tint instead). White is
    // the default; Green / Amber tint only the text; Custom uses the RGB in
    // colorMonitorTextCustomArgb. Set from the Settings > Display panel.
    ColorMonitorTextMode  colorMonitorTextMode       = ColorMonitorTextMode::White;
    uint32_t              colorMonitorTextCustomArgb = ColorUtil::kWhiteArgb;

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

    // Parallel to recentDisks: the wall-clock time each disk was last
    // loaded (Unix seconds; 0 == unknown). Same order and length as
    // recentDisks. A legacy prefs file without this key loads as empty,
    // so every recent disk starts with an unknown (0) load time.
    std::vector<std::int64_t>  recentDiskLoadedAt;

    // Host print-service preferences (Settings > Printing, FR-011). Global --
    // host print services are shared by every machine. The delivery
    // destination is no longer a stored preference: Print always targets a
    // Windows printer and Save always writes a PNG through the file dialog
    // (default folder <Pictures>\Casso Prints). Dot style is stored as the
    // contract's string token (like audioDownloadConsent).
    int          printOutputDpi   = 576;          // 288 | 576 (FR-028)
    std::string  printDotStyle    = "ink";        // "ink" | "plain" (FR-027)

    // ImageWriter II mechanical-sound preferences (Settings > Printing audio,
    // FR-034). `enabled` is the printer-sound master toggle (on by default);
    // when off the printer bus is silent (the shared "Drive Audio" master
    // enable still gates it above this). Volume 0..1 (default matches
    // PrinterAudioSource::kDefaultVolume). By default the sound auto-pans to
    // follow the preview window; panOverride pins a fixed pan (-1 left .. +1).
    bool         printerAudioEnabled      = true;
    float        printerAudioVolume       = 0.80f;
    bool         printerAudioPanOverride  = false;
    float        printerAudioPan          = 0.0f;   // -1 .. +1, used when override is on

    // Unknown JSON keys round-trip back to disk untouched.
    std::vector<std::pair<std::string, JsonValue>>  unknownPassthrough;


    HRESULT     Load     (const std::wstring & baseDir,
                          IFileSystem        & fs);
    HRESULT     Save     (const std::wstring & baseDir,
                          IFileSystem        & fs) const;

    JsonValue   ToJson   () const;
    HRESULT     FromJson (const JsonValue & v);

    // Revert the Color-monitor text colour to its default (White). The
    // custom ARGB is intentionally left intact so re-selecting "Custom"
    // restores the user's last-picked colour. Used by the Display page's
    // Restore-defaults action; keeping it here makes the durable behaviour
    // unit-testable without the (D3D-bound) Settings UI.
    void        ResetColorMonitorTextToDefault ();

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
    static JsonValue    RecentDiskTimesToJson (const std::vector<std::int64_t> & loadedAtUnix);

    static const char *      InputMappingModeToString   (InputMappingMode mode);
    static InputMappingMode  InputMappingModeFromString (const std::string & s, InputMappingMode fallback);

    static const char *           ColorTextModeToString   (ColorMonitorTextMode mode);
    static ColorMonitorTextMode   ColorTextModeFromString (const std::string & s, ColorMonitorTextMode fallback);

    static void         CrtModeFromJson     (const JsonValue & modeObj, Crt & c);
    static void         PlacementsFromJson  (const JsonValue                     & placementsObj,
                                             std::map<std::string, WindowBounds> & placements);
    static void         RecentDisksFromJson (const JsonValue          & recentArr,
                                             std::vector<std::string> & recentDisks);
    static void         RecentDiskTimesFromJson (const JsonValue           & loadedArr,
                                                 std::vector<std::int64_t> & loadedAtUnix);
};

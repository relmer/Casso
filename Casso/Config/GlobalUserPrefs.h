#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"
#include "CrtTypes.h"
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

    // Whether the display rides a period CRT monitor's curved glass. Default
    // ON; the Settings theme page checkbox drops the monitor and puts the
    // picture back on a flat rect. It governs the MONITOR only -- the 3D
    // drives are not optional and render either way. Skeuo themes only:
    // compact themes never draw the scene regardless. (Replaces the retired
    // skeuoMonitorFrame / deskScene keys, both ignored when loading.)
    bool         crtMonitor          = true;

    // The frames-per-second readout over the picture.
    //
    // OFF UNTIL ASKED FOR, in every build. It used to default on in a debug
    // build, on the reasoning that the build being worked on wants the number
    // in view. It is a debugging instrument either way, and one that sits on
    // the picture; a debug build is what the scene is normally LOOKED at in,
    // so defaulting it on put an overlay across most of the looking. Turn it
    // on from the View menu when there is a question for it to answer, and the
    // stored value wins from then on.
    bool         showFrameRate       = false;

    // The scene view -- orbit, zoom and pan -- written across the middle of
    // the picture.
    //
    // A REPORTING AID, not a measurement. A render fault in the desk scene is
    // usually only visible from one angle, and a screenshot does not carry the
    // angle it was taken from: reproducing it then means guessing the pose,
    // which wastes the reporter's time when the guess is wrong. With the pose
    // printed on the picture, any screenshot is self-describing and the view
    // can be restored exactly.
    //
    // Same default rule as the frame rate, and the same persistence: off until
    // asked for, then remembered. It earned its keep -- the CRT-to-bezel
    // hunt was reproduced from a pose read straight off a screenshot -- but a
    // line of numbers across the middle of the picture is for the report, not
    // for the ordinary view.
    bool         showSceneView       = false;

    // Multisampling for the 3D desk scene, in SAMPLES: 1 (off), 2, or 4. It
    // costs real GPU -- the whole scene is drawn into a target this many times
    // over -- and how much depends on the machine and the window size, so it
    // is a user choice rather than a constant. Global, not per machine or per
    // monitor: it describes the host's graphics budget, and nothing about the
    // emulated hardware. Values outside the set are clamped down to the
    // nearest supported one on load.
    int          sceneAntiAliasing   = 4;

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

    // HOW FAR EACH MONITOR'S BEZEL IS TILTED, in radians, keyed by the
    // monitor's catalog name. A property of the MONITOR rather than of the
    // machine, for the same reason its phosphor is: stand the same tube in
    // front of another machine and it is still tilted the way it was left.
    std::map<std::string, float>  monitorTilt;

    // Text color used when the Color monitor is active (the monochrome
    // monitors derive their text from the phosphor tint instead). White is
    // the default; Green / Amber tint only the text; Custom uses the RGB in
    // colorMonitorTextCustomArgb. Set from the Settings > Display panel.
    ColorMonitorTextMode  colorMonitorTextMode       = ColorMonitorTextMode::White;
    uint32_t              colorMonitorTextCustomArgb = ColorUtil::kWhiteArgb;

    // The user's own CRT adjustments, keyed by monitor and color mode as
    // "<monitorConfigName>/<mode>". SPARSE: only fields the user has
    // deliberately changed are present, and an absent field keeps following
    // the monitor preset and the active theme. Only pairs with at least one
    // override are stored.
    //
    // Build the key with CrtResolver::MakeKey rather than joining it here,
    // and resolve a picture with CrtResolver::Resolve. The monitor segment
    // is a frozen MonitorSpec::configName, so a stored key stays valid for
    // the life of that monitor in the catalog.
    std::map<std::string, CrtOverrides>  crtOverrides;

    struct WindowBounds
    {
        int   x = 0;
        int   y = 0;
        int   w = 0;
        int   h = 0;

        // Whether the window was MAXIMIZED when this placement was saved.
        // The rect above stays the NORMAL (restored) rect either way -- a
        // maximized window's own rect is the monitor's, and persisting that
        // as the windowed placement is the classic way to lose the user's
        // real window size.
        bool  maximized = false;
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

    // The folder the last created disk image landed in (UTF-8 absolute
    // path). The create dialog opens here next time; empty or vanished
    // falls back to Documents\Casso Disks.
    std::string  lastDiskCreateFolder;

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

    // Master output volume (the chrome toolbar's slider + mute): one gain over
    // the completed audio mix, so speaker, drives, printer, and Mockingboard
    // scale together. Mute keeps the slider value; the mix just gets gain 0.
    float        masterVolume             = 1.0f;   // 0 .. 1
    bool         masterMuted              = false;

    // Unknown JSON keys round-trip back to disk untouched.
    std::vector<std::pair<std::string, JsonValue>>  unknownPassthrough;


    HRESULT     Load     (const std::wstring & baseDir,
                          IFileSystem        & fs);
    HRESULT     Save     (const std::wstring & baseDir,
                          IFileSystem        & fs) const;

    JsonValue   ToJson   () const;
    HRESULT     FromJson (const JsonValue & v);

    // Revert the Color-monitor text color to its default (White). The
    // custom ARGB is intentionally left intact so re-selecting "Custom"
    // restores the user's last-picked color. Used by the Display page's
    // Restore-defaults action; keeping it here makes the durable behavior
    // unit-testable without the (D3D-bound) Settings UI.
    void        ResetColorMonitorTextToDefault ();

    static std::wstring  GetFilePath (const std::wstring & baseDir);


private:
    static bool         TryGetBoolOpt   (const JsonValue   & obj,
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

    static JsonValue    CrtOverridesToJson (const CrtOverrides & o);
    static JsonValue    PlacementsToJson  (const std::map<std::string, WindowBounds> & placements);
    static JsonValue    RecentDisksToJson (const std::vector<std::string> & recentDisks);
    static JsonValue    RecentDiskTimesToJson (const std::vector<std::int64_t> & loadedAtUnix);

    static const char *      InputMappingModeToString   (InputMappingMode mode);
    static InputMappingMode  InputMappingModeFromString (const std::string & s, InputMappingMode fallback);

    static const char *           ColorTextModeToString   (ColorMonitorTextMode mode);
    static ColorMonitorTextMode   ColorTextModeFromString (const std::string & s, ColorMonitorTextMode fallback);

    static void         CrtOverridesFromJson (const JsonValue & obj, CrtOverrides & o);
    static void         ReadCrtOverrides     (const JsonValue & v,
                                              std::map<std::string, CrtOverrides> & out);
    static void         PlacementsFromJson  (const JsonValue                     & placementsObj,
                                             std::map<std::string, WindowBounds> & placements);
    static void         RecentDisksFromJson (const JsonValue          & recentArr,
                                             std::vector<std::string> & recentDisks);
    static void         RecentDiskTimesFromJson (const JsonValue           & loadedArr,
                                                 std::vector<std::int64_t> & loadedAtUnix);
};

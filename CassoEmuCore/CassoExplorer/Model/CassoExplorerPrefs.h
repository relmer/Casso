#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkSettings.h"
#include "CassoExplorer/Model/FolderViews.h"
#include "CassoExplorer/Model/Location.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "Config/CrtTypes.h"
#include "Config/IFileSystem.h"
#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefs
//
//  The browser's own preferences, in a file of its own beside the emulator's.
//
//  ITS OWN FILE for the reason KnownFolderStore gives: the emulator rewrites
//  its preferences whole and would lose anything written there by another
//  process. The theme key is separate from the emulator's too; on first run
//  it starts from the emulator's current theme, read from the effective
//  `global` object of UserPrefs.json, and the two diverge from then on.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoExplorerPrefs
{
    struct Placement
    {
        int   x         = 0;
        int   y         = 0;
        int   w         = 0;
        int   h         = 0;
        bool  maximized = false;
        bool  valid     = false;
    };

    std::string            theme           = kThemeFollowSystem;
    bool                   previewVisible  = true;
    std::string            hostNaming      = kNamingDescriptive;


    //  Each folder's view as last chosen, most recent first. A folder with
    //  none opens in its type's view, as Explorer's do; the single listView
    //  of earlier builds is not read.
    FolderViews            folderViews;
    Placement              placement;
    int                    treeWidthDip    = kDefaultTreeWidthDip;
    int                    previewWidthDip = kDefaultPreviewWidthDip;

    //  Bytes between the spaces in a hex preview: 1, 2, 4 or 8.
    int                    hexGrouping     = kDefaultHexGrouping;

    //  Whether a BASIC listing shows where each line starts in memory.
    bool                   lineAddresses   = false;

    //  The navigation pane's options, from the menu below its last node.
    //  Each is empty, following File Explorer's own setting, until the user
    //  sets it here; then it holds Casso Explorer's value alone.
    std::optional<bool>    navShowThisPc;
    std::optional<bool>    navShowNetwork;
    std::optional<bool>    navShowLibraries;
    std::optional<bool>    navShowAllFolders;
    std::optional<bool>    navExpandToCurrent;

    //  How a hex preview reads: values in each row, 0 to fit the width;
    //  whether the values show at all; and their format.
    int                    hexColumns      = 0;
    bool                   hexShowValues   = true;
    std::string            hexFormat       = kHexFormatHex;

    //  The preview's text size, as a percentage of the theme's.
    int                    previewZoom     = kDefaultPreviewZoom;

    //  The monitor pictures are previewed on: "color", "green", "amber" or
    //  "white", as Casso writes them, "custom", or empty for the theme's
    //  default. Casso Explorer's own, apart from Casso's.
    std::string            previewMonitor;

    //  The custom monitor's phosphor color, as 0xAARRGGBB with the alpha
    //  opaque; saved as its RGB, as Casso saves its custom text color.
    uint32_t               previewCustomArgb = PicturePreview::kDefaultCustomArgb;

    //  The picture preview's CRT effects the user has changed, by monitor
    //  ("color", "green", "amber", "white", "custom"), each change on top of
    //  the monitor's own values and the theme's. Casso Explorer's own, apart
    //  from Casso's.
    std::map<std::string, CrtOverrides>  crtOverrides;

    std::vector<Location>  tabs;

    //  The address bar's typed path history, newest first.
    std::vector<std::wstring>  typedPaths;

    //  The file list's column widths, in order, or empty for File Explorer's
    //  defaults. A column left at its default stores nothing, so only what was
    //  dragged or fitted comes back. Stored as listColumnWidths: the older
    //  columnWidths key held another column order and is not read.
    std::vector<int>           columnWidthsDip;

    //  The order the list's columns were dragged into, left to right, as
    //  column indexes; empty for their own order.
    std::vector<int>           columnOrder;

    //  The columns last hidden from the header's menu, which a folder with no
    //  choice of its own follows. Stored as listHiddenColumns.
    std::vector<int>           hiddenColumns;

    //  The CassoLink connections, each under CassoLink in the tree, in the order they were
    //  added. Their names are unique; a later entry with a name already
    //  read is not read.
    std::vector<CassoLinkSettings>  links;

    //  Absent file: defaults, with the theme seeded from the emulator's
    //  preferences when they can be read. Reports the read's own result for
    //  a file that exists and will not parse.
    HRESULT  Load (const std::wstring & baseDir, IFileSystem & fs);
    HRESULT  Save (const std::wstring & baseDir, IFileSystem & fs) const;

    JsonValue  ToJson   () const;
    HRESULT    FromJson (const JsonValue & root);

    static std::wstring  GetFilePath (const std::wstring & baseDir);

    //  The emulator's active theme mapped onto this browser's palette for
    //  it: the three emulator themes by name, anything else Follow system.
    static std::string  MapCassoTheme (const std::string & activeTheme);
    static std::string  ReadCassoTheme (const std::wstring & baseDir, IFileSystem & fs);

    static bool  IsKnownTheme (const std::string & theme);

    //  The navigation pane's options with the key each is kept under.
    using NavOption = std::optional<bool> CassoExplorerPrefs::*;
    static std::array<std::pair<const char *, NavOption>, 5>  GetNavOptions ();

    //  1, 2, 4 and 8 are the groupings a hex view offers; anything else in
    //  the file is a hand edit and falls back to the default.
    static bool  IsKnownHexGrouping (int grouping);

    //  Each monitor's name as it is saved, in PicturePreview::Monitor's order:
    //  Casso's own, then the custom monitor's.
    static constexpr const char *  kMonitorNames[] = { "color", "green", "amber", "white", "custom" };

    static bool  IsMonitorName (const std::string & name);

    static constexpr const char *  kHexFormatHex       = "Hexadecimal";
    static constexpr const char *  kHexFormatSigned    = "Signed";
    static constexpr const char *  kHexFormatUnsigned  = "Unsigned";

    static constexpr const char *  kThemeLight         = "Light";
    static constexpr const char *  kThemeDark          = "Dark";
    static constexpr const char *  kThemeFollowSystem  = "FollowSystem";
    static constexpr const char *  kThemeSkeuomorphic  = "Skeuomorphic";
    static constexpr const char *  kThemeDarkModern    = "DarkModern";
    static constexpr const char *  kThemeRetroTerminal = "RetroTerminal";

    static constexpr const char *  kNamingDescriptive  = "Descriptive";
    static constexpr const char *  kNamingCiderPress   = "CiderPress";
    static constexpr const char *  kNamingAppleSingle  = "AppleSingle";

    static constexpr const wchar_t *  kFileName = L"CassoExplorerPrefs.json";

    static constexpr int  kViewCount              = 8;
    static constexpr int  kDefaultHexGrouping     = 1;
    static constexpr int  kDefaultPreviewZoom     = 100;
    static constexpr int  kMinPreviewZoom         = 50;
    static constexpr int  kMaxPreviewZoom         = 300;
    static constexpr int  kDefaultTreeWidthDip    = 240;
    static constexpr int  kDefaultPreviewWidthDip = 360;

    //  The name a location's kind is stored under, and the kind a name reads
    //  back as; anything else is no kind.
    static const char *  GetLocationKindName (Location::Kind kind);
    static bool          TryGetLocationKind  (const std::string & name, Location::Kind & outKind);

private:
    static JsonValue  LocationToJson (const Location & location);
    static JsonValue  LinkToJson     (const CassoLinkSettings & link);
    static bool       TryLinkFromJson (const JsonValue & value, CassoLinkSettings & outLink);

    //  A list of column indexes, and the one under a key; an entry that is
    //  not a number reads as -1, which nothing applies.
    static JsonValue         MakeIntArray (const std::vector<int> & values);
    static std::vector<int>  ReadIntArray (const JsonValue & object, const char * key);
    static bool       TryLocationFromJson (const JsonValue & value, Location & outLocation);
};

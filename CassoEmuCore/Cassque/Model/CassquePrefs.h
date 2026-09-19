#pragma once

#include "Pch.h"

#include "Cassque/Model/Location.h"
#include "Config/IFileSystem.h"
#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePrefs
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

struct CassquePrefs
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

    //  The file list's view, as DxuiListView numbers them; Details is 0.
    int                    listView        = 0;
    Placement              placement;
    int                    treeWidthDip    = kDefaultTreeWidthDip;
    int                    previewWidthDip = kDefaultPreviewWidthDip;

    //  Bytes between the spaces in a hex preview: 1, 2, 4 or 8.
    int                    hexGrouping     = kDefaultHexGrouping;

    //  Whether a BASIC listing shows where each line starts in memory.
    bool                   lineAddresses   = false;

    //  How a hex preview reads: values in each row, 0 to fit the width;
    //  whether the values show at all; and their format.
    int                    hexColumns      = 0;
    bool                   hexShowValues   = true;
    std::string            hexFormat       = kHexFormatHex;

    //  The preview's text size, as a percentage of the theme's.
    int                    previewZoom     = kDefaultPreviewZoom;
    std::vector<Location>  tabs;

    //  The address bar's typed path history, newest first.
    std::vector<std::wstring>  typedPaths;

    //  The file list's column widths, in order, or empty for the widths the
    //  columns fit for themselves. A column left at its fitted width stores
    //  nothing, so only what was dragged or fitted comes back.
    std::vector<int>           columnWidthsDip;

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

    //  1, 2, 4 and 8 are the groupings a hex view offers; anything else in
    //  the file is a hand edit and falls back to the default.
    static bool  IsKnownHexGrouping (int grouping);

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

    static constexpr const wchar_t *  kFileName = L"CassquePrefs.json";

    static constexpr int  kViewCount              = 8;
    static constexpr int  kDefaultHexGrouping     = 1;
    static constexpr int  kDefaultPreviewZoom     = 100;
    static constexpr int  kMinPreviewZoom         = 50;
    static constexpr int  kMaxPreviewZoom         = 300;
    static constexpr int  kDefaultTreeWidthDip    = 240;
    static constexpr int  kDefaultPreviewWidthDip = 360;

private:
    static JsonValue  LocationToJson (const Location & location);
    static bool       TryLocationFromJson (const JsonValue & value, Location & outLocation);
};

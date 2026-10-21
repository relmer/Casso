#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Config/IFileSystem.h"
#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSessionWindow
//
//  One window of a session: where it was on the screen, every tab it had, and
//  which tab was active. A tab is all of what it was showing -- its location,
//  the history behind and ahead of it, the selection, the sort and grouping,
//  the preview's scroll and whether the hex view disassembles. The catalog a
//  tab cached is not part of it; the tab reads its disk again.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoExplorerSessionWindow
{
    CassoExplorerPrefs::Placement   placement;
    std::vector<BrowserModel::Tab>  tabs;
    size_t                          activeTab = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession
//
//  The windows Casso Explorer reopens on its next launch, in a file of their
//  own beside the preferences. Which windows are written is the app's call;
//  this is the file.
//
//  Text is UTF-8, unlike the preferences file's narrow text, so a path in
//  any script reads back as it was written. A window or a tab that does not
//  read back, such as one with a sort this build does not have, is left out
//  rather than failing the rest.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerSession
{
public:
    static constexpr const wchar_t *  kFileName = L"CassoExplorerSession.json";

    static std::wstring  GetFilePath (const std::wstring & baseDir);

    //  No file is no session: S_OK with no windows.
    static HRESULT  Load (IFileSystem & fs, const std::wstring & baseDir, std::vector<CassoExplorerSessionWindow> & outWindows);
    static HRESULT  Save (IFileSystem & fs, const std::wstring & baseDir, const std::vector<CassoExplorerSessionWindow> & windows);

    static std::string  Write   (const std::vector<CassoExplorerSessionWindow> & windows);
    static bool         TryRead (const std::string & text, std::vector<CassoExplorerSessionWindow> & outWindows);

    static JsonValue  TabToJson      (const BrowserModel::Tab & tab);
    static bool       TryTabFromJson (const JsonValue & value, BrowserModel::Tab & outTab);

private:
    static JsonValue  WindowToJson        (const CassoExplorerSessionWindow & window);
    static bool       TryWindowFromJson   (const JsonValue & value, CassoExplorerSessionWindow & outWindow);
    static JsonValue  LocationToJson      (const Location & location);
    static JsonValue  LocationsToJson     (const std::vector<Location> & locations);
    static bool       TryLocationFromJson (const JsonValue & value, Location & outLocation);
    static bool       TryReadLocations    (const JsonValue & object, const char * key, std::vector<Location> & outLocations);
};

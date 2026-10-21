#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/BrowserModel.h"
#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TabTransfer
//
//  A tab moved out of one Casso Explorer window and into another, as text:
//  where it is, the history behind and ahead of it, and how it is viewed --
//  the selection, the sort, the grouping, the preview's scroll and whether
//  the hex view disassembles. Each window is its own process, so this is
//  what crosses between them, in a WM_COPYDATA to a window already open or
//  on the command line of a new one. The catalog cache stays behind; the
//  window the tab arrives in reads it again.
//
//  The text is JSON. Encode trims it to `maxChars`, the room a command line
//  has, by dropping the oldest history first, then the farthest forward,
//  then the selection.
//
////////////////////////////////////////////////////////////////////////////////

class TabTransfer
{
public:
    static constexpr size_t  kMaxCommandLineChars = 30000;

    static std::wstring  Encode    (const BrowserModel::Tab & tab, size_t maxChars = kMaxCommandLineChars);
    static bool          TryDecode (const std::wstring & text, BrowserModel::Tab & outTab);

private:
    static std::wstring  Write               (const BrowserModel::Tab & tab);
    static JsonValue     LocationToJson      (const Location & location);
    static JsonValue     LocationsToJson     (const std::vector<Location> & locations);
    static bool          TryLocationFromJson (const JsonValue & value, Location & outLocation);
    static bool          TryReadLocations    (const JsonValue & object, const char * key, std::vector<Location> & outLocations);
};

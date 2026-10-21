#include "Pch.h"

#include "CassoExplorer/Model/CassoExplorerSession.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::GetFilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerSession::GetFilePath (const std::wstring & baseDir)
{
    std::wstring  path = baseDir;



    if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
    {
        path += L'\\';
    }

    return path + kFileName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerSession::Load (IFileSystem & fs, const std::wstring & baseDir, std::vector<CassoExplorerSessionWindow> & outWindows)
{
    HRESULT       hr     = S_OK;
    std::wstring  path   = GetFilePath (baseDir);
    std::string   text;
    bool          exists = fs.Exists (path);
    bool          read   = false;



    outWindows.clear();
    BAIL_OUT_IF (!exists, S_OK);

    hr = fs.ReadAllText (path, text);
    CHR (hr);

    read = TryRead (text, outWindows);
    CBREx (read, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::Save
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerSession::Save (IFileSystem & fs, const std::wstring & baseDir, const std::vector<CassoExplorerSessionWindow> & windows)
{
    return fs.WriteAllText (GetFilePath (baseDir), Write (windows));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::Write
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoExplorerSession::Write (const std::vector<CassoExplorerSessionWindow> & windows)
{
    std::vector<std::pair<std::string, JsonValue>>  fields;
    std::vector<JsonValue>                          values;



    for (const CassoExplorerSessionWindow & window : windows)
    {
        values.push_back (WindowToJson (window));
    }

    fields.emplace_back ("windows", JsonValue (std::move (values)));

    return JsonWriter::Write (JsonValue (std::move (fields)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::TryRead
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerSession::TryRead (const std::string & text, std::vector<CassoExplorerSessionWindow> & outWindows)
{
    HRESULT                     hr      = S_OK;
    JsonValue                   root;
    JsonParseError              parseError;
    const JsonValue           * windows = nullptr;
    CassoExplorerSessionWindow  window;
    size_t                      i       = 0;
    bool                        listed  = false;



    outWindows.clear();

    hr = JsonParser::Parse (text, root, parseError);
    CHR (hr);

    listed = root.GetType() == JsonType::Object && root.HasArray ("windows", windows);
    CBR (listed);

    for (i = 0; i < windows->GetArraySize(); i++)
    {
        if (TryWindowFromJson (windows->GetArrayElement (i), window))
        {
            outWindows.push_back (std::move (window));
        }
    }

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::WindowToJson
//
////////////////////////////////////////////////////////////////////////////////

JsonValue CassoExplorerSession::WindowToJson (const CassoExplorerSessionWindow & window)
{
    std::vector<std::pair<std::string, JsonValue>>  fields;
    std::vector<std::pair<std::string, JsonValue>>  placement;
    std::vector<JsonValue>                          tabs;



    placement.emplace_back ("x",         JsonValue ((double) window.placement.x));
    placement.emplace_back ("y",         JsonValue ((double) window.placement.y));
    placement.emplace_back ("w",         JsonValue ((double) window.placement.w));
    placement.emplace_back ("h",         JsonValue ((double) window.placement.h));
    placement.emplace_back ("maximized", JsonValue (window.placement.maximized));
    placement.emplace_back ("valid",     JsonValue (window.placement.valid));

    for (const BrowserModel::Tab & tab : window.tabs)
    {
        tabs.push_back (TabToJson (tab));
    }

    fields.emplace_back ("placement", JsonValue (std::move (placement)));
    fields.emplace_back ("activeTab", JsonValue ((double) window.activeTab));
    fields.emplace_back ("tabs",      JsonValue (std::move (tabs)));

    return JsonValue (std::move (fields));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::TryWindowFromJson
//
//  A window with no tab that reads back is no window. One whose active tab
//  is past its last is on its first.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerSession::TryWindowFromJson (const JsonValue & value, CassoExplorerSessionWindow & outWindow)
{
    const JsonValue  * placement = nullptr;
    const JsonValue  * tabs      = nullptr;
    BrowserModel::Tab  tab;
    int                active    = 0;
    size_t             i         = 0;
    bool               read      = false;



    outWindow = CassoExplorerSessionWindow();

    read = value.GetType() == JsonType::Object && value.HasArray ("tabs", tabs);

    for (i = 0; read && i < tabs->GetArraySize(); i++)
    {
        if (TryTabFromJson (tabs->GetArrayElement (i), tab))
        {
            outWindow.tabs.push_back (std::move (tab));
        }
    }

    read = read && !outWindow.tabs.empty();

    if (read && value.HasObject ("placement", placement))
    {
        placement->HasInt  ("x",         outWindow.placement.x);
        placement->HasInt  ("y",         outWindow.placement.y);
        placement->HasInt  ("w",         outWindow.placement.w);
        placement->HasInt  ("h",         outWindow.placement.h);
        placement->HasBool ("maximized", outWindow.placement.maximized);
        placement->HasBool ("valid",     outWindow.placement.valid);
    }

    if (read && value.HasInt ("activeTab", active) && active >= 0 && active < (int) outWindow.tabs.size())
    {
        outWindow.activeTab = (size_t) active;
    }

    return read;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::TabToJson
//
////////////////////////////////////////////////////////////////////////////////

JsonValue CassoExplorerSession::TabToJson (const BrowserModel::Tab & tab)
{
    std::vector<std::pair<std::string, JsonValue>>  fields;
    std::vector<JsonValue>                          selection;



    for (const std::wstring & key : tab.selection)
    {
        selection.emplace_back (TextEncoding::WideToUtf8 (key));
    }

    fields.emplace_back ("location",        LocationToJson (tab.location));
    fields.emplace_back ("back",            LocationsToJson (tab.back));
    fields.emplace_back ("forward",         LocationsToJson (tab.forward));
    fields.emplace_back ("selection",       JsonValue (std::move (selection)));
    fields.emplace_back ("sortColumn",      JsonValue ((double) tab.sortColumn));
    fields.emplace_back ("sortDescending",  JsonValue (tab.sortDescending));
    fields.emplace_back ("groupBy",         JsonValue ((double) tab.groupBy));
    fields.emplace_back ("groupDescending", JsonValue (tab.groupDescending));
    fields.emplace_back ("previewScroll",   JsonValue ((double) tab.previewScroll));
    fields.emplace_back ("disassemble",     JsonValue (tab.disassemble));

    return JsonValue (std::move (fields));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::TryTabFromJson
//
//  A tab with no location, or with a sort or grouping this build does not
//  have, cannot be displayed.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerSession::TryTabFromJson (const JsonValue & value, BrowserModel::Tab & outTab)
{
    const JsonValue  * location  = nullptr;
    const JsonValue  * selection = nullptr;
    BrowserModel::Tab  tab;
    int                column    = 0;
    int                group     = 0;
    size_t             i         = 0;
    bool               read      = false;



    read = value.GetType() == JsonType::Object
        && value.HasObject ("location", location)
        && value.HasArray  ("selection", selection)
        && TryLocationFromJson (*location, tab.location)
        && TryReadLocations (value, "back", tab.back)
        && TryReadLocations (value, "forward", tab.forward)
        && value.HasInt  ("sortColumn",      column)
        && value.HasBool ("sortDescending",  tab.sortDescending)
        && value.HasInt  ("groupBy",         group)
        && value.HasBool ("groupDescending", tab.groupDescending)
        && value.HasInt  ("previewScroll",   tab.previewScroll)
        && value.HasBool ("disassemble",     tab.disassemble);

    read = read && column >= 0 && column <= (int) CatalogModel::Column::Folder && group >= 0 && group <= (int) RowGrouping::Field::Size;

    for (i = 0; read && i < selection->GetArraySize(); i++)
    {
        read = selection->GetArrayElement (i).GetType() == JsonType::String;

        if (read)
        {
            tab.selection.push_back (TextEncoding::Utf8ToWide (selection->GetArrayElement (i).GetString()));
        }
    }

    if (read)
    {
        tab.sortColumn = (CatalogModel::Column) column;
        tab.groupBy    = (RowGrouping::Field) group;
        outTab         = std::move (tab);
    }

    return read;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::LocationToJson
//
////////////////////////////////////////////////////////////////////////////////

JsonValue CassoExplorerSession::LocationToJson (const Location & location)
{
    std::vector<std::pair<std::string, JsonValue>>  fields;



    fields.emplace_back ("kind",  JsonValue (std::string (CassoExplorerPrefs::GetLocationKindName (location.kind))));
    fields.emplace_back ("path",  JsonValue (TextEncoding::WideToUtf8 (location.path)));
    fields.emplace_back ("inner", JsonValue (location.innerPath));
    fields.emplace_back ("label", JsonValue (TextEncoding::WideToUtf8 (location.label)));

    return JsonValue (std::move (fields));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::LocationsToJson
//
////////////////////////////////////////////////////////////////////////////////

JsonValue CassoExplorerSession::LocationsToJson (const std::vector<Location> & locations)
{
    std::vector<JsonValue>  values;



    for (const Location & location : locations)
    {
        values.push_back (LocationToJson (location));
    }

    return JsonValue (std::move (values));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::TryLocationFromJson
//
//  A tab on This PC, the Home a new tab opens on, has no kind of its own and
//  no path; it is written as a host folder with none, and reads back as Home.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerSession::TryLocationFromJson (const JsonValue & value, Location & outLocation)
{
    std::string  kind;
    std::string  path;
    std::string  inner;
    std::string  label;
    bool         read  = false;



    read = value.GetType() == JsonType::Object
        && value.HasString ("kind",  kind)
        && value.HasString ("path",  path)
        && value.HasString ("inner", inner)
        && value.HasString ("label", label)
        && CassoExplorerPrefs::TryGetLocationKind (kind, outLocation.kind);

    if (read)
    {
        outLocation.path      = TextEncoding::Utf8ToWide (path);
        outLocation.innerPath = inner;
        outLocation.label     = TextEncoding::Utf8ToWide (label);

        if (outLocation.kind == Location::Kind::HostFolder && outLocation.path.empty())
        {
            outLocation.kind = Location::Kind::None;
        }
    }

    return read;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSession::TryReadLocations
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerSession::TryReadLocations (const JsonValue & object, const char * key, std::vector<Location> & outLocations)
{
    const JsonValue  * array    = nullptr;
    Location           location;
    size_t             i        = 0;
    bool               read     = object.HasArray (key, array);



    outLocations.clear();

    for (i = 0; read && i < array->GetArraySize(); i++)
    {
        read = TryLocationFromJson (array->GetArrayElement (i), location);

        if (read)
        {
            outLocations.push_back (location);
        }
    }

    return read;
}

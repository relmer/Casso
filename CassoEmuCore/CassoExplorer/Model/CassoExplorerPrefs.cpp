#include "Pch.h"

#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Config/GlobalUserPrefs.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"



static constexpr const char *  s_kpszKindHostFolder    = "hostFolder";
static constexpr const char *  s_kpszKindDiskImage     = "diskImage";
static constexpr const char *  s_kpszKindDiskDirectory = "diskDirectory";
static constexpr const char *  s_kpszKindRoot          = "root";





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefs::GetFilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerPrefs::GetFilePath (const std::wstring & baseDir)
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
//  CassoExplorerPrefs::IsKnownTheme
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerPrefs::IsKnownTheme (const std::string & theme)
{
    return theme == kThemeLight || theme == kThemeDark || theme == kThemeFollowSystem
        || theme == kThemeSkeuomorphic || theme == kThemeDarkModern || theme == kThemeRetroTerminal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefs::IsKnownHexGrouping
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerPrefs::IsKnownHexGrouping (int grouping)
{
    return (grouping == 1) || (grouping == 2) || (grouping == 4) || (grouping == 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefs::MapCassoTheme
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoExplorerPrefs::MapCassoTheme (const std::string & activeTheme)
{
    if (activeTheme == kThemeSkeuomorphic || activeTheme == kThemeDarkModern || activeTheme == kThemeRetroTerminal)
    {
        return activeTheme;
    }

    return kThemeFollowSystem;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefs::ReadCassoTheme
//
//  The emulator's `activeTheme` under `global` in UserPrefs.json, read and
//  nothing else: the file is the emulator's to write. A missing or
//  unreadable file maps to Follow system.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoExplorerPrefs::ReadCassoTheme (const std::wstring & baseDir, IFileSystem & fs)
{
    HRESULT            hr     = S_OK;
    std::string        content;
    std::string        theme;
    JsonValue          root;
    JsonParseError     parseError;
    const JsonValue *  global = nullptr;
    std::wstring       path   = GlobalUserPrefs::GetFilePath (baseDir);



    hr = fs.ReadAllText (path, content);

    if (SUCCEEDED (hr))
    {
        hr = JsonParser::Parse (content, root, parseError);
    }

    if (SUCCEEDED (hr) && root.HasObject ("global", global))
    {
        global->HasString ("activeTheme", theme);
    }

    return MapCassoTheme (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefs::LocationToJson
//
////////////////////////////////////////////////////////////////////////////////

JsonValue CassoExplorerPrefs::LocationToJson (const Location & location)
{
    std::vector<std::pair<std::string, JsonValue>>  fields;
    const char *                                    kind = s_kpszKindHostFolder;



    switch (location.kind)
    {
        case Location::Kind::DiskImage:     kind = s_kpszKindDiskImage;     break;
        case Location::Kind::DiskDirectory: kind = s_kpszKindDiskDirectory; break;
        case Location::Kind::Root:          kind = s_kpszKindRoot;          break;
        default:                            break;
    }

    fields.emplace_back ("kind",  JsonValue (std::string (kind)));
    fields.emplace_back ("path",  JsonValue (TextEncoding::WideToNarrow (location.path)));
    fields.emplace_back ("inner", JsonValue (location.innerPath));

    return JsonValue (std::move (fields));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefs::TryLocationFromJson
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerPrefs::TryLocationFromJson (const JsonValue & value, Location & outLocation)
{
    std::string  kind;
    std::string  path;
    std::string  inner;



    outLocation = Location();

    if (value.GetType() != JsonType::Object || !value.HasString ("kind", kind) || !value.HasString ("path", path) || path.empty())
    {
        return false;
    }

    value.HasString ("inner", inner);

    outLocation.path      = TextEncoding::NarrowToWide (path);
    outLocation.innerPath = inner;

    if (kind == s_kpszKindDiskImage)          { outLocation.kind = Location::Kind::DiskImage; }
    else if (kind == s_kpszKindDiskDirectory) { outLocation.kind = Location::Kind::DiskDirectory; }
    else if (kind == s_kpszKindHostFolder)    { outLocation.kind = Location::Kind::HostFolder; }
    else if (kind == s_kpszKindRoot)          { outLocation.kind = Location::Kind::Root; }
    else                                      { return false; }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefs::ToJson
//
////////////////////////////////////////////////////////////////////////////////

JsonValue CassoExplorerPrefs::ToJson() const
{
    std::vector<std::pair<std::string, JsonValue>>  root;
    std::vector<std::pair<std::string, JsonValue>>  placementFields;
    std::vector<std::pair<std::string, JsonValue>>  splitterFields;
    std::vector<JsonValue>                          tabValues;
    std::vector<JsonValue>                          typedValues;
    std::vector<JsonValue>                          widthValues;
    std::vector<JsonValue>                          orderValues;
    std::vector<JsonValue>                          viewValues;



    for (const FolderViewEntry & entry : folderViews.GetEntries())
    {
        std::vector<std::pair<std::string, JsonValue>>  fields;

        fields.emplace_back ("key",  JsonValue (TextEncoding::WideToNarrow (entry.key)));
        fields.emplace_back ("view", JsonValue ((double) (int) entry.view));
        viewValues.push_back (JsonValue (std::move (fields)));
    }

    for (const std::wstring & typed : typedPaths)
    {
        typedValues.push_back (JsonValue (TextEncoding::WideToNarrow (typed)));
    }

    for (int width : columnWidthsDip)
    {
        widthValues.push_back (JsonValue ((double) width));
    }

    for (int column : columnOrder)
    {
        orderValues.push_back (JsonValue ((double) column));
    }

    placementFields.emplace_back ("x",         JsonValue ((double) placement.x));
    placementFields.emplace_back ("y",         JsonValue ((double) placement.y));
    placementFields.emplace_back ("w",         JsonValue ((double) placement.w));
    placementFields.emplace_back ("h",         JsonValue ((double) placement.h));
    placementFields.emplace_back ("maximized", JsonValue (placement.maximized));
    placementFields.emplace_back ("valid",     JsonValue (placement.valid));

    splitterFields.emplace_back ("treeDip",    JsonValue ((double) treeWidthDip));
    splitterFields.emplace_back ("previewDip", JsonValue ((double) previewWidthDip));

    for (const Location & tab : tabs)
    {
        tabValues.push_back (LocationToJson (tab));
    }

    root.emplace_back ("theme",          JsonValue (theme));
    root.emplace_back ("previewVisible", JsonValue (previewVisible));
    root.emplace_back ("hostNaming",     JsonValue (hostNaming));
    root.emplace_back ("hexGrouping",    JsonValue ((double) hexGrouping));
    root.emplace_back ("lineAddresses",  JsonValue (lineAddresses));
    root.emplace_back ("hexColumns",     JsonValue ((double) hexColumns));
    root.emplace_back ("hexShowValues",  JsonValue (hexShowValues));
    root.emplace_back ("hexFormat",      JsonValue (hexFormat));
    root.emplace_back ("previewZoom",    JsonValue ((double) previewZoom));
    root.emplace_back ("placement",      JsonValue (std::move (placementFields)));
    root.emplace_back ("splitters",      JsonValue (std::move (splitterFields)));
    root.emplace_back ("tabs",           JsonValue (std::move (tabValues)));
    root.emplace_back ("typedPaths",     JsonValue (std::move (typedValues)));
    root.emplace_back ("listColumnWidths", JsonValue (std::move (widthValues)));
    root.emplace_back ("listColumnOrder",  JsonValue (std::move (orderValues)));
    root.emplace_back ("folderViews",      JsonValue (std::move (viewValues)));

    return JsonValue (std::move (root));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefs::FromJson
//
//  Every field is optional and keeps its default when absent or malformed,
//  so a file written by a later build still loads.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerPrefs::FromJson (const JsonValue & root)
{
    HRESULT            hr         = S_OK;
    bool               isObject   = root.GetType() == JsonType::Object;
    const JsonValue  * placed     = nullptr;
    const JsonValue  * splitters  = nullptr;
    const JsonValue  * tabArray   = nullptr;
    const JsonValue  * typedArray = nullptr;
    const JsonValue  * widthArray = nullptr;
    const JsonValue  * orderArray = nullptr;
    const JsonValue  * viewArray  = nullptr;
    std::string        text;
    size_t             i          = 0;
    int                grouping   = kDefaultHexGrouping;
    int                columns    = 0;
    int                zoom       = kDefaultPreviewZoom;
    int                view       = 0;



    CBREx (isObject, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (root.HasString ("theme", text) && IsKnownTheme (text))
    {
        theme = text;
    }

    root.HasBool ("previewVisible", previewVisible);
    root.HasBool ("lineAddresses",  lineAddresses);
    root.HasBool ("hexShowValues",  hexShowValues);

    if (root.HasInt ("previewZoom", zoom) && zoom >= kMinPreviewZoom && zoom <= kMaxPreviewZoom)
    {
        previewZoom = zoom;
    }

    if (root.HasInt ("hexColumns", columns) && (columns == 0 || columns == 16 || IsKnownHexGrouping (columns)))
    {
        hexColumns = columns;
    }

    if (root.HasString ("hexFormat", text) && (text == kHexFormatHex || text == kHexFormatSigned || text == kHexFormatUnsigned))
    {
        hexFormat = text;
    }

    if (root.HasString ("hostNaming", text) && (text == kNamingDescriptive || text == kNamingCiderPress || text == kNamingAppleSingle))
    {
        hostNaming = text;
    }

    if (root.HasInt ("hexGrouping", grouping) && IsKnownHexGrouping (grouping))
    {
        hexGrouping = grouping;
    }

    if (root.HasObject ("placement", placed))
    {
        placed->HasInt  ("x",         placement.x);
        placed->HasInt  ("y",         placement.y);
        placed->HasInt  ("w",         placement.w);
        placed->HasInt  ("h",         placement.h);
        placed->HasBool ("maximized", placement.maximized);
        placed->HasBool ("valid",     placement.valid);
    }

    if (root.HasObject ("splitters", splitters))
    {
        splitters->HasInt ("treeDip",    treeWidthDip);
        splitters->HasInt ("previewDip", previewWidthDip);
    }

    if (root.HasArray ("tabs", tabArray))
    {
        tabs.clear();

        for (i = 0; i < tabArray->GetArraySize(); i++)
        {
            Location  location;

            if (TryLocationFromJson (tabArray->GetArrayElement (i), location))
            {
                tabs.push_back (location);
            }
        }
    }

    if (root.HasArray ("typedPaths", typedArray))
    {
        typedPaths.clear();

        for (i = 0; i < typedArray->GetArraySize(); i++)
        {
            const JsonValue &  value = typedArray->GetArrayElement (i);

            if (value.GetType() == JsonType::String)
            {
                typedPaths.push_back (TextEncoding::NarrowToWide (value.GetString()));
            }
        }
    }

    if (root.HasArray ("listColumnWidths", widthArray))
    {
        columnWidthsDip.clear();

        for (i = 0; i < widthArray->GetArraySize(); i++)
        {
            const JsonValue &  value = widthArray->GetArrayElement (i);

            //  A hand-edited width of zero or less means the column fits
            //  itself, which is what an absent entry means too.
            columnWidthsDip.push_back ((value.GetType() == JsonType::Number) ? (int) value.GetNumber() : 0);
        }
    }

    //  Checked by the list when applied: an order that does not name every
    //  column once is ignored there.
    if (root.HasArray ("listColumnOrder", orderArray))
    {
        columnOrder.clear();

        for (i = 0; i < orderArray->GetArraySize(); i++)
        {
            const JsonValue &  value = orderArray->GetArrayElement (i);

            columnOrder.push_back ((value.GetType() == JsonType::Number) ? (int) value.GetNumber() : -1);
        }
    }

    if (root.HasArray ("folderViews", viewArray))
    {
        std::vector<FolderViewEntry>  entries;

        for (i = 0; i < viewArray->GetArraySize(); i++)
        {
            const JsonValue &  value = viewArray->GetArrayElement (i);
            std::string        key;

            if (value.GetType() == JsonType::Object && value.HasString ("key", key) && value.HasInt ("view", view) && view >= 0 && view < kViewCount)
            {
                entries.push_back (FolderViewEntry { TextEncoding::NarrowToWide (key), (DxuiListView::View) view });
            }
        }

        folderViews.SetEntries (std::move (entries));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefs::Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerPrefs::Load (const std::wstring & baseDir, IFileSystem & fs)
{
    HRESULT         hr      = S_OK;
    std::wstring    path    = GetFilePath (baseDir);
    bool            present = fs.Exists (path);
    std::string     content;
    JsonValue       root;
    JsonParseError  parseError;



    *this = CassoExplorerPrefs();

    if (!present)
    {
        theme = ReadCassoTheme (baseDir, fs);

        return S_OK;
    }

    hr = fs.ReadAllText (path, content);
    CHR (hr);

    hr = JsonParser::Parse (content, root, parseError);
    CHR (hr);

    hr = FromJson (root);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefs::Save
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerPrefs::Save (const std::wstring & baseDir, IFileSystem & fs) const
{
    HRESULT              hr = S_OK;
    std::string          text;
    JsonWriter::Options  options;



    hr = JsonWriter::Write (ToJson(), options, text);
    CHR (hr);

    hr = fs.WriteAllText (GetFilePath (baseDir), text);
    CHR (hr);

Error:
    return hr;
}

#include "Pch.h"

#include "Cassque/Model/CassquePrefs.h"
#include "Config/GlobalUserPrefs.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"



static constexpr const char *  s_kpszKindHostFolder    = "hostFolder";
static constexpr const char *  s_kpszKindDiskImage     = "diskImage";
static constexpr const char *  s_kpszKindDiskDirectory = "diskDirectory";





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePrefs::GetFilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassquePrefs::GetFilePath (const std::wstring & baseDir)
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
//  CassquePrefs::IsKnownTheme
//
////////////////////////////////////////////////////////////////////////////////

bool CassquePrefs::IsKnownTheme (const std::string & theme)
{
    return theme == kThemeLight || theme == kThemeDark || theme == kThemeFollowSystem
        || theme == kThemeSkeuomorphic || theme == kThemeDarkModern || theme == kThemeRetroTerminal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePrefs::IsKnownHexGrouping
//
////////////////////////////////////////////////////////////////////////////////

bool CassquePrefs::IsKnownHexGrouping (int grouping)
{
    return (grouping == 1) || (grouping == 2) || (grouping == 4) || (grouping == 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePrefs::MapCassoTheme
//
////////////////////////////////////////////////////////////////////////////////

std::string CassquePrefs::MapCassoTheme (const std::string & activeTheme)
{
    if (activeTheme == kThemeSkeuomorphic || activeTheme == kThemeDarkModern || activeTheme == kThemeRetroTerminal)
    {
        return activeTheme;
    }

    return kThemeFollowSystem;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePrefs::ReadCassoTheme
//
//  The emulator's `activeTheme` under `global` in UserPrefs.json, read and
//  nothing else: the file is the emulator's to write. A missing or
//  unreadable file maps to Follow system.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassquePrefs::ReadCassoTheme (const std::wstring & baseDir, IFileSystem & fs)
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
//  CassquePrefs::LocationToJson
//
////////////////////////////////////////////////////////////////////////////////

JsonValue CassquePrefs::LocationToJson (const Location & location)
{
    std::vector<std::pair<std::string, JsonValue>>  fields;
    const char *                                    kind = s_kpszKindHostFolder;



    switch (location.kind)
    {
        case Location::Kind::DiskImage:     kind = s_kpszKindDiskImage;     break;
        case Location::Kind::DiskDirectory: kind = s_kpszKindDiskDirectory; break;
        default:                            break;
    }

    fields.emplace_back ("kind",  JsonValue (std::string (kind)));
    fields.emplace_back ("path",  JsonValue (TextEncoding::WideToNarrow (location.path)));
    fields.emplace_back ("inner", JsonValue (location.innerPath));

    return JsonValue (std::move (fields));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePrefs::TryLocationFromJson
//
////////////////////////////////////////////////////////////////////////////////

bool CassquePrefs::TryLocationFromJson (const JsonValue & value, Location & outLocation)
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
    else                                      { return false; }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePrefs::ToJson
//
////////////////////////////////////////////////////////////////////////////////

JsonValue CassquePrefs::ToJson() const
{
    std::vector<std::pair<std::string, JsonValue>>  root;
    std::vector<std::pair<std::string, JsonValue>>  placementFields;
    std::vector<std::pair<std::string, JsonValue>>  splitterFields;
    std::vector<JsonValue>                          tabValues;



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
    root.emplace_back ("placement",      JsonValue (std::move (placementFields)));
    root.emplace_back ("splitters",      JsonValue (std::move (splitterFields)));
    root.emplace_back ("tabs",           JsonValue (std::move (tabValues)));

    return JsonValue (std::move (root));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePrefs::FromJson
//
//  Every field is optional and keeps its default when absent or malformed,
//  so a file written by a later build still loads.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassquePrefs::FromJson (const JsonValue & root)
{
    HRESULT            hr        = S_OK;
    bool               isObject  = root.GetType() == JsonType::Object;
    const JsonValue *  placed    = nullptr;
    const JsonValue *  splitters = nullptr;
    const JsonValue *  tabArray  = nullptr;
    std::string        text;
    size_t             i         = 0;
    int                grouping  = kDefaultHexGrouping;



    CBREx (isObject, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (root.HasString ("theme", text) && IsKnownTheme (text))
    {
        theme = text;
    }

    root.HasBool ("previewVisible", previewVisible);

    if (root.HasString ("hostNaming", text) && (text == kNamingDescriptive || text == kNamingCiderPress))
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

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePrefs::Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassquePrefs::Load (const std::wstring & baseDir, IFileSystem & fs)
{
    HRESULT         hr      = S_OK;
    std::wstring    path    = GetFilePath (baseDir);
    bool            present = fs.Exists (path);
    std::string     content;
    JsonValue       root;
    JsonParseError  parseError;



    *this = CassquePrefs();

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
//  CassquePrefs::Save
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassquePrefs::Save (const std::wstring & baseDir, IFileSystem & fs) const
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

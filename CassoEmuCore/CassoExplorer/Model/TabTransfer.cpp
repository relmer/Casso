#include "Pch.h"

#include "CassoExplorer/Model/TabTransfer.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TabTransfer::Encode
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TabTransfer::Encode (const BrowserModel::Tab & tab, size_t maxChars)
{
    BrowserModel::Tab  trimmed = tab;
    std::wstring       text    = Write (trimmed);



    while (text.size() > maxChars && (!trimmed.back.empty() || !trimmed.forward.empty() || !trimmed.selection.empty()))
    {
        if (!trimmed.back.empty())
        {
            trimmed.back.erase (trimmed.back.begin());
        }
        else if (!trimmed.forward.empty())
        {
            trimmed.forward.erase (trimmed.forward.begin());
        }
        else
        {
            trimmed.selection.pop_back();
        }

        text = Write (trimmed);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TabTransfer::Write
//
//  Forward history is a stack whose top is the next step, so its first
//  entry is the farthest; Encode drops that one first.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TabTransfer::Write (const BrowserModel::Tab & tab)
{
    std::vector<std::pair<std::string, JsonValue>>  fields;
    std::vector<JsonValue>                          selection;
    JsonWriter::Options                             options;
    std::string                                     text;
    HRESULT                                         hr = S_OK;



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

    options.fPretty = false;

    hr = JsonWriter::Write (JsonValue (std::move (fields)), options, text);
    CHRA (hr);

Error:
    return TextEncoding::Utf8ToWide (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TabTransfer::TryDecode
//
//  A tab with no location, or with a sort or grouping this build does not
//  have, cannot be displayed in this window.
//
////////////////////////////////////////////////////////////////////////////////

bool TabTransfer::TryDecode (const std::wstring & text, BrowserModel::Tab & outTab)
{
    HRESULT            hr         = S_OK;
    JsonValue          root;
    JsonParseError     parseError;
    const JsonValue *  location   = nullptr;
    const JsonValue *  selection  = nullptr;
    BrowserModel::Tab  tab;
    int                column     = 0;
    int                group      = 0;
    bool               decoded    = false;
    bool               hasFields  = false;
    size_t             i          = 0;



    hr = JsonParser::Parse (TextEncoding::WideToUtf8 (text), root, parseError);
    CHR (hr);

    hasFields = root.GetType() == JsonType::Object && root.HasObject ("location", location) && root.HasArray ("selection", selection);
    CBR (hasFields);

    decoded = TryLocationFromJson (*location, tab.location)
           && TryReadLocations (root, "back", tab.back)
           && TryReadLocations (root, "forward", tab.forward)
           && root.HasInt  ("sortColumn",      column)
           && root.HasBool ("sortDescending",  tab.sortDescending)
           && root.HasInt  ("groupBy",         group)
           && root.HasBool ("groupDescending", tab.groupDescending)
           && root.HasInt  ("previewScroll",   tab.previewScroll)
           && root.HasBool ("disassemble",     tab.disassemble);
    CBR (decoded);

    decoded = column >= 0 && column <= (int) CatalogModel::Column::Folder && group >= 0 && group <= (int) RowGrouping::Field::Size;
    CBR (decoded);

    tab.sortColumn = (CatalogModel::Column) column;
    tab.groupBy    = (RowGrouping::Field) group;

    for (i = 0; i < selection->GetArraySize(); i++)
    {
        decoded = selection->GetArrayElement (i).GetType() == JsonType::String;
        CBR (decoded);

        tab.selection.push_back (TextEncoding::Utf8ToWide (selection->GetArrayElement (i).GetString()));
    }

    outTab = std::move (tab);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TabTransfer::LocationToJson
//
//  In UTF-8, unlike the preferences file's narrow text, so a path in any
//  script arrives as it left.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue TabTransfer::LocationToJson (const Location & location)
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
//  TabTransfer::LocationsToJson
//
////////////////////////////////////////////////////////////////////////////////

JsonValue TabTransfer::LocationsToJson (const std::vector<Location> & locations)
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
//  TabTransfer::TryLocationFromJson
//
//  A tab on This PC, the Home a new tab opens on, has no kind of its own and
//  no path; it is stored as a host folder with none, and reads back as it was.
//
////////////////////////////////////////////////////////////////////////////////

bool TabTransfer::TryLocationFromJson (const JsonValue & value, Location & outLocation)
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
//  TabTransfer::TryReadLocations
//
////////////////////////////////////////////////////////////////////////////////

bool TabTransfer::TryReadLocations (const JsonValue & object, const char * key, std::vector<Location> & outLocations)
{
    const JsonValue *  array = nullptr;
    Location           location;
    size_t             i     = 0;
    bool               read  = object.HasArray (key, array);



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

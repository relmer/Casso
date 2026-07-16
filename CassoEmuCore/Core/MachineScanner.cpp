#include "Pch.h"

#include "MachineScanner.h"
#include "JsonParser.h"
#include "JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExtractFields
//
//  Pulls the "name" and "releaseYear" fields out of a machine config
//  JSON blob. Returns empty / 0 when either is missing or unparseable
//  -- the caller treats absent "name" as "fall back to the
//  subdirectory name" and absent "releaseYear" as "sort to the end".
//
////////////////////////////////////////////////////////////////////////////////

static void ExtractFields (const string  & jsonText,
                           wstring       & outDisplayName,
                           int           & outReleaseYear)
{
    HRESULT         hr = S_OK;
    string          name;
    int             year = 0;
    JsonValue       root;
    JsonParseError  parseError;



    outDisplayName.clear();
    outReleaseYear = 0;

    hr = JsonParser::Parse (jsonText, root, parseError);
    CHR (hr);

    hr = root.GetString ("name", name);
    if (SUCCEEDED (hr))
    {
        outDisplayName.assign (name.begin(), name.end());
    }

    hr = root.GetInt ("releaseYear", year);
    if (SUCCEEDED (hr))
    {
        outReleaseYear = year;
    }


Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Scan
//
//  Walks each search path looking for a Machines/ directory. The first
//  one found wins. Within it, each immediate subdirectory <Name>/ is
//  expected to contain <Name>.json. The JSON's "name" field becomes the
//  display name; if the JSON is missing or unparseable, falls back to
//  the subdirectory name. Subdirectories without a matching .json are
//  silently skipped (they may belong to a different asset family).
//
////////////////////////////////////////////////////////////////////////////////

vector<MachineInfo> MachineScanner::Scan (
    const vector<fs::path>                & searchPaths,
    const MachineScanner::DirectoryLister & lister,
    const MachineScanner::FileReader      & reader)
{
    vector<MachineInfo>  results;



    for (const auto & basePath : searchPaths)
    {
        fs::path          machinesDir = basePath / "Machines";
        vector<fs::path>  subDirs     = lister (machinesDir);

        if (subDirs.empty())
        {
            continue;
        }

        for (const auto & subDir : subDirs)
        {
            MachineInfo  info;
            fs::path     jsonPath = subDir / (subDir.filename().string() + ".json");
            string       jsonText;
            wstring      name;
            HRESULT      hrRead   = reader (jsonPath, jsonText);

            if (FAILED (hrRead))
            {
                continue;
            }

            info.fileName    = subDir.filename().wstring();
            ExtractFields (jsonText, name, info.releaseYear);
            info.displayName = name.empty() ? info.fileName : name;

            results.push_back (move (info));
        }

        // First search path containing a non-empty Machines/ wins.
        break;
    }

    // Sort chronologically by releaseYear (ascending), then by display
    // name as a tiebreaker. Machines without a releaseYear sort to the
    // end so old configs without the field don't move ahead of the
    // embedded defaults.
    std::sort (results.begin(), results.end(),
               [] (const MachineInfo & a, const MachineInfo & b)
    {
        int  aYear = a.releaseYear > 0 ? a.releaseYear : INT_MAX;
        int  bYear = b.releaseYear > 0 ? b.releaseYear : INT_MAX;

        if (aYear != bYear) { return aYear < bYear; }
        return a.displayName < b.displayName;
    });

    return results;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EqualsIgnoreCaseAscii
//
//  ASCII case-insensitive compare of two wide strings. Machine identifiers
//  are ASCII ("Apple2e", "Apple2Plus"), so folding A-Z per code unit is
//  sufficient and avoids a locale dependency.
//
////////////////////////////////////////////////////////////////////////////////

static bool EqualsIgnoreCaseAscii (std::wstring_view a, std::wstring_view b)
{
    bool  equal = (a.size() == b.size());



    for (size_t i = 0; equal && i < a.size(); ++i)
    {
        wchar_t  ca = (a[i] >= L'A' && a[i] <= L'Z') ? (wchar_t) (a[i] + (L'a' - L'A')) : a[i];
        wchar_t  cb = (b[i] >= L'A' && b[i] <= L'Z') ? (wchar_t) (b[i] + (L'a' - L'A')) : b[i];

        equal = (ca == cb);
    }

    return equal;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SelectCanonical
//
//  Resolve `requested` to a discovered machine's canonical fileName so a
//  mis-cased --machine value still selects the right config and downstream
//  exact-match lookups (ROM catalog, display name) agree. See the header
//  for the fallback order.
//
////////////////////////////////////////////////////////////////////////////////

wstring MachineScanner::SelectCanonical (
    const vector<MachineInfo> & discovered,
    std::wstring_view           requested,
    std::wstring_view           preferred)
{
    wstring  result;



    if (!requested.empty())
    {
        for (const MachineInfo & info : discovered)
        {
            if (EqualsIgnoreCaseAscii (info.fileName, requested))
            {
                result = info.fileName;
                break;
            }
        }
    }

    if (result.empty())
    {
        for (const MachineInfo & info : discovered)
        {
            if (info.fileName == preferred)
            {
                result = info.fileName;
                break;
            }
        }
    }

    if (result.empty() && !discovered.empty())
    {
        result = discovered.front().fileName;
    }

    if (result.empty())
    {
        result = wstring (preferred);
    }

    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ListDirectory
//
////////////////////////////////////////////////////////////////////////////////

vector<fs::path> MachineScanner::ListDirectory (const fs::path & dir)
{
    vector<fs::path>  out;
    error_code        ec;



    if (fs::is_directory (dir, ec))
    {
        for (const auto & entry : fs::directory_iterator (dir, ec))
        {
            if (entry.is_directory (ec))
            {
                out.push_back (entry.path());
            }
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineScanner::ReadFile (const fs::path & file, string & outText)
{
    HRESULT       hr = S_OK;
    ifstream      stream (file);
    stringstream  ss;



    CBR (stream.good());

    ss << stream.rdbuf();
    outText = ss.str();


Error:
    return hr;
}
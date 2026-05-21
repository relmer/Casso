#include "Pch.h"

#include "MachineScanner.h"
#include "JsonParser.h"
#include "JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExtractDisplayName
//
//  Pulls the "name" field out of a machine config JSON blob. Returns
//  empty when the JSON is unparseable or has no "name" — the caller
//  treats that as "fall back to the subdirectory name".
//
////////////////////////////////////////////////////////////////////////////////

static wstring ExtractDisplayName (const string & jsonText)
{
    HRESULT         hr = S_OK;
    wstring         displayName;
    string          name;
    JsonValue       root;
    JsonParseError  parseError;



    hr = JsonParser::Parse (jsonText, root, parseError);
    CHR (hr);

    hr = root.GetString ("name", name);
    CHRF (hr, name.clear());

    displayName.assign (name.begin(), name.end());


Error:
    return displayName;
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
            name             = ExtractDisplayName (jsonText);
            info.displayName = name.empty() ? info.fileName : name;

            results.push_back (move (info));
        }

        // First search path containing a non-empty Machines/ wins.
        break;
    }

    return results;
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
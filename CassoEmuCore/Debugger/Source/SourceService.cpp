#include "Pch.h"

#include "Debugger/Source/SourceService.h"
#include "Sha1.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceService::Find
//
//  A file of the right name but the wrong size is kept aside: if nothing of
//  the right size turns up anywhere, it is the likeliest edit of the file and
//  opens with the warning rather than not at all.
//
////////////////////////////////////////////////////////////////////////////////

SourceLookup SourceService::Find (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                  const std::string & programKey)
{
    SourceLookup   result;
    std::wstring   fileName = GetFileName (SourcePathList::Utf8ToWide (record.name));
    std::wstring   nameOnly;
    HRESULT        hr       = S_OK;



    for (const std::wstring & folder : GetSearchFolders (record, debugFilePath, programKey))
    {
        if (TryFolder (record, folder, fileName, result, nameOnly))
        {
            m_paths.AddFound (programKey, folder);
            return result;
        }
    }

    if (!result.candidates.empty())
    {
        result.match = SourceMatch::Mismatch;
        result.path  = result.candidates.front();
        hr           = m_files.ReadAllText (result.path, result.text);
    }
    else if (!nameOnly.empty())
    {
        result.match = SourceMatch::Mismatch;
        result.path  = nameOnly;
        hr           = m_files.ReadAllText (result.path, result.text);
    }

    if (FAILED (hr))
    {
        result = SourceLookup();
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceService::GetSearchFolders
//
//  The recorded path's folder, then this program's list, then the global
//  one, each folder once.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> SourceService::GetSearchFolders (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                                           const std::string & programKey) const
{
    std::vector<std::wstring>  folders;
    std::wstring               recorded = Combine (GetFolder (debugFilePath), SourcePathList::Utf8ToWide (record.name));
    auto                       add      = [&] (const std::wstring & folder)
                                          {
                                              for (const std::wstring & each : folders)
                                              {
                                                  if (SourcePathList::IsSameFolder (each, folder))
                                                  {
                                                      return;
                                                  }
                                              }

                                              folders.push_back (folder);
                                          };



    add (GetFolder (recorded));

    for (const std::wstring & folder : m_paths.GetProgramFolders (programKey))
    {
        add (folder);
    }

    for (const std::wstring & folder : m_paths.GetGlobalFolders())
    {
        add (folder);
    }

    return folders;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceService::TryFolder
//
//  True when the folder holds the file: its hash matches, or the record has
//  no hash and its size matches. A size match with another hash becomes a
//  candidate, and a name match of another size is remembered once.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceService::TryFolder (const DebugSourceFile & record, const std::wstring & folder, const std::wstring & fileName,
                               SourceLookup & result, std::wstring & nameOnly)
{
    std::vector<FileSystemEntry>  entries;
    std::wstring                  path;
    std::string                   text;
    HRESULT                       hr     = m_files.EnumerateEntries (folder, entries);



    if (FAILED (hr))
    {
        return false;
    }

    for (const FileSystemEntry & entry : entries)
    {
        if (entry.isFolder || _wcsicmp (entry.name.c_str(), fileName.c_str()) != 0)
        {
            continue;
        }

        path = Combine (folder, entry.name);

        if (entry.sizeBytes != record.size)
        {
            nameOnly = nameOnly.empty() ? path : nameOnly;
            continue;
        }

        hr = m_files.ReadAllText (path, text);

        if (FAILED (hr))
        {
            continue;
        }

        if (record.sha1.empty() || Sha1::ComputeTextHex (text) == record.sha1)
        {
            result.match = record.sha1.empty() ? SourceMatch::Unverified : SourceMatch::Exact;
            result.path  = path;
            result.text  = std::move (text);
            return true;
        }

        result.candidates.push_back (path);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceService::MatchDropped
//
//  By hash against every record; failing that, by name, which opens with the
//  mismatch warning; failing that, as plain text with no record at all.
//
////////////////////////////////////////////////////////////////////////////////

SourceLookup SourceService::MatchDropped (const DebugFile & file, const std::wstring & droppedPath,
                                          const std::string & programKey, int & recordIndex)
{
    SourceLookup  result;
    std::string   hash;
    std::wstring  fileName = GetFileName (droppedPath);
    HRESULT       hr       = m_files.ReadAllText (droppedPath, result.text);



    recordIndex = -1;

    if (FAILED (hr))
    {
        result.text.clear();
        return result;
    }

    result.path = droppedPath;
    hash        = Sha1::ComputeTextHex (result.text);

    for (size_t i = 0; i < file.files.size() && recordIndex < 0; i++)
    {
        if (!file.files[i].sha1.empty() && file.files[i].sha1 == hash)
        {
            recordIndex  = (int) i;
            result.match = SourceMatch::Exact;
        }
    }

    for (size_t i = 0; i < file.files.size() && recordIndex < 0; i++)
    {
        std::wstring  recorded = GetFileName (SourcePathList::Utf8ToWide (file.files[i].name));

        if (_wcsicmp (recorded.c_str(), fileName.c_str()) == 0)
        {
            recordIndex  = (int) i;
            result.match = file.files[i].sha1.empty() ? SourceMatch::Unverified : SourceMatch::Mismatch;
        }
    }

    if (recordIndex >= 0)
    {
        m_paths.AddFound (programKey, GetFolder (droppedPath));
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceService::GetProgramKey
//
////////////////////////////////////////////////////////////////////////////////

std::string SourceService::GetProgramKey (const std::string & debugFileText)
{
    return Sha1::ComputeTextHex (debugFileText);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceService::GetFolder
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourceService::GetFolder (const std::wstring & path)
{
    size_t  slash = path.find_last_of (L"\\/");



    return (slash == std::wstring::npos) ? std::wstring() : path.substr (0, slash);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceService::GetFileName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourceService::GetFileName (const std::wstring & path)
{
    size_t  slash = path.find_last_of (L"\\/");



    return (slash == std::wstring::npos) ? path : path.substr (slash + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceService::Combine
//
//  A relative path from a folder, with `.` and `..` resolved and backslash
//  separators. An absolute path is taken as it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourceService::Combine (const std::wstring & folder, const std::wstring & relative)
{
    bool                        isAbsolute = (relative.size() > 1 && relative[1] == L':') ||
                                             (!relative.empty() && (relative[0] == L'\\' || relative[0] == L'/'));
    std::wstring                joined     = isAbsolute ? relative : (folder.empty() ? relative : folder + L"\\" + relative);
    std::vector<std::wstring>   parts;
    std::wstring                part;
    std::wstring                result;



    for (size_t i = 0; i <= joined.size(); i++)
    {
        wchar_t  c = (i < joined.size()) ? joined[i] : L'\\';

        if (c != L'\\' && c != L'/')
        {
            part += c;
            continue;
        }

        if (part == L".." && !parts.empty() && parts.back() != L"..")
        {
            parts.pop_back();
        }
        else if (!part.empty() && part != L".")
        {
            parts.push_back (part);
        }

        part.clear();
    }

    for (const std::wstring & each : parts)
    {
        result += (result.empty() ? L"" : L"\\") + each;
    }

    return (!joined.empty() && (joined[0] == L'\\' || joined[0] == L'/')) ? L"\\" + result : result;
}

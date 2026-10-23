#include "Pch.h"

#include "Debugger/Project/DebugProjectStore.h"

#include "Config/IFileSystem.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectStore::DebugProjectStore
//
////////////////////////////////////////////////////////////////////////////////

DebugProjectStore::DebugProjectStore (IFileSystem & files, std::wstring folder, StampFn stamp) :
    m_files  (files),
    m_folder (std::move (folder)),
    m_stamp  (std::move (stamp))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectStore::GetAutomaticPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebugProjectStore::GetAutomaticPath (const ProjectScope & scope) const
{
    return (fs::path (m_folder) / scope.GetFileName()).native();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectStore::Open
//
//  An automatic file with a redirect opens the chosen file instead; a chosen
//  file that has gone opens an empty project in the automatic file, and the
//  notice has the chosen file's path.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugProjectStore::Open (const ProjectScope & scope, DebugProject & project, std::vector<std::string> & notices)
{
    HRESULT         hr         = S_OK;
    HRESULT         hrRead     = E_FAIL;
    HRESULT         hrParse    = E_FAIL;
    std::wstring    automatic  = GetAutomaticPath (scope);
    std::string     text;
    std::string     redirect;
    JsonValue       root;
    JsonParseError  error;
    bool            isFound    = false;
    bool            isRedirect = false;



    project = DebugProject();

    if (m_files.Exists (automatic))
    {
        hrRead = m_files.ReadAllText (automatic, text);
    }

    if (SUCCEEDED (hrRead))
    {
        hrParse = JsonParser::Parse (text, root, error);
    }

    isRedirect = SUCCEEDED (hrParse) && root.HasString (kpszRedirect, redirect);

    hr = Read (isRedirect ? TextEncoding::NarrowToWide (redirect) : automatic, project, notices, isFound);
    CHR (hr);

    if (isRedirect && !isFound)
    {
        notices.push_back (std::format ("The debug project {} is not there; an empty project is open for {}.", redirect, scope.label));
    }

    project.scope       = scope;
    project.isAutomatic = !isRedirect || !isFound;
    project.filePath    = project.isAutomatic ? automatic : TextEncoding::NarrowToWide (redirect);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectStore::OpenFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugProjectStore::OpenFile (const std::wstring & path, DebugProject & project, std::vector<std::string> & notices)
{
    HRESULT  hr      = S_OK;
    bool     isFound = false;



    hr = Read (path, project, notices, isFound);
    CHR (hr);
    CBREx (isFound, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    project.filePath    = path;
    project.isAutomatic = false;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectStore::Save
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugProjectStore::Save (const DebugProject & project)
{
    HRESULT    hr         = S_OK;
    bool       hasPath    = !project.filePath.empty();
    bool       isReadable = !project.isUnreadable;
    JsonValue  redirect = JsonValue (std::vector<std::pair<std::string, JsonValue>>
    {
        { "format",     JsonValue (std::string (DebugProject::kpszFormat)) },
        { "version",    JsonValue ((double) DebugProject::kVersion) },
        { kpszRedirect, JsonValue (TextEncoding::WideToNarrow (project.filePath)) },
    });



    CBRAEx (hasPath,    E_INVALIDARG);
    CBREx  (isReadable, E_ACCESSDENIED);

    hr = m_files.WriteAllText (project.filePath, project.ToJson (GetFolder (project.filePath)));
    CHR (hr);

    if (!project.isAutomatic)
    {
        hr = m_files.WriteAllText (GetAutomaticPath (project.scope), JsonWriter::Write (redirect));
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectStore::SaveAs
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugProjectStore::SaveAs (DebugProject & project, const std::wstring & path)
{
    DebugProject  copy = project;
    HRESULT       hr   = S_OK;



    copy.filePath     = path;
    copy.isAutomatic  = false;
    copy.isUnreadable = false;

    hr = Save (copy);
    CHR (hr);

    project = copy;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectStore::Read
//
//  A missing file is no project and no notice. A file that is not a
//  project is copied aside under a dated name before anything replaces
//  it. A file that cannot be read, as one another program has open, is
//  marked unreadable, so Save never writes the empty project over it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugProjectStore::Read (const std::wstring & path, DebugProject & project, std::vector<std::string> & notices, bool & isFound)
{
    HRESULT       hr     = S_OK;
    std::string   text;
    std::string   error;
    std::wstring  aside  = path + TextEncoding::NarrowToWide (".unreadable-" + GetStamp());



    isFound = m_files.Exists (path);
    project = DebugProject();
    BAIL_OUT_IF (!isFound, S_OK);

    hr = m_files.ReadAllText (path, text);

    if (FAILED (hr))
    {
        isFound              = false;
        project.isUnreadable = true;
        notices.push_back (std::format ("The debug project {} could not be read (error {:08X}). An empty project is open, and it is not saved over that file.",
                                        TextEncoding::WideToNarrow (path), (uint32_t) hr));
        BAIL_OUT_IF (true, S_OK);
    }

    BAIL_OUT_IF (DebugProject::TryParse (text, GetFolder (path), project, error), S_OK);

    isFound = false;
    hr      = m_files.WriteAllText (aside, text);

    notices.push_back (SUCCEEDED (hr)
                       ? std::format ("The debug project {} could not be read: {} It is copied to {}, and an empty project is open.",
                                      TextEncoding::WideToNarrow (path), error, TextEncoding::WideToNarrow (aside))
                       : std::format ("The debug project {} could not be read: {} An empty project is open.",
                                      TextEncoding::WideToNarrow (path), error));
    hr = S_OK;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectStore::GetStamp
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugProjectStore::GetStamp() const
{
    std::chrono::zoned_time  now (std::chrono::current_zone(), std::chrono::floor<std::chrono::seconds> (std::chrono::system_clock::now()));



    return m_stamp ? m_stamp() : std::format ("{:%Y%m%d-%H%M%S}", now);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectStore::GetFolder
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebugProjectStore::GetFolder (const std::wstring & path)
{
    return fs::path (path).parent_path().native();
}

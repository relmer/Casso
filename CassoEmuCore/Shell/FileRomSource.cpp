#include "Pch.h"

#include "Shell/FileRomSource.h"

#include "AssetBootstrap.h"
#include "Core/PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileRomSource::FileRomSource
//
////////////////////////////////////////////////////////////////////////////////

FileRomSource::FileRomSource()
{
    m_searchPaths = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                    PathResolver::GetWorkingDirectory());
    m_searchPaths.push_back (AssetBootstrap::GetAssetBaseDirectory());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileRomSource::GetMachineJson
//
//  The file on the search paths, which is the copy the emulator extracted
//  and the one a user's edits live in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileRomSource::GetMachineJson (const std::string & machineId, std::string & jsonText) const
{
    HRESULT                hr      = S_OK;
    std::filesystem::path  relPath = std::filesystem::path ("Machines") / machineId / (machineId + ".json");
    std::filesystem::path  found   = PathResolver::FindFile (m_searchPaths, relPath);
    std::ifstream          file;
    std::stringstream      text;
    bool                   isFound = !found.empty();
    bool                   isOpen  = false;



    jsonText.clear();

    CBREx (isFound, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    file.open (found, std::ios::binary);
    isOpen = file.is_open();
    CBREx (isOpen, HRESULT_FROM_WIN32 (ERROR_OPEN_FAILED));

    text << file.rdbuf();
    jsonText = text.str();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileRomSource::ResolveRom
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path FileRomSource::ResolveRom (const std::filesystem::path & romRelPath) const
{
    return PathResolver::FindFile (m_searchPaths, romRelPath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileRomSource::GetSearchPaths
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::filesystem::path> FileRomSource::GetSearchPaths() const
{
    return m_searchPaths;
}

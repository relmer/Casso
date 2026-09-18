#pragma once

#include "Config/IFileSystem.h"
#include "Debugger/DebugFile.h"
#include "Debugger/Source/SourcePathList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceMatch / SourceLookup
//
//  How a source file was found, and what was found.
//
//  Exact       the recorded hash matched.
//  Unverified  the record carries no hash; the name and size matched.
//  Mismatch    a file of the recorded name was found but its hash differs, or
//              no file of the recorded size was found and one of the name
//              was; it opens with a warning that lines may not match.
//  NotFound    nothing of the recorded name anywhere searched.
//
////////////////////////////////////////////////////////////////////////////////

enum class SourceMatch
{
    NotFound,
    Exact,
    Unverified,
    Mismatch,
};

struct SourceLookup
{
    SourceMatch                match = SourceMatch::NotFound;
    std::wstring               path;
    std::string                text;

    //  Every file of the recorded name and size whose hash differs, in the
    //  order searched. More than one leaves the choice to the user.
    std::vector<std::wstring>  candidates;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SourceService
//
//  Finds a debug file's source files (FR-058, FR-059, R-032).
//
//  THE ORDER IS FIXED: the path recorded relative to the debug file, then the
//  folders where this program's sources were found before, then the folders
//  where any sources were found. A candidate must match the recorded name and
//  size before it is read and hashed; the first whose hash matches is used.
//
//  A file the user drops is hashed and matched against every file record. A
//  match of either kind puts its folder at the front of both folder lists.
//
////////////////////////////////////////////////////////////////////////////////

class SourceService
{
public:
    SourceService (IFileSystem & files, SourcePathList & paths) : m_files (files), m_paths (paths) {}

    //  `programKey` is the SHA-1 of the debug file's text.
    SourceLookup  Find          (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                 const std::string & programKey);

    //  Which record a dropped file is, or -1 when it is none of them; a file
    //  that is none still comes back with its text, to be shown as plain text.
    SourceLookup  MatchDropped  (const DebugFile & file, const std::wstring & droppedPath,
                                 const std::string & programKey, int & recordIndex);

    static std::string   GetProgramKey (const std::string & debugFileText);
    static std::wstring  GetFolder     (const std::wstring & path);
    static std::wstring  GetFileName   (const std::wstring & path);
    static std::wstring  Combine       (const std::wstring & folder, const std::wstring & relative);

private:
    bool  TryFolder (const DebugSourceFile & record, const std::wstring & folder, const std::wstring & fileName,
                     SourceLookup & result, std::wstring & nameOnly);

    std::vector<std::wstring>  GetSearchFolders (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                                 const std::string & programKey) const;

    IFileSystem     & m_files;
    SourcePathList  & m_paths;
};

#pragma once

#include "Config/GlobalUserPrefs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourcePathList
//
//  The folders where the debugger has found source files, remembered in the
//  global preferences (FR-060): one list for each program, keyed by the SHA-1
//  of its debug file, and one for every program. Both are most-recent-first
//  and hold at most kMaxFolders; a folder found again moves to the front.
//
//  Folders compare without regard to case or separator, as Windows paths do.
//
////////////////////////////////////////////////////////////////////////////////

class SourcePathList
{
public:
    static constexpr size_t  kMaxFolders = 16;

    explicit SourcePathList (GlobalUserPrefs & prefs) : m_prefs (prefs) {}

    std::vector<std::wstring>  GetProgramFolders (const std::string & programKey) const;
    std::vector<std::wstring>  GetGlobalFolders  () const;

    //  A folder where a source for this program was found: to the front of
    //  both lists.
    void  AddFound (const std::string & programKey, const std::wstring & folder);

    static std::wstring  Utf8ToWide  (const std::string & text);
    static std::string   WideToUtf8  (const std::wstring & text);
    static bool          IsSameFolder (const std::wstring & a, const std::wstring & b);

private:
    static void                        PushFront (std::vector<std::string> & list, const std::string & folder);
    static std::vector<std::wstring>   ToWide    (const std::vector<std::string> & list);

    GlobalUserPrefs  & m_prefs;
};

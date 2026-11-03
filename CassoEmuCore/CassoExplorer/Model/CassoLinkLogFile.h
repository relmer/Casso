#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile
//
//  Every connection's activity and the packets on each line, appended to a
//  file a day in a folder: CassoLink-2026-11-03.log. A file older than
//  kKeepDays is deleted when the first line of a day is written. With no
//  folder set, nothing is written.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkLogFile
{
public:
    CassoLinkLogFile  () = default;
    ~CassoLinkLogFile ();

    CassoLinkLogFile (const CassoLinkLogFile &)             = delete;
    CassoLinkLogFile & operator= (const CassoLinkLogFile &) = delete;

    void  SetFolder (const std::filesystem::path & folder);

    //  One line, from any thread. A file that cannot be written is skipped.
    void  Append    (const std::string & connection, const std::string & text);

    //  The line as written: the local time to the millisecond, the
    //  connection and the text, with any line break in it as a space.
    static std::string   FormatLine  (const SYSTEMTIME & time, const std::string & connection, const std::string & text);
    static std::wstring  GetFileName (const SYSTEMTIME & time);

    //  Whether a file in the folder is a log written before the cutoff day,
    //  as yyyymmdd.
    static bool          IsExpired   (const std::wstring & fileName, int cutoffDay);

    static constexpr int  kKeepDays = 30;

private:
    void  Open  (const SYSTEMTIME & time);
    void  Prune (const SYSTEMTIME & time);

    std::filesystem::path  m_folder;
    HANDLE                 m_file = INVALID_HANDLE_VALUE;
    int                    m_day  = 0;     // the open file's day, as yyyymmdd
    std::mutex             m_lock;
};

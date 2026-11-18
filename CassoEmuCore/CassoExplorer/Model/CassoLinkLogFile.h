#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile
//
//  Every connection's activity and the packets on each line, as rows of
//  CassoLink Log.csv in a folder: the local time, the connection, what the row is
//  (Activity, Sent or Received) and the message in plain English. Rows older
//  than kKeepDays are dropped when the file is first opened and each day
//  after. With no folder set, nothing is written.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkLogFile
{
public:
    CassoLinkLogFile  () = default;
    ~CassoLinkLogFile ();

    CassoLinkLogFile (const CassoLinkLogFile &)             = delete;
    CassoLinkLogFile & operator= (const CassoLinkLogFile &) = delete;

    static constexpr const char     * kActivity = "Activity";
    static constexpr const char     * kSent     = "Sent";
    static constexpr const char     * kReceived = "Received";
    static constexpr const char     * kHeader   = "Time,Connection,Kind,Message";
    static constexpr const wchar_t  * kFileName = L"CassoLink Log.csv";
    static constexpr int              kKeepDays = 30;

    void  SetFolder (const std::filesystem::path & folder);

    //  One row, from any thread. A file that cannot be written is skipped.
    void  Append    (const std::string & connection, const char * kind, const std::string & text);

    //  The row as written, with each field quoted where CSV requires it.
    static std::string  FormatRow   (const SYSTEMTIME & time, const std::string & connection, const char * kind, const std::string & text);
    static std::string  QuoteField  (const std::string & field);
    static std::string  FormatDate  (const SYSTEMTIME & time);

    //  The rows of a file's text from the cutoff day, as yyyy-mm-dd, on, with
    //  the header and any row that doesn't begin with a date.
    static std::string  KeepFrom    (const std::string & text, const std::string & cutoffDate);

private:
    void  Open  (const SYSTEMTIME & time);
    void  Prune (const SYSTEMTIME & time);

    std::filesystem::path  m_folder;
    HANDLE                 m_file = INVALID_HANDLE_VALUE;
    int                    m_day  = 0;     // the day the file was last pruned, as yyyymmdd
    std::mutex             m_lock;
};

#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkLogFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::~CassoLinkLogFile
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkLogFile::~CassoLinkLogFile()
{
    if (m_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle (m_file);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::SetFolder
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkLogFile::SetFolder (const std::filesystem::path & folder)
{
    std::lock_guard<std::mutex>  lock (m_lock);



    m_folder = folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::Append
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkLogFile::Append (const std::string & connection, const char * kind, const std::string & text)
{
    std::lock_guard<std::mutex>  lock (m_lock);
    SYSTEMTIME                   now     = {};
    std::string                  row;
    DWORD                        written = 0;
    BOOL                         wrote   = FALSE;



    if (m_folder.empty())
    {
        return;
    }

    GetLocalTime (&now);

    //  The first row of a day drops the rows past the cutoff.
    if (m_file == INVALID_HANDLE_VALUE || m_day != now.wYear * 10000 + now.wMonth * 100 + now.wDay)
    {
        Open (now);
    }

    if (m_file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    row   = FormatRow (now, connection, kind, text);
    wrote = WriteFile (m_file, row.data(), (DWORD) row.size(), &written, nullptr);
    IGNORE_RETURN_VALUE (wrote, TRUE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::Open
//
//  The file pruned, then opened to append to, and for others to read while
//  it is written. A new file begins with the header.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkLogFile::Open (const SYSTEMTIME & time)
{
    std::error_code  ec;
    LARGE_INTEGER    size    = {};
    BOOL             sized   = FALSE;
    DWORD            written = 0;
    BOOL             wrote   = FALSE;
    std::string      header  = std::string (kHeader) + "\r\n";



    if (m_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle (m_file);
        m_file = INVALID_HANDLE_VALUE;
    }

    std::filesystem::create_directories (m_folder, ec);

    m_day = time.wYear * 10000 + time.wMonth * 100 + time.wDay;
    Prune (time);

    m_file = CreateFileW ((m_folder / kFileName).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (m_file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    sized = GetFileSizeEx (m_file, &size);

    if (sized && size.QuadPart == 0)
    {
        wrote = WriteFile (m_file, header.data(), (DWORD) header.size(), &written, nullptr);
        IGNORE_RETURN_VALUE (wrote, TRUE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::Prune
//
//  The file rewritten with the rows of the last kKeepDays, through a file
//  beside it that replaces it, so the old file is whole after a failure.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkLogFile::Prune (const SYSTEMTIME & time)
{
    FILETIME                now    = {};
    ULARGE_INTEGER          ticks  = {};
    SYSTEMTIME              cutoff = {};
    std::filesystem::path   path   = m_folder / kFileName;
    std::filesystem::path   temp   = m_folder / (std::wstring (kFileName) + L".tmp");
    std::string             text;
    std::string             kept;
    std::error_code         ec;



    {
        std::ifstream  in (path, std::ios::binary);

        if (!in)
        {
            return;
        }

        text.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
    }

    SystemTimeToFileTime (&time, &now);

    ticks.LowPart   = now.dwLowDateTime;
    ticks.HighPart  = now.dwHighDateTime;
    ticks.QuadPart -= (ULONGLONG) kKeepDays * 24 * 60 * 60 * 10000000ULL;

    now.dwLowDateTime  = ticks.LowPart;
    now.dwHighDateTime = ticks.HighPart;

    FileTimeToSystemTime (&now, &cutoff);

    kept = KeepFrom (text, FormatDate (cutoff));

    if (kept.size() == text.size())
    {
        return;
    }

    {
        std::ofstream  out (temp, std::ios::binary | std::ios::trunc);

        out.write (kept.data(), (std::streamsize) kept.size());

        if (!out)
        {
            return;
        }
    }

    std::filesystem::rename (temp, path, ec);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::FormatRow
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkLogFile::FormatRow (const SYSTEMTIME & time, const std::string & connection, const char * kind, const std::string & text)
{
    return std::format ("{} {:02}:{:02}:{:02}.{:03},{},{},{}\r\n", FormatDate (time), time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
                        QuoteField (connection), kind, QuoteField (text));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::QuoteField
//
//  A field with a comma, a quote or a line break, in quotes with each quote
//  doubled; any other field as it is.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkLogFile::QuoteField (const std::string & field)
{
    std::string  quoted = "\"";



    if (field.find_first_of (",\"\r\n") == std::string::npos)
    {
        return field;
    }

    for (char ch : field)
    {
        quoted += (ch == '"') ? std::string ("\"\"") : std::string (1, ch);
    }

    return quoted + "\"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::FormatDate
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkLogFile::FormatDate (const SYSTEMTIME & time)
{
    return std::format ("{:04}-{:02}-{:02}", time.wYear, time.wMonth, time.wDay);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::KeepFrom
//
//  A quoted message can span lines, so a line that doesn't begin with a date
//  goes with the row before it.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkLogFile::KeepFrom (const std::string & text, const std::string & cutoffDate)
{
    std::string  kept;
    size_t       at   = 0;
    bool         keep = true;



    while (at < text.size())
    {
        size_t       end   = text.find ('\n', at);
        std::string  line  = text.substr (at, (end == std::string::npos) ? std::string::npos : end + 1 - at);
        bool         dated = line.size() >= 10 && isdigit ((unsigned char) line[0]) && line[4] == '-' && line[7] == '-';

        if (dated)
        {
            keep = line.compare (0, 10, cutoffDate) >= 0;
        }

        if (keep)
        {
            kept += line;
        }

        at = (end == std::string::npos) ? text.size() : end + 1;
    }

    return kept;
}

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

void CassoLinkLogFile::Append (const std::string & connection, const std::string & text)
{
    std::lock_guard<std::mutex>  lock (m_lock);
    SYSTEMTIME                   now     = {};
    std::string                  line;
    DWORD                        written = 0;
    BOOL                         wrote   = FALSE;



    if (m_folder.empty())
    {
        return;
    }

    GetLocalTime (&now);

    if (m_file == INVALID_HANDLE_VALUE || m_day != now.wYear * 10000 + now.wMonth * 100 + now.wDay)
    {
        Open (now);
    }

    if (m_file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    line  = FormatLine (now, connection, text);
    wrote = WriteFile (m_file, line.data(), (DWORD) line.size(), &written, nullptr);
    IGNORE_RETURN_VALUE (wrote, TRUE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::Open
//
//  The day's file, appended to, and opened for others to read while it is
//  written.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkLogFile::Open (const SYSTEMTIME & time)
{
    std::error_code  ec;



    if (m_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle (m_file);
        m_file = INVALID_HANDLE_VALUE;
    }

    std::filesystem::create_directories (m_folder, ec);

    m_day  = time.wYear * 10000 + time.wMonth * 100 + time.wDay;
    m_file = CreateFileW ((m_folder / GetFileName (time)).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    Prune (time);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::Prune
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkLogFile::Prune (const SYSTEMTIME & time)
{
    FILETIME         now    = {};
    ULARGE_INTEGER   ticks  = {};
    SYSTEMTIME       cutoff = {};
    std::error_code  ec;



    SystemTimeToFileTime (&time, &now);

    ticks.LowPart   = now.dwLowDateTime;
    ticks.HighPart  = now.dwHighDateTime;
    ticks.QuadPart -= (ULONGLONG) kKeepDays * 24 * 60 * 60 * 10000000ULL;

    now.dwLowDateTime  = ticks.LowPart;
    now.dwHighDateTime = ticks.HighPart;

    FileTimeToSystemTime (&now, &cutoff);

    for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator (m_folder, ec))
    {
        if (IsExpired (entry.path().filename().wstring(), cutoff.wYear * 10000 + cutoff.wMonth * 100 + cutoff.wDay))
        {
            std::filesystem::remove (entry.path(), ec);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::FormatLine
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkLogFile::FormatLine (const SYSTEMTIME & time, const std::string & connection, const std::string & text)
{
    std::string  line = std::format ("{:02}:{:02}:{:02}.{:03}  {}  {}", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, connection, text);



    std::replace (line.begin(), line.end(), '\r', ' ');
    std::replace (line.begin(), line.end(), '\n', ' ');

    return line + "\r\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::GetFileName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkLogFile::GetFileName (const SYSTEMTIME & time)
{
    return std::format (L"CassoLink-{:04}-{:02}-{:02}.log", time.wYear, time.wMonth, time.wDay);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFile::IsExpired
//
//  Only a file with the log's name is ever expired, so nothing else in the
//  folder is deleted.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkLogFile::IsExpired (const std::wstring & fileName, int cutoffDay)
{
    int  year  = 0;
    int  month = 0;
    int  day   = 0;
    int  read  = 0;



    if (fileName.size() != std::wstring (L"CassoLink-0000-00-00.log").size() || !fileName.starts_with (L"CassoLink-") || !fileName.ends_with (L".log"))
    {
        return false;
    }

    read = swscanf_s (fileName.c_str(), L"CassoLink-%4d-%2d-%2d.log", &year, &month, &day);

    return read == 3 && year * 10000 + month * 100 + day < cutoffDay;
}

#include "Pch.h"

#include "Core/WindowTrace.h"




static std::wstring  s_path;
static bool          s_looked = false;





////////////////////////////////////////////////////////////////////////////////
//
//  WindowTrace::IsOn
//
//  The environment is read once. A path that is set after the process starts
//  does not turn the log on mid-run, which keeps every caller's cost to one
//  bool.
//
////////////////////////////////////////////////////////////////////////////////

bool WindowTrace::IsOn()
{
    wchar_t  buffer[MAX_PATH] = {};
    DWORD    written          = 0;



    if (!s_looked)
    {
        s_looked = true;
        written  = GetEnvironmentVariableW (kPathVariable, buffer, (DWORD) _countof (buffer));

        if (written > 0 && written < _countof (buffer))
        {
            s_path = buffer;
        }
    }

    return !s_path.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowTrace::Write
//
//  Opened and closed per line. The log is a diagnostic read while the program
//  runs, often from another process, and a held handle would keep the last
//  lines out of it -- which is exactly where a placement fault ends up.
//
////////////////////////////////////////////////////////////////////////////////

void WindowTrace::Write (const std::string & line)
{
    SYSTEMTIME     now = {};
    std::ofstream  file (s_path, std::ios::app);
    char           stamp[64] = {};



    if (!file.is_open())
    {
        return;
    }

    GetLocalTime (&now);
    sprintf_s (stamp, _countof (stamp), "%02u:%02u:%02u.%03u pid=%-6lu",
               now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, GetCurrentProcessId());

    file << stamp << " " << line << "\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowTrace::Log
//
////////////////////////////////////////////////////////////////////////////////

void WindowTrace::Log (const char * event, const char * who, const std::string & note)
{
    std::ostringstream  line;
    std::string         tag  = event;
    std::string         name = who;



    if (!IsOn())
    {
        return;
    }

    tag.append  ((tag.size()  < 22) ? 22 - tag.size()  : 1, ' ');
    name.append ((name.size() < 9)  ? 9  - name.size() : 1, ' ');

    line << tag << " " << name << " " << note;
    Write (line.str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowTrace::LogRect
//
////////////////////////////////////////////////////////////////////////////////

void WindowTrace::LogRect (const char * event, const char * who, const RECT & rect, const std::string & note)
{
    LogRect (event, who, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, note);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowTrace::LogRect
//
////////////////////////////////////////////////////////////////////////////////

void WindowTrace::LogRect (const char * event, const char * who, long x, long y, long w, long h,
                           const std::string & note)
{
    std::ostringstream  text;



    if (!IsOn())
    {
        return;
    }

    text << "x=" << x << " y=" << y << " w=" << w << " h=" << h;

    if (!note.empty())
    {
        text << "  " << note;
    }

    Log (event, who, text.str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowTrace::LogWindow
//
////////////////////////////////////////////////////////////////////////////////

void WindowTrace::LogWindow (const char * event, const char * who, HWND hwnd, const std::string & note)
{
    RECT                rect = {};
    std::ostringstream  text;



    if (!IsOn() || hwnd == nullptr || !GetWindowRect (hwnd, &rect))
    {
        return;
    }

    text << "dpi=" << GetDpiForWindow (hwnd);

    if (IsZoomed (hwnd))  { text << " maximized"; }
    if (IsIconic (hwnd))  { text << " minimized"; }

    if (!note.empty())
    {
        text << "  " << note;
    }

    LogRect (event, who, rect, text.str());
}

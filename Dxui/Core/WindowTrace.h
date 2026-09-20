#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  WindowTrace
//
//  A log of every rect a window is given and every rect anyone writes down,
//  for the one class of fault that cannot be reasoned about from the code: a
//  window that does not come back where it was left. Placement runs through
//  four parties -- the shell that asks for a rect, the OS that may answer
//  with another, the preferences file that two processes write, and the
//  restore that reads it -- and only a record of all four in order shows
//  which of them changed the number.
//
//  Off unless CASSO_WINDOW_LOG names a file, so a shipped build pays a
//  string compare on an environment variable once.
//
//  Each line is: time, process, event, window, rect, and a note. One event
//  per line, appended, so two running instances interleave into one story.
//
////////////////////////////////////////////////////////////////////////////////

class WindowTrace
{
public:

    //  The environment variable that names the log file.
    static constexpr const wchar_t *  kPathVariable = L"CASSO_WINDOW_LOG";

    static bool  IsOn ();

    //  `who` names the window (main, debugger); `event` names what happened
    //  (create.request, create.actual, move.end, save, restore.hit ...).
    static void  Log     (const char * event, const char * who, const std::string & note);
    static void  LogRect  (const char * event, const char * who, const RECT & rect, const std::string & note);
    static void  LogRect  (const char * event, const char * who, long x, long y, long w, long h,
                           const std::string & note);

    //  The rect of a live window, with its DPI, which is the number the OS
    //  will have had the last word on.
    static void  LogWindow (const char * event, const char * who, HWND hwnd, const std::string & note);

private:

    static void  Write (const std::string & line);
};

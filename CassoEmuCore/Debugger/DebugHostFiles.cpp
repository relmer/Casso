#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Debugger/Channel/HostAccessGuard.h"
#include "Debugger/DebugHostFiles.h"
#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHostFiles::StripQuotes
//
//  The path typed, without surrounding white space or the matching quotes
//  around it.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugHostFiles::StripQuotes (const std::string & path)
{
    size_t       first = path.find_first_not_of (" \t\r\n");
    size_t       last  = path.find_last_not_of  (" \t\r\n");
    std::string  bare  = (first == std::string::npos) ? std::string() : path.substr (first, last - first + 1);



    if (bare.size() >= 2 && (bare.front() == '"' || bare.front() == '\'') && bare.back() == bare.front())
    {
        bare = bare.substr (1, bare.size() - 2);
    }

    return bare;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHostFiles::ResolvePath
//
//  Quotes around the name are dropped. A rooted path is used as given; any
//  other is taken from the directory CD set or, with none, the process's.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebugHostFiles::ResolvePath (const std::string & path) const
{
    std::wstring     wide      = TextEncoding::NarrowToWide (StripQuotes (path));
    std::wstring     directory = m_currentDirectory;
    std::error_code  code;
    bool             isRooted  = wide.starts_with (L'\\') || wide.starts_with (L'/') || (wide.size() > 1 && wide[1] == L':');



    if (isRooted || wide.empty())
    {
        return wide;
    }

    //  With no directory set by CD, the process's own is the one a relative
    //  path is opened from, so it is given in full.
    if (directory.empty())
    {
        directory = std::filesystem::current_path (code).wstring();
    }

    if (directory.empty())
    {
        return wide;
    }

    return directory + (directory.ends_with (L'\\') ? L"" : L"\\") + wide;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHostFiles::GetPathEcho
//
//  A path that resolved to something other than what was typed is the
//  relative one; a rooted path resolves to itself, less any quotes.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugHostFiles::GetPathEcho (const std::string & path, const char * preposition) const
{
    std::string   bare     = StripQuotes (path);
    std::wstring  resolved = ResolvePath (path);



    if (resolved.empty() || resolved == TextEncoding::NarrowToWide (bare))
    {
        return std::string();
    }

    return std::format (" {} {}", preposition, TextEncoding::WideToNarrow (resolved));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHostFiles::GetPathEcho
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugHostFiles::GetPathEcho (const std::string & path, const char * preposition, Reply & reply) const
{
    std::string  echo = GetPathEcho (path, preposition);



    if (!echo.empty())
    {
        reply.files.push_back (TextEncoding::WideToNarrow (ResolvePath (path)));
    }

    return echo;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHostFiles::GetFullPath
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugHostFiles::GetFullPath (const std::string & path) const
{
    std::wstring  resolved = ResolvePath (path);



    return resolved.empty() ? path : TextEncoding::WideToNarrow (resolved);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHostFiles::CheckHostAccess
//
////////////////////////////////////////////////////////////////////////////////

bool DebugHostFiles::CheckHostAccess (const std::wstring & path, HostAccess access, Reply & reply)
{
    if (m_hostAccessGuard == nullptr || m_hostAccessGuard->TryAccess (path, access))
    {
        return true;
    }

    reply.SetError (CommandStatus::Error, HostAccessGuard::kFailedLabel, m_hostAccessGuard->GetLastReason());
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHostFiles::NoteScriptRun
//
//  Adds the console line for a file of commands a channel line runs. The
//  guarded file system checks reading the file, and each command in it is
//  checked as a channel line is.
//
////////////////////////////////////////////////////////////////////////////////

void DebugHostFiles::NoteScriptRun (const std::wstring & path)
{
    if (m_hostAccessGuard != nullptr)
    {
        m_hostAccessGuard->ListScriptRun (path);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHostFiles::GetHostAccessFailures
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DebugHostFiles::GetHostAccessFailures() const
{
    return (m_hostAccessGuard != nullptr) ? m_hostAccessGuard->GetFailureCount() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHostFiles::ApplyHostAccessFailure
//
//  A command whose file access failed through the guarded file system
//  replies with the policy's reason in place of the command's failure text,
//  which reports only that the file could not be read or written. A reply already
//  formatted, as a script's is, has the reason in its lines.
//
////////////////////////////////////////////////////////////////////////////////

void DebugHostFiles::ApplyHostAccessFailure (uint64_t failuresBefore, Reply & reply) const
{
    if (m_hostAccessGuard == nullptr || reply.isFormatted || m_hostAccessGuard->GetFailureCount() == failuresBefore)
    {
        return;
    }

    reply.SetError (CommandStatus::Error, HostAccessGuard::kFailedLabel, m_hostAccessGuard->GetLastReason());
}

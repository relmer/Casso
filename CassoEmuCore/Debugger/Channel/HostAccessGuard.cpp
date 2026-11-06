#include "Pch.h"

#include "Debugger/Channel/HostAccessGuard.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/ConsoleLinks.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessGuard::HostAccessGuard
//
////////////////////////////////////////////////////////////////////////////////

HostAccessGuard::HostAccessGuard (HostAccessPolicy & policy, const std::string & clientName, bool overwrite) :
    m_policy     (policy),
    m_clientName (clientName.empty() ? std::string (kDefaultClientName) : clientName),
    m_overwrite  (overwrite)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessGuard::TryAccess
//
////////////////////////////////////////////////////////////////////////////////

bool HostAccessGuard::TryAccess (const std::wstring & path, HostAccess access)
{
    bool               isCreated = m_created.contains (GetKey (path));
    HostAccessVerdict  verdict   = m_policy.Check (path, access, m_overwrite || isCreated);
    bool               isListed  = false;



    isListed = AddAccessLine (access == HostAccess::Read ? "read" : access == HostAccess::Write ? "write" : "delete", verdict.fullPath);

    if (!verdict.isAllowed)
    {
        Fail (verdict.reason, isListed);
    }

    return verdict.isAllowed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessGuard::IsReadable
//
////////////////////////////////////////////////////////////////////////////////

bool HostAccessGuard::IsReadable (const std::wstring & path)
{
    return m_policy.Check (path, HostAccess::Read, false).isAllowed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessGuard::ListScriptRun
//
//  Adds the console line for a file of commands the line runs. Reading the file
//  is checked by TryAccess, and so is each command in it.
//
////////////////////////////////////////////////////////////////////////////////

void HostAccessGuard::ListScriptRun (const std::wstring & path)
{
    AddAccessLine ("run", path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessGuard::NoteCreated
//
////////////////////////////////////////////////////////////////////////////////

void HostAccessGuard::NoteCreated (const std::wstring & path)
{
    m_created.insert (GetKey (path));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessGuard::TakeConsoleLines
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> HostAccessGuard::TakeConsoleLines()
{
    std::vector<std::string>  lines;



    lines.swap (m_consoleLines);
    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessGuard::AddAccessLine
//
//  One line for each path and kind of access in a line: writing a file in
//  pieces is one access. False when the access was already listed.
//
////////////////////////////////////////////////////////////////////////////////

bool HostAccessGuard::AddAccessLine (const char * verb, const std::wstring & path)
{
    std::string  narrow = TextEncoding::WideToNarrow (path);
    std::wstring key    = TextEncoding::NarrowToWide (verb) + L" " + GetKey (path);



    if (!m_listed.insert (key).second)
    {
        return false;
    }

    m_consoleLines.push_back (std::format ("{} {} {}", m_clientName, verb, ConsoleLinks::MakeLink (narrow, narrow)));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessGuard::Fail
//
////////////////////////////////////////////////////////////////////////////////

void HostAccessGuard::Fail (const std::string & reason, bool isListed)
{
    m_failureCount++;
    m_lastReason = reason;

    if (isListed)
    {
        m_consoleLines.push_back ("Failed: " + reason);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessGuard::GetKey
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HostAccessGuard::GetKey (const std::wstring & path)
{
    std::wstring  key = path;



    for (wchar_t & ch : key)
    {
        ch = (ch == L'/') ? L'\\' : (wchar_t) towlower (ch);
    }

    return key;
}

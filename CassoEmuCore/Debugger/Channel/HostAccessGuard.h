#pragma once

#include "Debugger/Channel/HostAccessPolicy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessGuard
//
//  The host file access of one line from one debug channel client. Each
//  access is checked against the client's policy and listed for the
//  console, a failure with its reason. The last reason is stored for the
//  reply of the command that failed.
//
//  A file the line created may be written again and deleted without the
//  overwrite option, so a command that creates a file to test that it can
//  be written, and removes it, works.
//
////////////////////////////////////////////////////////////////////////////////

class HostAccessGuard
{
public:
    static constexpr const char * kDefaultClientName = "Debug channel client";
    static constexpr const char * kFailedLabel       = "file access denied";

    HostAccessGuard (HostAccessPolicy & policy, const std::string & clientName, bool allowScripts, bool overwrite);

    //  Whether the access is allowed, with a console line for it.
    bool  TryAccess        (const std::wstring & path, HostAccess access);

    //  Whether the path may be read, with no console line: for a test of
    //  whether a file exists.
    bool  IsReadable       (const std::wstring & path);

    //  Whether a file of commands may run, with console lines for it.
    bool  TryRunScript     (const std::wstring & path);

    void  NoteCreated      (const std::wstring & path);

    uint64_t                         GetFailureCount  () const { return m_failureCount; }
    const std::string              & GetLastReason    () const { return m_lastReason; }
    const std::string              & GetClientName    () const { return m_clientName; }
    std::vector<std::string>         TakeConsoleLines ();

    static std::string  GetScriptReason ();

private:
    bool  AddAccessLine (const char * verb, const std::wstring & path);
    void  Fail          (const std::string & reason, bool isListed);

    static std::wstring  GetKey (const std::wstring & path);

    HostAccessPolicy                     & m_policy;
    std::string                            m_clientName;
    bool                                   m_allowScripts = false;
    bool                                   m_overwrite    = false;
    uint64_t                               m_failureCount = 0;
    std::string                            m_lastReason;
    std::vector<std::string>               m_consoleLines;
    std::set<std::wstring>                 m_listed;
    std::set<std::wstring>                 m_created;
};

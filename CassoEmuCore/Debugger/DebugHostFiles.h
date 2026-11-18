#pragma once

class HostAccessGuard;
class IFileSystem;
struct Reply;
enum class HostAccess;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHostFiles
//
//  A debug session's access to host files: the injected file system, the
//  directory CD set, and the access rules of the channel client whose line
//  is running. Relative paths typed to a command resolve here, and a path
//  echoed in a reply is the one the command opened.
//
////////////////////////////////////////////////////////////////////////////////

class DebugHostFiles
{
public:
    void                  SetFileSystem       (IFileSystem * fileSystem)        { m_fileSystem = fileSystem; }
    IFileSystem         * GetFileSystem       () const                          { return m_fileSystem; }
    const std::wstring  & GetCurrentDirectory () const                          { return m_currentDirectory; }
    void                  SetCurrentDirectory (const std::wstring & directory)  { m_currentDirectory = directory; }
    void                  SetHostAccessGuard  (HostAccessGuard * guard)         { m_hostAccessGuard = guard; }
    HostAccessGuard     * GetHostAccessGuard  () const                          { return m_hostAccessGuard; }

    std::wstring          ResolvePath            (const std::string & path) const;
    std::string           GetPathEcho            (const std::string & path, const char * preposition) const;
    std::string           GetPathEcho            (const std::string & path, const char * preposition, Reply & reply) const;
    std::string           GetFullPath            (const std::string & path) const;
    bool                  CheckHostAccess        (const std::wstring & path, HostAccess access, Reply & reply);
    void                  NoteScriptRun          (const std::wstring & path);

    //  The guard's failure count before a command runs, and the reply's
    //  error replaced with the guard's reason when the count rose.
    uint64_t              GetHostAccessFailures  () const;
    void                  ApplyHostAccessFailure (uint64_t failuresBefore, Reply & reply) const;

private:
    static std::string    StripQuotes            (const std::string & path);

    IFileSystem      * m_fileSystem      = nullptr;
    HostAccessGuard  * m_hostAccessGuard = nullptr;
    std::wstring       m_currentDirectory;
};

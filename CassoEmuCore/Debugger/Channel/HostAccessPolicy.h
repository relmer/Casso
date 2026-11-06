#pragma once

#include "Pch.h"

class IHostPathResolver;





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccess
//
////////////////////////////////////////////////////////////////////////////////

enum class HostAccess
{
    Read,
    Write,
    Delete,
};





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessVerdict
//
//  `fullPath` is the path the access resolved to, links followed, or the
//  path as given when it could not be resolved. `reason` is empty when the
//  access is allowed.
//
////////////////////////////////////////////////////////////////////////////////

struct HostAccessVerdict
{
    bool          isAllowed = false;
    std::wstring  fullPath;
    std::string   reason;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy
//
//  Which host files a command from the debug channel may read and write.
//
//  A path is allowed when, once `.`, `..` and every link and junction are
//  resolved, it lies under an allowed folder: one of `allowedFolders` (the
//  folders of the mounted disk images and the loaded debug and symbol
//  files, and any `CassoCli mcp --allow-path` folder) or Documents\Casso.
//  A UNC, network or device path always fails. A write also fails for a
//  program or shortcut extension, for a path under a Startup folder, and,
//  without `overwrite`, for a file that exists.
//
//  The decisions are pure. Every host question goes through the resolver.
//
////////////////////////////////////////////////////////////////////////////////

class HostAccessPolicy
{
public:
    HostAccessPolicy (IHostPathResolver & resolver, std::vector<std::wstring> allowedFolders);

    HostAccessVerdict  Check (const std::wstring & path, HostAccess access, bool overwrite);

    //  The resolved allowed folders, Documents\Casso among them.
    const std::vector<std::wstring> &  GetAllowedFolders ();

    //  Whether `path` is `folder` or lies under it, ignoring case.
    static bool          IsUnder             (const std::wstring & path, const std::wstring & folder);
    static bool          HasBlockedExtension (const std::wstring & path, std::wstring & extension);

    //  A reason for a path that is not a plain full path on a local drive:
    //  UNC and network paths, device paths, and relative, drive-relative or
    //  stream paths. Empty for a plain one.
    static std::string   GetPathFormReason   (const std::wstring & path);

private:
    bool          TryResolve      (const std::wstring & path, std::wstring & fullPath, std::string & reason);
    std::wstring  ResolveFolder   (const std::wstring & folder);
    void          ResolveFolders  ();
    bool          IsInStartup     (const std::wstring & fullPath);

    static std::string   Narrow   (const std::wstring & text);

    IHostPathResolver          & m_resolver;
    std::vector<std::wstring>    m_givenFolders;
    std::vector<std::wstring>    m_allowedFolders;
    std::vector<std::wstring>    m_startupFolders;
    bool                         m_isResolved = false;
};

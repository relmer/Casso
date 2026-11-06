#include "Pch.h"

#include "Debugger/Channel/HostAccessPolicy.h"

#include "Core/TextEncoding.h"
#include "Debugger/Channel/IHostPathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy::HostAccessPolicy
//
//  The folders are resolved at the first check rather than here, so a
//  command that doesn't access a file doesn't make file system calls.
//
////////////////////////////////////////////////////////////////////////////////

HostAccessPolicy::HostAccessPolicy (IHostPathResolver & resolver, std::vector<std::wstring> allowedFolders) :
    m_resolver     (resolver),
    m_givenFolders (std::move (allowedFolders))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy::Check
//
//  The path's form first, then where it resolves to: the blocked extensions
//  and the Startup folders for a write, the allowed folders, and last the
//  overwrite rule, so a reason reports the first rule the access breaks.
//
////////////////////////////////////////////////////////////////////////////////

HostAccessVerdict HostAccessPolicy::Check (const std::wstring & path, HostAccess access, bool overwrite)
{
    HostAccessVerdict  verdict;
    std::wstring       fullPath;
    std::wstring       extension;
    std::string        reason;
    bool               isInside  = false;
    bool               isWrite   = access != HostAccess::Read;



    verdict.fullPath = path;

    if (!TryResolve (path, fullPath, reason))
    {
        verdict.reason = reason;
        return verdict;
    }

    verdict.fullPath = fullPath;
    ResolveFolders();

    if (isWrite && (HasBlockedExtension (fullPath, extension) || HasBlockedExtension (path, extension)))
    {
        verdict.reason = std::format ("Writing {} files is not allowed.", Narrow (extension));
        return verdict;
    }

    if (isWrite && IsInStartup (fullPath))
    {
        verdict.reason = std::format ("{} is in a Startup folder.", Narrow (fullPath));
        return verdict;
    }

    for (const std::wstring & folder : m_allowedFolders)
    {
        isInside = isInside || IsUnder (fullPath, folder);
    }

    if (!isInside)
    {
        verdict.reason = std::format ("{} is outside the allowed folders.", Narrow (fullPath));
        return verdict;
    }

    if (isWrite && !overwrite && m_resolver.Exists (fullPath))
    {
        verdict.reason = std::format ("{} already exists. Set the overwrite option to {} it.",
                                      Narrow (fullPath), access == HostAccess::Delete ? "delete" : "replace");
        return verdict;
    }

    verdict.isAllowed = true;
    return verdict;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy::GetAllowedFolders
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::wstring> & HostAccessPolicy::GetAllowedFolders()
{
    ResolveFolders();
    return m_allowedFolders;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy::GetPathFormReason
//
//  `\\server\share`, `\\?\UNC\` and `//server` are network paths; `\\.\` and
//  any other `\\?\` path are device paths. A path also has to start with a
//  drive letter and a separator, and has no colon after the drive, so an
//  alternate data stream such as `out.txt:run.exe` fails too.
//
////////////////////////////////////////////////////////////////////////////////

std::string HostAccessPolicy::GetPathFormReason (const std::wstring & path)
{
    static constexpr std::wstring_view  kUncPrefix    = L"\\\\?\\UNC\\";
    static constexpr std::wstring_view  kDevicePrefix = L"\\\\.\\";
    static constexpr std::wstring_view  kLongPrefix   = L"\\\\?\\";
    static constexpr std::wstring_view  kBadChars     = L":*?\"<>|";
    static constexpr size_t             kRootLength   = 3;
    bool                                isSeparated   = false;



    if (path.empty())
    {
        return "The path is empty.";
    }

    if (path.starts_with (kUncPrefix) || path.starts_with (L"//") ||
        (path.starts_with (L"\\\\") && !path.starts_with (kDevicePrefix) && !path.starts_with (kLongPrefix)))
    {
        return std::format ("{} is a network path.", Narrow (path));
    }

    if (path.starts_with (kDevicePrefix) || path.starts_with (kLongPrefix))
    {
        return std::format ("{} is a device path.", Narrow (path));
    }

    isSeparated = path.size() >= kRootLength && (path[kRootLength - 1] == L'\\' || path[kRootLength - 1] == L'/');

    if (!isSeparated || !iswalpha (path[0]) || path[1] != L':')
    {
        return std::format ("{} isn't a full path.", Narrow (path));
    }

    for (size_t at = kRootLength; at < path.size(); at++)
    {
        if (kBadChars.find (path[at]) != std::wstring_view::npos || path[at] < L' ')
        {
            return std::format ("{} isn't a plain file path.", Narrow (path));
        }
    }

    return std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy::IsUnder
//
////////////////////////////////////////////////////////////////////////////////

bool HostAccessPolicy::IsUnder (const std::wstring & path, const std::wstring & folder)
{
    static constexpr size_t  kRootLength = 3;
    std::wstring             base        = folder;



    while (base.size() > kRootLength && (base.back() == L'\\' || base.back() == L'/'))
    {
        base.pop_back();
    }

    if (base.empty() || path.size() < base.size())
    {
        return false;
    }

    for (size_t at = 0; at < base.size(); at++)
    {
        if (towupper (path[at]) != towupper (base[at]))
        {
            return false;
        }
    }

    return path.size() == base.size() || base.back() == L'\\' || path[base.size()] == L'\\';
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy::HasBlockedExtension
//
//  Windows drops trailing dots and spaces from a file name, so `run.bat.`
//  is created as `run.bat`; the extension is taken after they are dropped.
//
////////////////////////////////////////////////////////////////////////////////

bool HostAccessPolicy::HasBlockedExtension (const std::wstring & path, std::wstring & extension)
{
    static constexpr std::wstring_view  kBlocked[] = { L".exe", L".dll", L".bat", L".cmd", L".ps1", L".vbs", L".js", L".lnk", L".scr" };
    size_t                              slash      = path.find_last_of (L"\\/");
    std::wstring                        name       = path.substr (slash == std::wstring::npos ? 0 : slash + 1);
    size_t                              dot        = 0;



    while (!name.empty() && (name.back() == L'.' || name.back() == L' '))
    {
        name.pop_back();
    }

    dot = name.find_last_of (L'.');

    if (dot == std::wstring::npos)
    {
        return false;
    }

    extension = name.substr (dot);

    for (wchar_t & ch : extension)
    {
        ch = towlower (ch);
    }

    return std::ranges::find (kBlocked, std::wstring_view (extension)) != std::end (kBlocked);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy::TryResolve
//
//  `.` and `..` are resolved on the text; then the deepest part of the path
//  that exists is resolved by the resolver, which follows its links and
//  junctions, and the parts that don't exist yet are added back. A file a
//  write creates is therefore checked where it will be, not where its name
//  points. A path that resolves to the network fails as a network path.
//
////////////////////////////////////////////////////////////////////////////////

bool HostAccessPolicy::TryResolve (const std::wstring & path, std::wstring & fullPath, std::string & reason)
{
    static constexpr size_t     kRootLength = 3;
    std::wstring                current;
    std::wstring                finalPath;
    std::vector<std::wstring>   tail;
    size_t                      slash       = 0;
    bool                        isFound     = false;



    reason = GetPathFormReason (path);

    if (!reason.empty())
    {
        return false;
    }

    current = std::filesystem::path (path).make_preferred().lexically_normal().wstring();

    while (current.size() > kRootLength && current.back() == L'\\')
    {
        current.pop_back();
    }

    while (!isFound)
    {
        isFound = m_resolver.TryGetFinalPath (current, finalPath);

        if (isFound || current.size() <= kRootLength)
        {
            break;
        }

        slash = current.find_last_of (L'\\');
        tail.insert (tail.begin(), current.substr (slash + 1));
        current = (slash < kRootLength) ? current.substr (0, kRootLength) : current.substr (0, slash);
    }

    fullPath = isFound ? finalPath : current;

    for (const std::wstring & part : tail)
    {
        fullPath += (fullPath.ends_with (L'\\') ? L"" : L"\\") + part;
    }

    if (fullPath.starts_with (L"\\\\"))
    {
        reason = std::format ("{} is a network path.", Narrow (path));
        return false;
    }

    reason = GetPathFormReason (fullPath);
    return reason.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy::ResolveFolder
//
//  Empty for a folder that is not a plain local path, which then allows
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HostAccessPolicy::ResolveFolder (const std::wstring & folder)
{
    std::wstring  fullPath;
    std::string   reason;



    if (!TryResolve (folder, fullPath, reason))
    {
        return std::wstring();
    }

    return fullPath;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy::ResolveFolders
//
////////////////////////////////////////////////////////////////////////////////

void HostAccessPolicy::ResolveFolders()
{
    std::vector<std::wstring>  folders = m_givenFolders;
    std::wstring               known;
    std::wstring               resolved;



    if (m_isResolved)
    {
        return;
    }

    m_isResolved = true;

    if (m_resolver.TryGetKnownFolder (HostKnownFolder::Documents, known))
    {
        folders.push_back (known + (known.ends_with (L'\\') ? L"" : L"\\") + L"Casso");
    }

    for (const std::wstring & folder : folders)
    {
        resolved = ResolveFolder (folder);

        if (!resolved.empty())
        {
            m_allowedFolders.push_back (resolved);
        }
    }

    for (HostKnownFolder startup : { HostKnownFolder::Startup, HostKnownFolder::CommonStartup })
    {
        if (m_resolver.TryGetKnownFolder (startup, known))
        {
            resolved = ResolveFolder (known);

            if (!resolved.empty())
            {
                m_startupFolders.push_back (resolved);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy::IsInStartup
//
////////////////////////////////////////////////////////////////////////////////

bool HostAccessPolicy::IsInStartup (const std::wstring & fullPath)
{
    for (const std::wstring & folder : m_startupFolders)
    {
        if (IsUnder (fullPath, folder))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAccessPolicy::Narrow
//
////////////////////////////////////////////////////////////////////////////////

std::string HostAccessPolicy::Narrow (const std::wstring & text)
{
    return TextEncoding::WideToNarrow (text);
}

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserArchives.h"
#include "CassoExplorer/Browser/BrowserSelection.h"
#include "CassoExplorer/Model/ArchiveProbe.h"
#include "CassoExplorer/Model/BrowserModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserArchives::BrowserArchives
//
////////////////////////////////////////////////////////////////////////////////

BrowserArchives::BrowserArchives (const BrowserParts & parts, const BrowserSelection & selection)
    : m_parts     (parts),
      m_selection (selection)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserArchives::IsLocalArchivePath
//
//  A path on a lettered drive that the shell, not the file system, lists:
//  an archive or a folder inside one.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserArchives::IsLocalArchivePath (const std::wstring & id) const
{
    return m_parts.sources.shellVerbs != nullptr && id.size() > 3 && id[1] == L':' && id[2] == L'\\'
        && !m_parts.fs.DirectoryExists (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserArchives::TryGetFileSize
//
//  A file's size as its folder lists it. False for a file the folder does not
//  list.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserArchives::TryGetFileSize (const std::wstring & path, uintmax_t & outSize) const
{
    size_t                        separator = path.find_last_of (L"\\/");
    std::vector<FileSystemEntry>  entries;
    HRESULT                       hr        = S_OK;
    bool                          found     = false;



    if (separator == std::wstring::npos)
    {
        return false;
    }

    hr = m_parts.fs.EnumerateAllEntries (path.substr (0, separator), entries);

    if (FAILED (hr))
    {
        return false;
    }

    for (const FileSystemEntry & entry : entries)
    {
        if (!found && !entry.isFolder && _wcsicmp (entry.name.c_str(), path.c_str() + separator + 1) == 0)
        {
            outSize = entry.sizeBytes;
            found   = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserArchives::DescribeEmptyArchive
//
//  Read whole, up to a size past which a damaged file is more likely than
//  an archive worth probing, and judged by its directory.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserArchives::DescribeEmptyArchive (const std::wstring & path) const
{
    uintmax_t     size    = 0;
    bool          hasSize = TryGetFileSize (path, size);
    std::string   bytes;
    std::wstring  name    = path.substr (path.find_last_of (L"\\/") + 1);
    HRESULT       hr      = S_OK;



    if (hasSize && size > kArchiveProbeMaxBytes)
    {
        return ArchiveProbe::GetReason (name, ArchiveProbe::Verdict::NotZip);
    }

    hr = m_parts.fs.ReadAllText (path, bytes);

    if (FAILED (hr))
    {
        return ArchiveProbe::GetReason (name, ArchiveProbe::Verdict::NotZip);
    }

    return ArchiveProbe::GetReason (name, ArchiveProbe::ProbeZip (std::span<const Byte> ((const Byte *) bytes.data(), bytes.size())));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserArchives::GetExtractTarget
//
//  The archive Extract all applies to: the one selected alone in a host
//  folder, or the one open, found as the first part of the open path that is
//  a file. Empty when no archive applies.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserArchives::GetExtractTarget() const
{
    std::vector<std::wstring>  paths;
    std::wstring               prefix;
    const Location           & location = GetLocation();



    if (location.kind == Location::Kind::HostFolder && m_parts.list.selectedRows.size() == 1
        && m_parts.list.rows[(size_t) m_parts.list.selectedRows[0]].isArchive)
    {
        m_selection.GetSelectedHostPaths (paths);
        return paths.empty() ? std::wstring() : paths[0];
    }

    if (location.kind != Location::Kind::ShellFolder || !IsLocalArchivePath (location.path))
    {
        return std::wstring();
    }

    for (size_t at = location.path.find (L'\\', 3); ; at = location.path.find (L'\\', at + 1))
    {
        prefix = location.path.substr (0, at);

        if (m_parts.fs.Exists (prefix) && !m_parts.fs.DirectoryExists (prefix))
        {
            return prefix;
        }

        if (at == std::wstring::npos)
        {
            return std::wstring();
        }
    }
}

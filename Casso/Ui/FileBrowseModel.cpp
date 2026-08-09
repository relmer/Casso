#include "Pch.h"

#include "FileBrowseModel.h"





//  Legacy device names Windows refuses as file-name stems.
static constexpr const wchar_t *  s_kReservedNames[] =
{
    L"CON", L"PRN", L"AUX", L"NUL",
    L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
    L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9",
};





////////////////////////////////////////////////////////////////////////////////
//
//  ToLower
//
//  ASCII-range lowering is enough here: it feeds case-insensitive PATH
//  comparisons (drive letters, extensions) and name sorts, matching the
//  normalization InMemoryFileSystem and NTFS defaults apply.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileBrowseModel::ToLower (const std::wstring & s)
{
    std::wstring  result = s;



    for (wchar_t & ch : result)
    {
        if (ch >= L'A' && ch <= L'Z')
        {
            ch = (wchar_t) (ch - L'A' + L'a');
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NormalizeForCompare
//
//  Case-lowered with separators unified, so the same file reached via either
//  separator style compares equal.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileBrowseModel::NormalizeForCompare (const std::wstring & path)
{
    std::wstring  result = ToLower (path);



    for (wchar_t & ch : result)
    {
        if (ch == L'/')
        {
            ch = L'\\';
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Bind
//
////////////////////////////////////////////////////////////////////////////////

void FileBrowseModel::Bind (IFileSystem * fs)
{
    m_fs = fs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFolder
//
//  Points the model at an absolute folder and lists it. The folder is stored
//  without a trailing separator (except a bare drive root) so composed paths
//  stay canonical.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBrowseModel::SetFolder (const std::wstring & absolute)
{
    HRESULT       hr        = S_OK;
    std::wstring  folder    = absolute;
    bool          hasFolder = !absolute.empty();



    CBRA (m_fs != nullptr);
    CBRAEx (hasFolder, E_INVALIDARG);

    while (folder.size() > 3 && (folder.back() == L'\\' || folder.back() == L'/'))
    {
        folder.pop_back();
    }

    m_folder = folder;

    hr = Refresh();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NavigateInto
//
//  Folder navigation; not yet implemented.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBrowseModel::NavigateInto (size_t entryIndex)
{
    UNREFERENCED_PARAMETER (entryIndex);

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NavigateUp
//
//  Folder navigation; not yet implemented.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBrowseModel::NavigateUp()
{
    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Refresh
//
//  Relists the current folder into the unfiltered cache, then rebuilds the
//  filtered view. Folders sort first, then files, each group case-insensitive
//  by name -- the fixed presentation order the dialog's list shows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBrowseModel::Refresh()
{
    HRESULT  hr        = S_OK;
    bool     hasFolder = !m_folder.empty();



    CBRA (m_fs != nullptr);
    CBR (hasFolder);

    m_allEntries.clear();

    hr = m_fs->EnumerateEntries (m_folder, m_allEntries);
    CHR (hr);

    std::sort (m_allEntries.begin(), m_allEntries.end(),
               [] (const FileBrowseEntry & a, const FileBrowseEntry & b)
    {
        if (a.isFolder != b.isFolder)
        {
            return a.isFolder;   // folders first
        }

        return ToLower (a.name) < ToLower (b.name);
    });

    RebuildFilteredView();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetExtensionFilter
//
//  Re-filters the cached listing without touching the filesystem (the
//  format dropdown changes this on every selection).
//
////////////////////////////////////////////////////////////////////////////////

void FileBrowseModel::SetExtensionFilter (const std::wstring & ext)
{
    m_extension = ToLower (ext);

    RebuildFilteredView();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFilteredView
//
//  Folders always show; files show only when they carry the active
//  extension (or no filter is set).
//
////////////////////////////////////////////////////////////////////////////////

void FileBrowseModel::RebuildFilteredView()
{
    m_entries.clear();

    for (const FileBrowseEntry & entry : m_allEntries)
    {
        bool  keep = entry.isFolder || m_extension.empty();

        if (!keep)
        {
            std::wstring  lowered = ToLower (entry.name);

            keep = lowered.size() > m_extension.size() &&
                   lowered.compare (lowered.size() - m_extension.size(),
                                    m_extension.size(), m_extension) == 0;
        }

        if (keep)
        {
            m_entries.push_back (entry);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UniqueDefaultName
//
//  "Blank Disk.woz", then "Blank Disk (2).woz", ... -- the first name absent
//  from the current folder. Existence is asked of the
//  filesystem, not the cached listing, so a file created since the last
//  Refresh still collides correctly.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileBrowseModel::UniqueDefaultName (const std::wstring & baseName) const
{
    constexpr int  kMaxSuffix = 999;
    std::wstring   candidate;
    int            suffix     = 1;
    bool           available  = false;



    if (m_fs == nullptr || m_folder.empty())
    {
        return baseName + m_extension;
    }

    for (suffix = 1; suffix <= kMaxSuffix && !available; suffix++)
    {
        candidate = (suffix == 1)
                  ? baseName + m_extension
                  : baseName + L" (" + std::to_wstring (suffix) + L")" + m_extension;

        available = !m_fs->Exists (ComposeTargetPath (candidate));
    }

    return candidate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMountedPaths
//
////////////////////////////////////////////////////////////////////////////////

void FileBrowseModel::SetMountedPaths (std::vector<std::wstring> paths, std::vector<int> drives)
{
    m_mountedPaths  = std::move (paths);
    m_mountedDrives = std::move (drives);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsValidFileName
//
////////////////////////////////////////////////////////////////////////////////

bool FileBrowseModel::IsValidFileName (const std::wstring & fileName)
{
    constexpr const wchar_t *  kIllegalNameChars = L"<>:\"/\\|?*";
    std::wstring               stem              = fileName;
    size_t                     dotPos            = 0;
    bool                       valid             = !fileName.empty();



    valid = valid && fileName.find_first_of (kIllegalNameChars) == std::wstring::npos;
    valid = valid && fileName.back() != L'.' && fileName.back() != L' ';

    if (valid)
    {
        // Reserved device names apply to the stem, case-insensitive.
        dotPos = stem.find (L'.');

        if (dotPos != std::wstring::npos)
        {
            stem = stem.substr (0, dotPos);
        }

        stem = ToLower (stem);

        for (const wchar_t * reserved : s_kReservedNames)
        {
            if (stem == ToLower (reserved))
            {
                valid = false;
                break;
            }
        }
    }

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValidateTarget
//
//  Precedence per the contract: name validity, then the mounted-image
//  refusal, then existence. The order is load-bearing -- a live mount's
//  backing file must never reach the overwrite-confirm path.
//  Folder writability is not probed here; the atomic create itself reports
//  a failing folder with a clear error.
//
////////////////////////////////////////////////////////////////////////////////

TargetVerdict FileBrowseModel::ValidateTarget (const std::wstring & fileName, int & outDrive) const
{
    TargetVerdict  verdict = TargetVerdict::Ok;
    std::wstring   target;
    size_t         i       = 0;



    outDrive = -1;

    if (m_fs == nullptr || !IsValidFileName (fileName))
    {
        verdict = TargetVerdict::InvalidName;
    }
    else
    {
        target = NormalizeForCompare (ComposeTargetPath (fileName));

        for (i = 0; i < m_mountedPaths.size(); i++)
        {
            if (NormalizeForCompare (m_mountedPaths[i]) == target)
            {
                outDrive = (i < m_mountedDrives.size()) ? m_mountedDrives[i] : -1;
                verdict  = TargetVerdict::MountedInDrive;
                break;
            }
        }

        if (verdict == TargetVerdict::Ok && m_fs->Exists (ComposeTargetPath (fileName)))
        {
            verdict = TargetVerdict::Exists;
        }
    }

    return verdict;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComposeTargetPath
//
//  CurrentFolder + fileName, appending the active extension when the name
//  doesn't already carry it (case-insensitive).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FileBrowseModel::ComposeTargetPath (const std::wstring & fileName) const
{
    std::wstring  name    = fileName;
    std::wstring  lowered = ToLower (fileName);
    bool          hasExt  = false;



    if (!m_extension.empty())
    {
        hasExt = lowered.size() > m_extension.size() &&
                 lowered.compare (lowered.size() - m_extension.size(),
                                  m_extension.size(), m_extension) == 0;

        if (!hasExt)
        {
            name += m_extension;
        }
    }

    return m_folder + L"\\" + name;
}

#include "Pch.h"

#pragma comment(lib, "shlwapi.lib")

#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "Machines/Apple2/Common/AppleFileName.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::GetUnitBytes
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CatalogModel::GetUnitBytes (VolumeKind kind)
{
    return (kind == VolumeKind::Dos33)
         ? (uint64_t) NibblizationLayer::kSectorByteSize
         : (uint64_t) ProDosSkeleton::kBlockByteSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::GetTypeText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CatalogModel::GetTypeText (Byte type, VolumeKind kind)
{
    if (kind == VolumeKind::Dos33)
    {
        return std::wstring (1, (wchar_t) HostFileNaming::GetDos33TypeLetter (type));
    }

    for (const ProDosTypeName & row : kProDosTypeNames)
    {
        if (row.type == type)
        {
            return row.mnemonic;
        }
    }

    return std::format (L"${:02X}", (unsigned) type);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::FormatAddress
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CatalogModel::FormatAddress (Word address)
{
    return std::format (L"${:04X}", (unsigned) address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::GetDisplayName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CatalogModel::GetDisplayName (const std::wstring & name, std::vector<std::pair<int, int>> & outControlRanges)
{
    std::wstring  shown;
    int           start = 0;



    outControlRanges.clear();

    for (wchar_t c : name)
    {
        if (c >= 0x20 && c != 0x7F)
        {
            shown += c;
            continue;
        }

        start  = (int) shown.size();
        shown += L'^';
        shown += (c == 0x7F) ? L'?' : (wchar_t) (c + L'@');

        //  A run of control characters is one muted range.
        if (!outControlRanges.empty() && outControlRanges.back().second == start)
        {
            outControlRanges.back().second = (int) shown.size();
        }
        else
        {
            outControlRanges.push_back ({ start, (int) shown.size() });
        }
    }

    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::FromFileEntry
//
//  Size is the recorded length where the filesystem records one, else the
//  units it occupies times the unit size. The address column shows a load
//  address when the entry holds one and the auxiliary type otherwise, and
//  stays blank when neither flag is set rather than showing a zero that
//  would read as a real address.
//
////////////////////////////////////////////////////////////////////////////////

CatalogRow CatalogModel::FromFileEntry (const FileEntry & entry, VolumeKind kind, size_t sourceIndex)
{
    CatalogRow  row;



    row.name         = AppleFileName::ToDisplay (entry.name);
    row.typeText     = GetTypeText (entry.type, kind);
    row.sizeBytes    = entry.hasEofBytes ? entry.eofBytes
                                         : (uint64_t) entry.sizeUnits * GetUnitBytes (kind);
    row.locked       = entry.isLocked;
    row.isDirectory  = entry.isDirectory;
    row.hasModified  = entry.hasModified;
    row.modifiedUnix = entry.modifiedUnix;
    row.sourceIndex  = sourceIndex;

    row.modifiedIsWallClock = true;

    //  A directory's auxiliary field holds nothing a reader would call an
    //  address.
    if (entry.isDirectory)
    {
        return row;
    }

    if (entry.hasLoadAddress)
    {
        row.addressText = FormatAddress (entry.loadAddress);
    }
    else if (entry.hasAuxType)
    {
        row.addressText = FormatAddress (entry.auxType);
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::IsExtensionNeverShown
//
////////////////////////////////////////////////////////////////////////////////

bool CatalogModel::IsExtensionNeverShown (const std::wstring & extension)
{
    static std::mutex                     s_lock;
    static std::map<std::wstring, bool>   s_known;
    std::wstring                          key       = extension;
    wchar_t                               type[256] = {};
    DWORD                                 bytes     = sizeof (type);
    bool                                  hidden    = false;
    LSTATUS                               status    = ERROR_SUCCESS;



    std::transform (key.begin(), key.end(), key.begin(), towlower);

    {
        std::lock_guard<std::mutex>  guard (s_lock);
        auto                         found = s_known.find (key);

        if (found != s_known.end())
        {
            return found->second;
        }
    }

    status = RegGetValueW (HKEY_CLASSES_ROOT, key.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, type, &bytes);

    if (status == ERROR_SUCCESS && type[0] != L'\0')
    {
        hidden = RegGetValueW (HKEY_CLASSES_ROOT, type, L"NeverShowExt", RRF_RT_ANY, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    }

    {
        std::lock_guard<std::mutex>  guard (s_lock);

        s_known[key] = hidden;
    }

    return hidden;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::FromHostEntry
//
//  A host file's type column is its extension, upper-cased, since that is
//  the nearest thing the host has to a type.
//
////////////////////////////////////////////////////////////////////////////////

CatalogRow CatalogModel::FromHostEntry (const FileSystemEntry & entry, bool isDiskImage, size_t sourceIndex)
{
    CatalogRow  row;
    size_t      dot = entry.name.rfind (L'.');



    row.name         = entry.name;
    row.sizeBytes    = entry.sizeBytes;
    row.isDirectory  = entry.isFolder;
    row.isDiskImage  = isDiskImage;
    row.hasModified  = entry.modifiedUnix != 0;
    row.modifiedUnix = entry.modifiedUnix;
    row.sourceIndex  = sourceIndex;
    row.isHidden     = entry.isHidden;
    row.isCompressed = entry.isCompressed;
    row.isEncrypted  = entry.isEncrypted;

    //  A type whose extension Windows always hides, such as a shortcut, is
    //  listed without it, as in File Explorer.
    if (!entry.isFolder && dot != std::wstring::npos && dot > 0 && IsExtensionNeverShown (entry.name.substr (dot)))
    {
        row.label = entry.name.substr (0, dot);
    }

    if (isDiskImage && !GetDiskImageTypeName (entry.name).empty())
    {
        row.typeText = GetDiskImageTypeName (entry.name);
    }
    else if (entry.isFolder)
    {
        row.typeText = L"Folder";
    }
    else if (dot != std::wstring::npos && dot + 1 < entry.name.size())
    {
        row.typeText = entry.name.substr (dot + 1);

        for (wchar_t & c : row.typeText)
        {
            if (c >= L'a' && c <= L'z')
            {
                c = (wchar_t) (c - L'a' + L'A');
            }
        }
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::FromListing
//
////////////////////////////////////////////////////////////////////////////////

void CatalogModel::FromListing (const VolumeListing & listing, VolumeKind kind, std::vector<CatalogRow> & outRows)
{
    size_t  i = 0;



    outRows.clear();
    outRows.reserve (listing.entries.size());

    for (i = 0; i < listing.entries.size(); i++)
    {
        outRows.push_back (FromFileEntry (listing.entries[i], kind, i));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::IsBefore
//
//  Ascending order on one column, with the name breaking ties so the order
//  is total and a re-sort cannot shuffle equal rows.
//
////////////////////////////////////////////////////////////////////////////////

bool CatalogModel::IsBefore (const CatalogRow & a, const CatalogRow & b, Column column)
{
    //  Drives go by their letter, as Explorer orders them, whatever their
    //  volume names. Names go as Explorer's do, numbers by their value: f2
    //  before f10. A shell folder's items go in the folder's own order, as
    //  Explorer's do, which puts its system items ahead of its folders.
    bool                   drives = a.isDrive && b.isDrive;
    bool                   shell  = a.shellOrder > 0 && b.shellOrder > 0;
    const std::wstring  &  nameA  = a.label.empty() ? a.name : a.label;
    const std::wstring  &  nameB  = b.label.empty() ? b.name : b.label;
    int                    byName = drives ? _wcsicmp (a.hostPath.c_str(), b.hostPath.c_str())
                                  : shell  ? a.shellOrder - b.shellOrder
                                           : StrCmpLogicalW (nameA.c_str(), nameB.c_str());



    switch (column)
    {
        case Column::Type:
            if (a.typeText != b.typeText) { return _wcsicmp (a.typeText.c_str(), b.typeText.c_str()) < 0; }
            break;

        case Column::Size:
            if (a.sizeBytes != b.sizeBytes) { return a.sizeBytes < b.sizeBytes; }
            break;

        case Column::Address:
            if (a.addressText != b.addressText) { return a.addressText < b.addressText; }
            break;

        case Column::Locked:
            if (a.locked != b.locked) { return !a.locked; }
            break;

        case Column::Modified:
            if (a.hasModified != b.hasModified) { return !a.hasModified; }
            if (a.modifiedUnix != b.modifiedUnix) { return a.modifiedUnix < b.modifiedUnix; }
            break;

        case Column::OriginalLocation:
            if (_wcsicmp (a.originalFolder.c_str(), b.originalFolder.c_str()) != 0) { return _wcsicmp (a.originalFolder.c_str(), b.originalFolder.c_str()) < 0; }
            break;

        case Column::DateDeleted:
            if (a.hasDeleted != b.hasDeleted) { return !a.hasDeleted; }
            if (a.deletedUnix != b.deletedUnix) { return a.deletedUnix < b.deletedUnix; }
            break;

        case Column::Folder:
            if (_wcsicmp (a.folderPath.c_str(), b.folderPath.c_str()) != 0) { return _wcsicmp (a.folderPath.c_str(), b.folderPath.c_str()) < 0; }
            break;

        case Column::Name:
        default:
            break;
    }

    return byName < 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::Sort
//
//  Folders first whatever the column, then the column's order, reversed as
//  a whole for descending so the folders stay in front.
//
////////////////////////////////////////////////////////////////////////////////

void CatalogModel::Sort (std::vector<CatalogRow> & rows, Column column, bool descending)
{
    std::stable_sort (rows.begin(), rows.end(),
                      [column, descending] (const CatalogRow & a, const CatalogRow & b)
                      {
                          if (a.isDirectory != b.isDirectory)
                          {
                              return a.isDirectory;
                          }

                          return descending ? IsBefore (b, a, column) : IsBefore (a, b, column);
                      });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::GetDiskImageTypeName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CatalogModel::GetDiskImageTypeName (const std::wstring & name)
{
    static constexpr std::pair<const wchar_t *, const wchar_t *>  s_kTypes[] =
    {
        { L".dsk", L"Apple II disk image"       },
        { L".do",  L"DOS-order disk image"      },
        { L".po",  L"ProDOS-order disk image"   },
        { L".d13", L"13-sector disk image"      },
        { L".nib", L"Nibble disk image"         },
        { L".woz", L"WOZ disk image"            },
        { L".2mg", L"2IMG disk image"           },
        { L".hdv", L"ProDOS hard disk image"    },
    };
    size_t        dot       = name.rfind (L'.');
    std::wstring  extension = (dot == std::wstring::npos) ? std::wstring() : name.substr (dot);



    for (const auto & [ext, type] : s_kTypes)
    {
        if (_wcsicmp (extension.c_str(), ext) == 0)
        {
            return type;
        }
    }

    return std::wstring();
}

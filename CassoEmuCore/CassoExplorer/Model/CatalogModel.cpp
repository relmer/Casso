#include "Pch.h"

#pragma comment(lib, "shlwapi.lib")

#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/ApplesoftVariables.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/AppleFileName.h"
#include "Machines/Apple2/Common/CpmVolume.h"
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
    bool  isSectors = kind == VolumeKind::Dos33 || kind == VolumeKind::Gutenberg || kind == VolumeKind::Rdos;



    return isSectors                  ? (uint64_t) NibblizationLayer::kSectorByteSize
         : (kind == VolumeKind::Cpm)  ? (uint64_t) CpmVolume::kBlockBytes
                                      : (uint64_t) ProDosSkeleton::kBlockByteSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::DescribeDamage
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CatalogModel::DescribeDamage (const VolumeListing & listing)
{
    std::wstring  text;



    for (const std::string & note : listing.damage)
    {
        text += (text.empty() ? L"" : L". ") + TextEncoding::NarrowToWide (note);
    }

    if (!text.empty())
    {
        text[0] = towupper (text[0]);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::GetPascalKindText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CatalogModel::GetPascalKindText (Byte kind)
{
    static constexpr const wchar_t *  kNames[] = { L"NONE", L"BAD", L"CODE", L"TEXT", L"INFO", L"DATA", L"GRAF", L"FOTO", L"SDIR" };



    return (kind < std::size (kNames)) ? kNames[kind] : std::format (L"${:02X}", (unsigned) kind);
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

    if (kind == VolumeKind::Pascal)
    {
        return GetPascalKindText (type);
    }

    //  CP/M has no types, only names.
    if (kind == VolumeKind::Cpm)
    {
        return std::wstring();
    }

    //  A Gutenberg or RDOS type is a letter, and a Gutenberg document's is a
    //  space.
    if (kind == VolumeKind::Gutenberg || kind == VolumeKind::Rdos)
    {
        return (type == ' ') ? std::wstring() : std::wstring (1, (wchar_t) type);
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
//  CatalogModel::GetEntryTypeText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CatalogModel::GetEntryTypeText (const FileEntry & entry, VolumeKind kind)
{
    size_t  dot = entry.name.rfind ('.');



    if (kind == VolumeKind::Cpm)
    {
        return (dot == std::string::npos) ? std::wstring() : TextEncoding::NarrowToWide (entry.name.substr (dot + 1));
    }

    return GetTypeText (entry.type, kind);
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
//  address when the entry has one, else the address a ProDOS file loads or
//  runs at. It is blank for any other file rather than showing a number that
//  would read as an address.
//
////////////////////////////////////////////////////////////////////////////////

CatalogRow CatalogModel::FromFileEntry (const FileEntry & entry, VolumeKind kind, size_t sourceIndex)
{
    CatalogRow  row;
    Word        address = 0;



    row.name         = AppleFileName::ToDisplay (entry.name);
    row.typeText     = GetEntryTypeText (entry, kind);
    row.sizeBytes    = entry.hasEofBytes ? entry.eofBytes
                                         : (uint64_t) entry.sizeUnits * GetUnitBytes (kind);
    row.locked       = entry.isLocked;
    row.isDirectory  = entry.isDirectory;
    row.hasModified  = entry.hasModified;
    row.modifiedUnix = entry.modifiedUnix;
    row.sourceIndex  = sourceIndex;

    row.modifiedIsWallClock = true;

    if (entry.hasAccess && !entry.isDirectory)
    {
        row.hasForks          = true;
        row.dataForkBytes     = row.sizeBytes;
        row.resourceForkBytes = entry.resourceForkBytes;
        row.hasResourceFork   = entry.hasResourceFork;
    }

    if (entry.hasAccess)
    {
        row.accessText = FormatAccess (entry.access);
    }

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
    else if (entry.hasAuxType && TryGetProDosAddress (entry, address))
    {
        row.addressText = FormatAddress (address);
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::TryGetProDosAddress
//
//  The address a ProDOS file loads or runs at. BASIC.SYSTEM loads a CMD file
//  at $4000 and ProDOS loads a SYS file at $2000, whatever their auxiliary
//  types are. A BAS file's auxiliary type is where the program loads, and a
//  VAR file's is where its variables were saved from. Any other type's
//  auxiliary type is not an address: a TXT file's is its record length.
//
////////////////////////////////////////////////////////////////////////////////

bool CatalogModel::TryGetProDosAddress (const FileEntry & entry, Word & outAddress)
{
    constexpr Word  s_kCommandAddress = 0x4000;
    constexpr Word  s_kSystemAddress  = 0x2000;
    bool            found             = true;



    switch (entry.type)
    {
    case ProDosVolume::kTypeCommand:
        outAddress = s_kCommandAddress;
        break;
    case ProDosVolume::kTypeSystem:
        outAddress = s_kSystemAddress;
        break;
    case ProDosVolume::kTypeBasic:
    case ApplesoftVariables::kProDosType:
        outAddress = entry.auxType;
        break;
    default:
        found = false;
        break;
    }

    return found;
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
//  is total and a re-sort cannot shuffle equal rows. With tiesReversed, the
//  rows are passed swapped for a descending sort, and its ties go by name
//  from A to Z all the same, as File Explorer's do.
//
////////////////////////////////////////////////////////////////////////////////

bool CatalogModel::IsBefore (const CatalogRow & a, const CatalogRow & b, Column column, bool tiesReversed)
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

        case Column::DataFork:
            if (a.dataForkBytes != b.dataForkBytes) { return a.dataForkBytes < b.dataForkBytes; }
            break;

        case Column::ResourceFork:
            if (a.hasResourceFork != b.hasResourceFork) { return !a.hasResourceFork; }
            if (a.resourceForkBytes != b.resourceForkBytes) { return a.resourceForkBytes < b.resourceForkBytes; }
            break;

        case Column::TotalSize:
            if (a.dataForkBytes + a.resourceForkBytes != b.dataForkBytes + b.resourceForkBytes) { return a.dataForkBytes + a.resourceForkBytes < b.dataForkBytes + b.resourceForkBytes; }
            break;

        case Column::Access:
            if (a.accessText != b.accessText) { return a.accessText < b.accessText; }
            break;

        case Column::FileSystem:
            if (a.collection.fileSystem != b.collection.fileSystem) { return _wcsicmp (a.collection.fileSystem.c_str(), b.collection.fileSystem.c_str()) < 0; }
            break;

        case Column::VolumeName:
            if (a.collection.volumeName != b.collection.volumeName) { return StrCmpLogicalW (a.collection.volumeName.c_str(), b.collection.volumeName.c_str()) < 0; }
            break;

        case Column::FileCount:
            if (a.collection.files != b.collection.files) { return a.collection.files < b.collection.files; }
            break;

        case Column::FreeSpace:
            if (a.collection.freeBytes != b.collection.freeBytes) { return a.collection.freeBytes < b.collection.freeBytes; }
            break;

        case Column::Crc32:
            if (a.collection.crc32 != b.collection.crc32) { return a.collection.crc32 < b.collection.crc32; }
            break;

        case Column::Sha256:
            if (a.collection.sha256 != b.collection.sha256) { return a.collection.sha256 < b.collection.sha256; }
            break;

        case Column::Folder:
        {
            std::wstring  folderA = GetFolderColumnText (a.folderPath);
            std::wstring  folderB = GetFolderColumnText (b.folderPath);

            if (_wcsicmp (folderA.c_str(), folderB.c_str()) != 0) { return _wcsicmp (folderA.c_str(), folderB.c_str()) < 0; }
            break;
        }

        case Column::Name:
        default:
            break;
    }

    return (tiesReversed && column != Column::Name) ? byName > 0 : byName < 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::Sort
//
//  Folders first, then the column's order. Descending, as File Explorer's is,
//  the folders go last, except by type, where the folders' type sorts first
//  both ways. Rows equal on the column go by name from A to Z either way.
//
//  By name, a shell folder's items go in the folder's order alone, folders
//  among them, and descending reverses that order whole: the desktop's order
//  puts the public desktop's shortcuts and its system items that are not
//  folders ahead of the user's folders.
//
////////////////////////////////////////////////////////////////////////////////

void CatalogModel::Sort (std::vector<CatalogRow> & rows, Column column, bool descending)
{
    std::stable_sort (rows.begin(), rows.end(),
                      [column, descending] (const CatalogRow & a, const CatalogRow & b)
                      {
                          bool  shellByName = column == Column::Name && a.shellOrder > 0 && b.shellOrder > 0;

                          //  Folders and files are mixed, as File Explorer mixes them.
                          //  Descending, as File Explorer opens a search, is the most
                          //  relevant first, with equal ranks in the order found.
                          if (column == kRelevance && a.searchRank != b.searchRank)
                          {
                              return descending ? a.searchRank > b.searchRank : a.searchRank < b.searchRank;
                          }

                          if (column == kRelevance)
                          {
                              return descending ? a.sourceIndex < b.sourceIndex : a.sourceIndex > b.sourceIndex;
                          }

                          if (a.isDirectory != b.isDirectory && !shellByName)
                          {
                              return (descending && column != Column::Type) ? b.isDirectory : a.isDirectory;
                          }

                          return descending ? IsBefore (b, a, column, true) : IsBefore (a, b, column);
                      });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::SortByProperty
//
//  The keys are in the rows' order, so the rows are sorted by index and then
//  moved into it.
//
////////////////////////////////////////////////////////////////////////////////

void CatalogModel::SortByProperty (std::vector<CatalogRow> & rows, const std::vector<PropertyKey> & keys, bool descending)
{
    std::vector<size_t>      order;
    std::vector<CatalogRow>  sorted;



    if (keys.size() != rows.size())
    {
        return;
    }

    for (size_t index = 0; index < rows.size(); index++)
    {
        order.push_back (index);
    }

    std::stable_sort (order.begin(), order.end(),
                      [&rows, &keys, descending] (size_t a, size_t b)
                      {
                          if (rows[a].isDirectory != rows[b].isDirectory)
                          {
                              return rows[a].isDirectory;
                          }

                          return descending ? IsPropertyBefore (rows[b], keys[b], rows[a], keys[a]) : IsPropertyBefore (rows[a], keys[a], rows[b], keys[b]);
                      });

    sorted.reserve (rows.size());

    for (size_t index : order)
    {
        sorted.push_back (std::move (rows[index]));
    }

    rows = std::move (sorted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::IsPropertyBefore
//
////////////////////////////////////////////////////////////////////////////////

bool CatalogModel::IsPropertyBefore (const CatalogRow & a, const PropertyKey & keyA, const CatalogRow & b, const PropertyKey & keyB)
{
    int  byText = 0;



    if (keyA.has != keyB.has)
    {
        return !keyA.has;
    }

    if (keyA.numeric && keyB.numeric)
    {
        if (keyA.number != keyB.number)
        {
            return keyA.number < keyB.number;
        }
    }
    else
    {
        byText = StrCmpLogicalW (keyA.text.c_str(), keyB.text.c_str());

        if (byText != 0)
        {
            return byText < 0;
        }
    }

    return IsBefore (a, b, Column::Name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::GetFolderColumnText
//
//  The folder's leaf name and then its parent in parentheses, as Explorer's
//  Folder column has them. A drive's root has no parent.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CatalogModel::GetFolderColumnText (const std::wstring & path)
{
    std::wstring  trimmed = path;
    size_t        slash   = 0;
    std::wstring  parent;



    while (trimmed.size() > 3 && trimmed.back() == L'\\')
    {
        trimmed.pop_back();
    }

    slash = trimmed.find_last_of (L'\\');

    if (trimmed.size() <= 3 || slash == std::wstring::npos)
    {
        return trimmed;
    }

    parent = trimmed.substr (0, slash);

    return trimmed.substr (slash + 1) + L" (" + parent + L")";
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
        { L".dsk",   L"Apple II disk image"       },
        { L".do",    L"DOS-order disk image"      },
        { L".po",    L"ProDOS-order disk image"   },
        { L".d13",   L"13-sector disk image"      },
        { L".nib",   L"Nibble disk image"         },
        { L".nb2",   L"NB2 nibble disk image"     },
        { L".woz",   L"WOZ disk image"            },
        { L".2mg",   L"2IMG disk image"           },
        { L".2img",  L"2IMG disk image"           },
        { L".hdv",   L"ProDOS hard disk image"    },
        { L".dc",    L"Disk Copy 4.2 disk image"  },
        { L".dc42",  L"Disk Copy 4.2 disk image"  },
        { L".image", L"Disk Copy 4.2 disk image"  },
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





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::GetColumnId
//
////////////////////////////////////////////////////////////////////////////////

int CatalogModel::GetColumnId (size_t column)
{
    if (column < kOriginalColumnCount)
    {
        return (int) column;
    }

    if (column < kColumnCount)
    {
        return kFirstAddedColumnId + (int) (column - kOriginalColumnCount);
    }

    return (int) (kOriginalColumnCount + (column - kColumnCount));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::TryGetColumn
//
//  False for a negative id, and for an id in the added range with no column,
//  as a later build's may be.
//
////////////////////////////////////////////////////////////////////////////////

bool CatalogModel::TryGetColumn (int id, size_t & outColumn)
{
    constexpr size_t  kAddedCount = kColumnCount - kOriginalColumnCount;



    outColumn = 0;

    if (id < 0)
    {
        return false;
    }

    if (id >= kFirstAddedColumnId)
    {
        outColumn = kOriginalColumnCount + (size_t) (id - kFirstAddedColumnId);
        return (size_t) (id - kFirstAddedColumnId) < kAddedCount;
    }

    outColumn = ((size_t) id < kOriginalColumnCount) ? (size_t) id : kColumnCount + ((size_t) id - kOriginalColumnCount);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::IsHiddenUntilChosen
//
////////////////////////////////////////////////////////////////////////////////

bool CatalogModel::IsHiddenUntilChosen (size_t column)
{
    return column >= kOriginalColumnCount && column <= (size_t) Column::Access;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::IsCollectionColumn
//
////////////////////////////////////////////////////////////////////////////////

bool CatalogModel::IsCollectionColumn (size_t column)
{
    return column >= (size_t) Column::FileSystem && column < kColumnCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::GetDefaultOrder
//
//  The fork lengths and the total follow Size, and the access flags follow
//  them, ahead of the catalog's Address and Locked. Disk list's columns
//  come last.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<CatalogModel::Column> & CatalogModel::GetDefaultOrder()
{
    static const std::vector<Column>  s_order = { Column::Name, Column::Modified, Column::Type, Column::Size,
                                                  Column::DataFork, Column::ResourceFork, Column::TotalSize, Column::Access,
                                                  Column::Address, Column::Locked, Column::OriginalLocation, Column::DateDeleted, Column::Folder,
                                                  Column::FileSystem, Column::VolumeName, Column::FileCount, Column::FreeSpace, Column::Crc32, Column::Sha256 };



    return s_order;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel::FormatAccess
//
//  ProDOS's six access flags, destroy, rename, backup, invisible, write and
//  read, as their letters, each a dash when clear.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CatalogModel::FormatAccess (Byte access)
{
    static constexpr std::pair<Byte, wchar_t>  kFlags[] = { { 0x80, L'D' }, { 0x40, L'N' }, { 0x20, L'B' }, { 0x04, L'I' }, { 0x02, L'W' }, { 0x01, L'R' } };



    std::wstring  text;



    for (const auto & [mask, letter] : kFlags)
    {
        text += (access & mask) ? letter : L'-';
    }

    return text;
}

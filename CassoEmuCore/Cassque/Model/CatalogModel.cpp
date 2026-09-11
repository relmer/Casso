#include "Pch.h"

#include "Cassque/Model/CatalogModel.h"
#include "Cassque/Model/HostFileNaming.h"
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
//  CatalogModel::FromFileEntry
//
//  Size is the recorded length where the filesystem records one, else the
//  units it occupies times the unit size. The address column shows a load
//  address when the entry carries one and the auxiliary type otherwise, and
//  stays blank when neither flag is set rather than showing a zero that
//  would read as a real address.
//
////////////////////////////////////////////////////////////////////////////////

CatalogRow CatalogModel::FromFileEntry (const FileEntry & entry, VolumeKind kind, size_t sourceIndex)
{
    CatalogRow  row;



    row.name         = std::wstring (entry.name.begin(), entry.name.end());
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

    if (entry.isFolder)
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
    int  byName = _wcsicmp (a.name.c_str(), b.name.c_str());



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

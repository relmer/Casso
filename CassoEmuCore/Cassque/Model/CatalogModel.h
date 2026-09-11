#pragma once

#include "Pch.h"

#include "Config/IFileSystem.h"
#include "Machines/Apple2/Common/VolumeTypes.h"


enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogRow
//
//  One row of the file list, whichever side it came from. Text columns are
//  already formatted so the widget shows them as they are; the numeric
//  originals ride along for sorting.
//
////////////////////////////////////////////////////////////////////////////////

struct CatalogRow
{
    std::wstring  name;
    std::wstring  typeText;
    std::wstring  addressText;
    uint64_t      sizeBytes    = 0;
    int64_t       modifiedUnix = 0;
    bool          hasModified  = false;
    bool          locked       = false;
    bool          isDirectory  = false;
    bool          isDiskImage  = false;

    //  Where the row came from in the listing it was built from, so a sort
    //  can be undone by the consumer and a selection can find its entry.
    size_t        sourceIndex  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogModel
//
//  FileEntry and host directory entries to rows, and the rows to an order.
//
//  A ROW IS TEXT PLUS THE NUMBER IT CAME FROM. The list widget paints text and
//  the sort compares numbers, and building both once here keeps the widget
//  from parsing "$2000" back into an address to order the column.
//
//  Folders sort ahead of files in every order, as the host's own browser
//  does, so a column sort never scatters directories among the files.
//
////////////////////////////////////////////////////////////////////////////////

class CatalogModel
{
public:
    enum class Column { Name, Type, Size, Address, Locked, Modified };

    static CatalogRow  FromFileEntry (const FileEntry & entry, VolumeKind kind, size_t sourceIndex);
    static CatalogRow  FromHostEntry (const FileSystemEntry & entry, bool isDiskImage, size_t sourceIndex);

    static void  FromListing (const VolumeListing & listing, VolumeKind kind, std::vector<CatalogRow> & outRows);

    static void  Sort (std::vector<CatalogRow> & rows, Column column, bool descending);

    //  The DOS 3.3 letter or the ProDOS mnemonic for a type byte.
    static std::wstring  GetTypeText (Byte type, VolumeKind kind);

    static std::wstring  FormatAddress (Word address);

    //  What one allocation unit holds: a sector on DOS 3.3, a block on ProDOS.
    static uint64_t  GetUnitBytes (VolumeKind kind);

private:
    //  The ProDOS file types a catalog names in three letters, as ProDOS's
    //  own CATALOG writes them. Anything else shows its number.
    struct ProDosTypeName
    {
        Byte             type;
        const wchar_t  * mnemonic;
    };

    static constexpr ProDosTypeName  kProDosTypeNames[] =
    {
        { 0x00, L"NON" },
        { 0x01, L"BAD" },
        { 0x04, L"TXT" },
        { 0x06, L"BIN" },
        { 0x0F, L"DIR" },
        { 0x19, L"ADB" },
        { 0x1A, L"AWP" },
        { 0x1B, L"ASP" },
        { 0xEF, L"PAS" },
        { 0xF0, L"CMD" },
        { 0xFA, L"INT" },
        { 0xFB, L"IVR" },
        { 0xFC, L"BAS" },
        { 0xFD, L"VAR" },
        { 0xFE, L"REL" },
        { 0xFF, L"SYS" },
    };

    static bool  IsBefore (const CatalogRow & a, const CatalogRow & b, Column column);
};

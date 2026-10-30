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

    //  A catalog records its date as the wall-clock time where it was written,
    //  converted with no zone; a host file's is a real instant. The first is
    //  shown as recorded, the second in local time.
    bool  modifiedIsWallClock = false;

    //  What the Home folder records of an item; homeGroup is 0 anywhere else.
    int           homeGroup     = 0;
    uint32_t      homeOrder     = 0;
    bool          homePinned    = false;
    bool          isShared      = false;
    int64_t       accessedUnix  = 0;
    int64_t       activityUnix  = 0;
    std::wstring  folderDisplay;
    bool          locked        = false;
    bool          isDirectory   = false;
    bool          isDiskImage   = false;

    //  A host item's attributes: a hidden one's icon is drawn faded, and a
    //  compressed or encrypted one's name in color, as Explorer draws them.
    bool  isHidden            = false;
    bool  isCompressed        = false;
    bool  isEncrypted         = false;

    //  A known folder whose drive is not attached: drawn dimmed, and opens
    //  nothing.
    bool  isMissing           = false;

    //  A file the shell opens as a folder, such as a zip: it opens and lists
    //  as one.
    bool  isArchive           = false;

    //  A drive in This PC, whose tile shows how full it is: sizeBytes holds
    //  its capacity and freeBytes what is left.
    bool      isDrive         = false;
    uint64_t  freeBytes       = 0;

    //  A root's row: the folder or drive it opens, whose icon it shows.
    std::wstring  hostPath;

    //  The name in a folder's desktop.ini, which Explorer lists it by, as
    //  iCloud Drive for the folder iCloudDrive; empty for every other row,
    //  which is listed by its own name.
    std::wstring  label;

    //  A shell folder's item: 1 and up, its place in the folder's own Name
    //  order, which Explorer's Name column follows; 0 for any other row.
    int  shellOrder = 0;

    //  A Recycle Bin row: the shell's id for the item, the folder it was
    //  deleted from, and when.
    std::wstring  recycledId;
    std::wstring  originalFolder;

    //  A search result's folder, and for a file inside a disk image, the
    //  image it is in.
    std::wstring  folderPath;
    std::wstring  imagePath;
    size_t        imageCatalogIndex = 0;
    std::string   imageDirectory;
    int64_t       deletedUnix       = 0;
    bool          hasDeleted        = false;

    //  A shell folder's row: the shell's own name for the item, which opens it
    //  whether or not it is a file; hostPath holds its path when it has one.
    std::wstring  shellId;

    //  A desktop entry such as OneDrive, whose icon is the entry's own rather
    //  than its folder's: the shell's name for the entry.
    std::wstring  iconId;

    //  Where the row came from in the listing it was built from, so a sort
    //  can be undone by the consumer and a selection can find its entry.
    size_t        sourceIndex  = 0;

    //  An item CassoLink's root lists that is not on the disk until it is
    //  opened or dragged: its boot disk, or its help, with hostPath where it
    //  is written; or the row that adds a machine, which is no file at all.
    enum class CassoLinkItem { None, BootDisk, Help, AdtProHelp, AddMachine };

    CassoLinkItem  cassoLinkItem = CassoLinkItem::None;

    //  A linked machine that is not connected, whose apple is drawn in grays.
    bool           offline       = false;

    bool  operator== (const CatalogRow &) const = default;
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
    //  In File Explorer's order: Name, Date modified, Type, Size, then the
    //  two only a disk catalog has.
    //  The last two are the Recycle Bin's, shown only there.
    enum class Column { Name, Modified, Type, Size, Address, Locked, OriginalLocation, DateDeleted, Folder };

    static CatalogRow  FromFileEntry (const FileEntry & entry, VolumeKind kind, size_t sourceIndex);
    static CatalogRow  FromHostEntry (const FileSystemEntry & entry, bool isDiskImage, size_t sourceIndex);

    //  Whether Windows hides an extension whatever the user's setting: its
    //  file type has NeverShowExt, as a shortcut's does. Read once a type.
    static bool        IsExtensionNeverShown (const std::wstring & extension);

    //  The Type column's text for a disk image by its extension, as
    //  ProDOS-order disk image, or empty for a name that is not one.
    static std::wstring  GetDiskImageTypeName (const std::wstring & name);

    static void  FromListing (const VolumeListing & listing, VolumeKind kind, std::vector<CatalogRow> & outRows);

    static void  Sort (std::vector<CatalogRow> & rows, Column column, bool descending);

    //  The count of fixed columns; a column after them is a shell property's.
    static constexpr size_t  kColumnCount = (size_t) Column::Folder + 1;

    //  A row's value of a shell property, as its column sorts by it: by number
    //  when both have one, else by text. A row without one sorts first.
    struct PropertyKey
    {
        bool          has     = false;
        bool          numeric = false;
        double        number  = 0.0;
        std::wstring  text;
    };

    //  Sorts by a property, with a key for each row in the rows' order.
    //  Folders first, and the name breaking ties, as Sort.
    static void  SortByProperty (std::vector<CatalogRow> & rows, const std::vector<PropertyKey> & keys, bool descending);

    //  The DOS 3.3 letter or the ProDOS mnemonic for a type byte.
    static std::wstring  GetTypeText (Byte type, VolumeKind kind);

    static std::wstring  FormatAddress (Word address);

    //  A name as displayed in the list. DOS 3.3 names can contain control
    //  characters (a heading entry uses backspaces to overwrite CATALOG's type
    //  and size columns), and each is displayed in caret form: ^H for a
    //  backspace, ^? for DEL. The ranges of those pairs are returned so the
    //  list draws them muted, which distinguishes them from a typed caret and
    //  letter.
    static std::wstring  GetDisplayName (const std::wstring & name, std::vector<std::pair<int, int>> & outControlRanges);

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

    static bool  IsBefore         (const CatalogRow & a, const CatalogRow & b, Column column);
    static bool  IsPropertyBefore (const CatalogRow & a, const PropertyKey & keyA, const CatalogRow & b, const PropertyKey & keyB);
};

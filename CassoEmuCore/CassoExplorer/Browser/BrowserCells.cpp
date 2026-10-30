#include "Pch.h"

#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::BrowserCells
//
////////////////////////////////////////////////////////////////////////////////

BrowserCells::BrowserCells (CassoExplorerBrowser & browser)
    : m_browser (browser)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::ToTreeNode
//
//  Collapsed, with children left to the provider, so nothing below a root is
//  read until someone expands it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTreeNode BrowserCells::ToTreeNode (const TreeNode & node, IShellIcons * icons)
{
    DxuiTreeNode  out;



    out.id             = node.id;
    out.label          = node.label;
    out.dividerAbove   = node.dividerAbove;

    //  A drive under its name, as Explorer's tree shows it.
    if (node.kind == TreeNode::Kind::Drive && icons != nullptr)
    {
        IShellIcons::DriveInfo  drive;

        if (icons->GetDriveInfo (node.location.path, drive) && !drive.name.empty())
        {
            out.label = drive.name;
        }
    }

    out.expanded       = false;
    out.childrenLoaded = !node.canExpand;
    //  A known folder that is gone is dimmed. An image that will not open is
    //  not: the preview says why when it is chosen.
    out.dimmed         = node.missing;
    out.iconGhosted    = node.hidden;
    out.iconBroken     = node.broken;
    out.iconWarning    = node.warning;
    out.pinned         = node.pinned;

    if (icons != nullptr)
    {
        out.icon = GetNodeIcon (node, *icons);
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::UpdateStatus
//
////////////////////////////////////////////////////////////////////////////////

void BrowserCells::UpdateStatus()
{
    m_browser.m_status = BrowserStatus();
    m_browser.m_status.selection = FormatSelection (0, m_browser.m_rows.size());

    if (!m_browser.m_selectedRows.empty())
    {
        uint64_t  bytes   = 0;
        bool      anyFile = false;

        for (int selected : m_browser.m_selectedRows)
        {
            if (!m_browser.m_rows[(size_t) selected].isDirectory)
            {
                bytes  += m_browser.m_rows[(size_t) selected].sizeBytes;
                anyFile = true;
            }
        }

        m_browser.m_status.selected = FormatSelected (m_browser.m_selectedRows.size(), bytes, anyFile);
    }

    if (m_browser.m_selectedRows.size() == 1)
    {
        const CatalogRow &  row = m_browser.m_rows[m_browser.m_selectedRows[0]];

        m_browser.m_status.detail = row.typeText;

        if (!row.isDirectory)
        {
            m_browser.m_status.detail += L", " + FormatSize (row.sizeBytes);
        }

        if (!row.addressText.empty())
        {
            m_browser.m_status.detail += L", " + row.addressText;
        }
    }

    //  As Explorer words a drive's: what is free of the whole. For the image
    //  or linked drive open, or for one image selected in a folder.
    if (m_browser.m_isImage || (m_browser.m_isAppleDrive && m_browser.m_listing.totalUnits > 0))
    {
        DescribeVolumeSpace (m_browser.GetLocation().path, m_browser.m_listing, m_browser.m_kind, m_browser.m_status);
    }
    else if (m_browser.m_selectedRows.size() == 1)
    {
        Location       image;
        VolumeListing  listing;
        VolumeKind     kind = VolumeKind::Unknown;

        if (m_browser.TryGetRowLocation (m_browser.m_selectedRows[0], image) && image.kind == Location::Kind::DiskImage &&
            m_browser.m_operations.List (TextEncoding::WideToNarrow (image.path), std::string(), listing, kind).Succeeded())
        {
            DescribeVolumeSpace (image.path, listing, kind, m_browser.m_status);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::DescribeVolumeSpace
//
////////////////////////////////////////////////////////////////////////////////

void BrowserCells::DescribeVolumeSpace (const std::wstring & imagePath, const VolumeListing & listing, VolumeKind kind, BrowserStatus & outStatus)
{
    uint64_t  unit = CatalogModel::GetUnitBytes (kind);



    outStatus.freeSpace    = FormatSize ((uint64_t) listing.freeUnits * unit) + L" free of " + FormatSize ((uint64_t) listing.totalUnits * unit);
    outStatus.freeSpaceTip = std::filesystem::path (imagePath).filename().wstring();

    if (kind == VolumeKind::Dos33)
    {
        outStatus.freeSpaceTip += std::format (L", DOS 3.3 volume {}", listing.volumeNumber);
    }
    else if (kind == VolumeKind::ProDos && !listing.volumeName.empty())
    {
        outStatus.freeSpaceTip += L", ProDOS volume /" + TextEncoding::NarrowToWide (listing.volumeName);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::GetColumns
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Column> BrowserCells::GetColumns()
{
    std::vector<DxuiListView::Column>  columns;



    //  File Explorer's order and its default widths, measured at 100%: each a
    //  fixed width, none stretching to fill the pane.
    columns.push_back (DxuiListView::Column { L"Name",          kNameColumnDip,     false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Date modified", kModifiedColumnDip, false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Type",          kTypeColumnDip,     false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Size",          kSizeColumnDip,     false, DxuiTextHAlign::Right });
    columns.push_back (DxuiListView::Column { L"Address",       kAddressColumnDip,  false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Locked",        kLockedColumnDip,   false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Original location", kOriginalColumnDip, false, DxuiTextHAlign::Left });
    columns.push_back (DxuiListView::Column { L"Date deleted",  kDeletedColumnDip,  false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Folder",        kOriginalColumnDip, false, DxuiTextHAlign::Left  });

    return columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::GetCatalogPreviewColumns
//
//  The preview pane is narrower than the list, so a catalog there keeps only
//  what identifies a file, each column as wide as its header or its widest
//  entry.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Column> BrowserCells::GetCatalogPreviewColumns()
{
    std::vector<DxuiListView::Column>  columns;



    columns.push_back (DxuiListView::Column { L"Name", 0, false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Type", 0, false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Size", 0, false, DxuiTextHAlign::Right });

    return columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::ToCatalogPreviewCells
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> BrowserCells::ToCatalogPreviewCells (const CatalogRow & row)
{
    std::vector<DxuiListView::Cell>  cells;
    DxuiListView::Cell               name;



    name.text = CatalogModel::GetDisplayName (row.label.empty() ? row.name : row.label, name.dimRanges);

    cells.push_back (name);
    cells.push_back (DxuiListView::Cell { row.typeText, false });
    //  A folder has no size of its own, except in the Recycle Bin, which adds up
    //  what each deleted folder held, as Explorer shows; nor does a shell item
    //  that is not a file, such as a computer on the network.
    cells.push_back (DxuiListView::Cell { (row.isDirectory && row.recycledId.empty()) || (!row.shellId.empty() && row.hostPath.empty() && row.imagePath.empty()) ? std::wstring() : FormatSizeColumn (row.sizeBytes), false });

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::ToCells
//
//  A folder has no size to show, and an entry that records no date leaves the
//  column blank rather than showing 1970.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> BrowserCells::ToCells (const CatalogRow & row, const Location & at, IShellIcons * icons)
{
    std::vector<DxuiListView::Cell>  cells;
    DxuiListView::Cell               name;
    bool                             hostItem = at.kind == Location::Kind::HostFolder || !row.hostPath.empty();



    name.text = CatalogModel::GetDisplayName (row.label.empty() ? row.name : row.label, name.dimRanges);

    cells.push_back (name);
    cells.push_back (DxuiListView::Cell { row.hasModified ? FormatModified (row.modifiedUnix, row.modifiedIsWallClock) : std::wstring(), false });
    cells.push_back (DxuiListView::Cell { row.typeText, false });
    //  A folder has no size of its own, except in the Recycle Bin, which adds up
    //  what each deleted folder held, as Explorer shows; nor does a shell item
    //  that is not a file, such as a computer on the network.
    cells.push_back (DxuiListView::Cell { (row.isDirectory && row.recycledId.empty()) || (!row.shellId.empty() && row.hostPath.empty() && row.imagePath.empty() && row.sizeBytes == 0) ? std::wstring() : FormatSizeColumn (row.sizeBytes), false });
    cells.push_back (DxuiListView::Cell { row.addressText, false });
    cells.push_back (DxuiListView::Cell { row.locked ? L"Yes" : L"", false });
    cells.push_back (DxuiListView::Cell { row.originalFolder, false });
    cells.push_back (DxuiListView::Cell { row.hasDeleted ? FormatModified (row.deletedUnix, false) : std::wstring(), false });
    cells.push_back (DxuiListView::Cell { row.folderPath, false });

    if (icons != nullptr && !cells.empty())
    {
        cells[0].icon = GetRowIcon (row, at, *icons);
    }

    cells[0].iconGhosted  = row.isHidden || row.isMissing;

    if (row.isMissing)
    {
        for (DxuiListView::Cell & cell : cells)
        {
            cell.dim = true;
        }
    }

    //  Explorer's Content row, until the shell's own lines for a host item
    //  replace it: the date and the size in the right column, each with its
    //  name. A file in a disk image, which has no shell properties, and a
    //  drive have their type under their name.
    if ((!row.isDirectory || row.isDrive) && (row.isDrive || !hostItem))
    {
        cells[0].contentLeft = { MakeContentLine (L"Type:", row.typeText) };
    }

    //  A search result says where it is, ahead of its type; a folder, which
    //  has no type, by its place alone.
    cells[0].contentPath = row.folderPath;

    if (!row.folderPath.empty() && cells[0].contentLeft.empty())
    {
        cells[0].contentLeft = { DxuiListView::Cell { std::wstring(), false } };
    }

    cells[0].contentRight.push_back (row.hasModified ? MakeContentLine (L"Date modified:", FormatModified (row.modifiedUnix, row.modifiedIsWallClock))
                                                     : DxuiListView::Cell());

    if (!row.isDirectory)
    {
        cells[0].contentRight.push_back (MakeContentLine (L"Size:", FormatSize (row.sizeBytes)));
    }

    cells[0].tileNameOnly = row.isDirectory && !row.isDrive;

    //  Explorer's drive tile: a bar as full as the drive, then what is free.
    if (row.isDrive && row.sizeBytes > 0)
    {
        DxuiListView::Cell  meter;
        DxuiListView::Cell  free;

        meter.meter = 1.0f - (float) ((double) row.freeBytes / (double) row.sizeBytes);
        free.text   = std::format (L"{} free of {}", FormatSize (row.freeBytes), FormatSize (row.sizeBytes));

        cells[0].tileLines = { meter, free };
    }
    else if (!row.isDirectory)
    {
        //  Explorer's file tile: its type, then its size.
        cells[0].tileLines = { DxuiListView::Cell { row.typeText, false }, DxuiListView::Cell { FormatSize (row.sizeBytes), false } };
    }

    //  Home's items: the location under the name, then the tab's columns,
    //  which Shared has more of.
    if (row.homeGroup > 0)
    {
        static constexpr int  s_kHomeGroupShared = 4;

        cells.resize (1);
        cells[0].tileLines  = { DxuiListView::Cell { row.folderDisplay, false } };
        cells[0].homePinned = row.homePinned;
        cells[0].homeCloud  = row.hostPath.empty();

        if (row.homeGroup == s_kHomeGroupShared)
        {
            cells.push_back (DxuiListView::Cell { row.sharedUnix > 0 ? FormatModified (row.sharedUnix, false) : std::wstring(), false });
            cells.push_back (DxuiListView::Cell { row.sharedBy, false });
        }
        else
        {
            cells.push_back (DxuiListView::Cell { row.accessedUnix > 0 ? FormatModified (row.accessedUnix, false) : std::wstring(), false });
        }

        cells.push_back (DxuiListView::Cell { row.activity, false });
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::MakeContentLine
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Cell BrowserCells::MakeContentLine (const std::wstring & label, const std::wstring & value)
{
    DxuiListView::Cell  line;



    line.label = label;
    line.text  = value;

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::GetNameArgb
//
//  Explorer's colors for a compressed or an encrypted item's name, when its
//  options ask for them. The dark theme's compressed blue is measured from
//  Explorer; its encrypted green, which EFS would be needed to measure, is a
//  green of the same lightness. The light theme's are Explorer's classic
//  pair.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BrowserCells::GetNameArgb (const CatalogRow & row, const FolderOptions & options, bool dark)
{
    if (!options.colorCompressed)
    {
        return 0;
    }

    if (row.isEncrypted)
    {
        return dark ? kEncryptedDarkArgb : kEncryptedLightArgb;
    }

    if (row.isCompressed)
    {
        return dark ? CassoExplorerBrowser::kCompressedDarkArgb : CassoExplorerBrowser::kCompressedLightArgb;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::ShortenPaths
//
//  Each full path in a message cut to its last part, the file or folder's own
//  name: the address bar and the tree already show where it is, and a whole
//  path makes a message too long to read. A path runs from its drive to the
//  colon, quote or line end that follows; a sentence's period after it is not
//  part of it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserCells::ShortenPaths (const std::wstring & message)
{
    std::wstring  out;
    size_t        i = 0;



    while (i < message.size())
    {
        bool  isPath = i + 2 < message.size() && iswalpha (message[i]) && message[i + 1] == L':' && message[i + 2] == L'\\'
                       && (i == 0 || !iswalnum (message[i - 1]));

        if (!isPath)
        {
            out += message[i++];
            continue;
        }

        size_t        end  = message.find_first_of (L":\r\n\"'", i + 2);
        std::wstring  path  = message.substr (i, (end == std::wstring::npos) ? std::wstring::npos : end - i);
        std::wstring  tail;
        size_t        slash = 0;

        while (!path.empty() && (path.back() == L' ' || path.back() == L'.' || path.back() == L','))
        {
            tail.insert (tail.begin(), path.back());
            path.pop_back();
        }

        //  A file's extension ends in a letter, so a period only moved to the
        //  tail when it closed the sentence -- unless the name itself ended
        //  there, which the tail cannot tell apart and leaves as it was.
        while (!path.empty() && path.back() == L'\\')
        {
            path.pop_back();
        }

        slash = path.find_last_of (L'\\');
        out  += ((slash == std::wstring::npos) ? path : path.substr (slash + 1)) + tail;
        i     = (end == std::wstring::npos) ? message.size() : end;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::FormatLinkError
//
//  A disk whose sectors read but whose data is no DOS 3.3 or ProDOS volume
//  gets the same sentence as an image with no file system; any other failure
//  with no message is a drive that could not be read.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserCells::FormatLinkError (HRESULT result, const std::string & message)
{
    if (!message.empty())
    {
        return TextEncoding::NarrowToWide (message);
    }

    if (result == HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME))
    {
        return std::wstring (L"The disk in this drive ") + TreeModel::kNoFilesystemText;
    }

    return L"This drive could not be read.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::FormatImageError
//
//  The command line's refusal, "path: reason", as a sentence about the file
//  by its own name, since its path is in the address bar and the tree. An
//  image with no file system gets one plain sentence, and the rest is in the
//  preview's details.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserCells::FormatImageError (const std::wstring & imagePath, const std::string & message)
{
    std::wstring  text   = TextEncoding::NarrowToWide (message);
    std::wstring  prefix = imagePath + L": ";
    size_t        slash  = imagePath.find_last_of (L"\\/");
    std::wstring  leaf   = imagePath.substr ((slash == std::wstring::npos) ? 0 : slash + 1);



    if (message.find (DiskImageSession::kNoFilesystemText) != std::string::npos)
    {
        return leaf + L" " + TreeModel::kNoFilesystemText;
    }

    while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r' || text.back() == L' '))
    {
        text.pop_back();
    }

    if (text.compare (0, prefix.size(), prefix) == 0)
    {
        text = leaf + L" " + text.substr (prefix.size());
    }

    //  The general part leads and the detail follows on a line of its own.
    text = TreeModel::BreakAfterFirstSentence (text);

    if (!text.empty() && text.back() != L'.')
    {
        text += L'.';
    }

    return ShortenPaths (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::FormatSize
//
//  A size as Explorer's status bar and its free space show one: Windows' own
//  byte-size format, three significant digits in the largest unit that fits.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserCells::FormatSize (uint64_t bytes)
{
    wchar_t  text[64] = {};



    if (StrFormatByteSizeW ((LONGLONG) bytes, text, (UINT) std::size (text)) == nullptr)
    {
        return std::format (L"{} bytes", bytes);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::FormatSizeColumn
//
//  A size as Explorer's Size column shows one: whole kilobytes rounded up,
//  with the user's thousands separator, so a 187-byte file is "1 KB".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserCells::FormatSizeColumn (uint64_t bytes)
{
    wchar_t  text[64] = {};



    if (StrFormatKBSizeW ((LONGLONG) bytes, text, (UINT) std::size (text)) == nullptr)
    {
        return std::format (L"{} KB", (bytes + 1023) / 1024);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::FormatModified
//
//  A host file's instant in local time, as Explorer shows it; a catalog's
//  wall-clock time as it was recorded. Either way in the user's own short
//  date and short time, as Explorer's Date modified column writes them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserCells::FormatModified (int64_t unixSeconds, bool wallClock)
{
    __time64_t  seconds  = (__time64_t) unixSeconds;
    tm          local    = {};
    errno_t     err      = wallClock ? _gmtime64_s (&local, &seconds) : _localtime64_s (&local, &seconds);
    SYSTEMTIME  st       = {};
    wchar_t     date[80] = {};
    wchar_t     time[80] = {};
    int         dateLen  = 0;
    int         timeLen  = 0;



    if (err != 0)
    {
        return std::wstring();
    }

    st.wYear   = (WORD) (local.tm_year + 1900);
    st.wMonth  = (WORD) (local.tm_mon + 1);
    st.wDay    = (WORD) local.tm_mday;
    st.wHour   = (WORD) local.tm_hour;
    st.wMinute = (WORD) local.tm_min;
    st.wSecond = (WORD) local.tm_sec;

    dateLen = GetDateFormatEx (LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, date, (int) std::size (date), nullptr);
    timeLen = GetTimeFormatEx (LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &st, nullptr, time, (int) std::size (time));

    if (dateLen == 0 || timeLen == 0)
    {
        return std::format (L"{:04}-{:02}-{:02} {:02}:{:02}", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    }

    return std::wstring (date) + L" " + time;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::FormatSelected
//
//  Two spaces before the size, as Explorer's field has them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserCells::FormatSelected (size_t selected, uint64_t bytes, bool anyFile)
{
    std::wstring  text = std::format (L"{} {} selected", selected, selected == 1 ? L"item" : L"items");



    if (anyFile)
    {
        text += L"  " + FormatSize (bytes);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::FormatSelection
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserCells::FormatSelection (size_t selected, size_t total)
{
    std::wstring  items = std::format (L"{} {}", total, total == 1 ? L"item" : L"items");



    if (selected == 0)
    {
        return items;
    }

    return std::format (L"{}, {} selected", items, selected);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::GetLocationLabel
//
//  A folder's own name, a drive's root as written, an image's file name, and
//  the innermost directory of a path inside an image.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserCells::GetLocationLabel (const Location & location)
{
    std::wstring  path  = location.path;
    size_t        slash = 0;
    std::string   inner = location.innerPath;



    switch (location.kind)
    {
        case Location::Kind::None:
            return L"Home";

        case Location::Kind::Root:
            return TreeModel::GetRootLabel (location.path);

        case Location::Kind::RecycleBin:
            return TreeModel::GetRootLabel (Location::kRecycleBinId);

        case Location::Kind::ShellFolder:
            return location.label.empty() ? location.path : location.label;

        case Location::Kind::DiskDirectory:
            slash = inner.find_last_of ("/:");
            return TextEncoding::NarrowToWide ((slash == std::string::npos) ? inner : inner.substr (slash + 1));

        case Location::Kind::AppleDrive:
        {
            std::wstring  link;
            int           slot  = 0;
            int           drive = 0;

            TreeModel::TryParseLinkId (location.path, link, slot, drive);
            slash = inner.find_last_of ('/');

            return inner.empty() ? TreeModel::GetLinkDriveLabel (slot, drive) : TextEncoding::NarrowToWide ((slash == std::string::npos) ? inner : inner.substr (slash + 1));
        }

        default:
            break;
    }

    while (path.size() > 3 && path.back() == L'\\')
    {
        path.pop_back();
    }

    if (path.size() <= 3)
    {
        return path;
    }

    slash = path.rfind (L'\\');

    return (slash == std::wstring::npos) ? path : path.substr (slash + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::GetTabLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserCells::GetTabLabel (size_t index) const
{
    if (index >= m_browser.m_model.GetTabCount())
    {
        return std::wstring();
    }

    return GetLocationLabel (m_browser.m_model.GetTab (index).location);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::GetRowIcon
//
//  A host folder's rows are real files and folders, so each gets the shell's
//  icon for its path, except a disk image, which has Casso's floppy. A row
//  inside an image has no host path, so it gets the generic folder or file
//  icon.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> BrowserCells::GetRowIcon (const CatalogRow & row, const Location & at, IShellIcons & icons)
{
    std::wstring  link;
    int           slot  = 0;
    int           drive = 0;



    //  A disk image has Casso's floppy wherever it is listed, one inside a zip
    //  too, as does a linked Apple's drive.
    if (row.isDiskImage || (TreeModel::TryParseLinkId (row.hostPath, link, slot, drive) && drive != 0)
        || (at.kind == Location::Kind::ShellFolder && !row.isDirectory && !CatalogModel::GetDiskImageTypeName (row.name).empty()))
    {
        return icons.GetForKind (IShellIcons::Kind::DiskImage);
    }

    //  A linked Apple has the apple, as does the row that adds one, and
    //  CassoLink's help the icon of its type, before it is written.
    if (!link.empty() || row.cassoLinkItem == CatalogRow::CassoLinkItem::AddMachine)
    {
        return icons.GetForKind (row.offline ? IShellIcons::Kind::AppleMachineOffline : IShellIcons::Kind::AppleMachine);
    }

    if (row.cassoLinkItem == CatalogRow::CassoLinkItem::Help || row.cassoLinkItem == CatalogRow::CassoLinkItem::AdtProHelp)
    {
        return icons.GetForPath (row.hostPath, false);
    }

    //  A BASIC listing kept on the host, named .bas or .bas.txt, has the
    //  BASIC icon its program has inside an image.
    if (!row.isDirectory && IsBasicListingName (row.name) && (at.kind == Location::Kind::HostFolder || !row.hostPath.empty()))
    {
        return icons.GetForKind (IShellIcons::Kind::AppleApplesoft);
    }

    if (at.kind == Location::Kind::HostFolder)
    {
        return icons.GetForPath (CassoExplorerBrowser::JoinPath (at.path, row.name), row.isDirectory);
    }

    //  A root's rows are its known folders and drives, with their own icons.
    if (at.kind == Location::Kind::Root && !row.hostPath.empty())
    {
        return icons.GetForPath (row.hostPath, true);
    }

    //  A search's match inside a disk image has its Apple type's icon.
    if (!row.imagePath.empty())
    {
        return icons.GetForKind (GetAppleTypeIconKind (row.typeText));
    }

    //  A shell folder's item has the icon the shell draws for it: a desktop
    //  entry's own, else by its path when it has one.
    if ((at.kind == Location::Kind::ShellFolder || row.homeGroup > 0) && !row.shellId.empty())
    {
        return icons.GetForPath (!row.iconId.empty() ? row.iconId : row.hostPath.empty() ? row.shellId : row.hostPath, row.isDirectory);
    }

    //  A deleted item still has its file, under the bin's own folder on its
    //  drive, so it has the icon it had.
    if (at.kind == Location::Kind::RecycleBin && row.recycledId.size() > 2 && row.recycledId[1] == L':')
    {
        return icons.GetForPath (row.recycledId, row.isDirectory);
    }

    return icons.GetForKind (row.isDirectory ? IShellIcons::Kind::Folder : GetAppleTypeIconKind (row.typeText));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::GetLocationIcon
//
//  The icon the address bar leads with, as Explorer's shows the folder's own:
//  a host folder's or a shell folder's from the shell, a disk image's by its
//  extension, and a directory inside an image the generic folder.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> BrowserCells::GetLocationIcon (const Location & at, IShellIcons & icons)
{
    //  A search's path is its query, not a place; File Explorer's address bar
    //  leads with This PC for one.
    if (SearchQuery::IsId (at.path))
    {
        return icons.GetForKind (IShellIcons::Kind::ThisPc);
    }

    switch (at.kind)
    {
        case Location::Kind::HostFolder:    return icons.GetForPath (at.path, true);
        case Location::Kind::ShellFolder:   return icons.GetForPath (at.path, true);
        case Location::Kind::DiskImage:     return icons.GetForKind (IShellIcons::Kind::DiskImage);
        case Location::Kind::DiskDirectory: return icons.GetForKind (IShellIcons::Kind::Folder);
        case Location::Kind::RecycleBin:    return icons.GetForKind (IShellIcons::Kind::RecycleBin);
        case Location::Kind::Root:
            return icons.GetForKind ((at.path == TreeModel::kCassoRootId)     ? IShellIcons::Kind::Casso
                                   : (at.path == TreeModel::kCassoLinkRootId) ? IShellIcons::Kind::AppleMachine
                                                                              : IShellIcons::Kind::ThisPc);
        default:                            return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::IsBasicListingName
//
//  A host file named as a BASIC listing: .bas, or .bas.txt for one an
//  editor should open as text. Case is ignored, as Windows ignores it.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserCells::IsBasicListingName (const std::wstring & name)
{
    static constexpr std::wstring_view  s_kSuffixes[] = { L".bas", L".bas.txt" };
    bool                                matches        = false;



    for (std::wstring_view suffix : s_kSuffixes)
    {
        matches = matches || (name.size() > suffix.size() && _wcsicmp (name.c_str() + name.size() - suffix.size(), suffix.data()) == 0);
    }

    return matches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::GetAppleTypeIconKind
//
//  By the type a catalog shows: DOS 3.3's letter or ProDOS's mnemonic. Any
//  other type has the generic file icon.
//
////////////////////////////////////////////////////////////////////////////////

IShellIcons::Kind BrowserCells::GetAppleTypeIconKind (const std::wstring & typeText)
{
    static constexpr std::pair<const wchar_t *, IShellIcons::Kind>  kTypes[] =
    {
        { L"T",   IShellIcons::Kind::AppleText        },
        { L"TXT", IShellIcons::Kind::AppleText        },
        { L"A",   IShellIcons::Kind::AppleApplesoft   },
        { L"BAS", IShellIcons::Kind::AppleApplesoft   },
        { L"I",   IShellIcons::Kind::AppleInteger     },
        { L"INT", IShellIcons::Kind::AppleInteger     },
        { L"B",   IShellIcons::Kind::AppleBinary      },
        { L"BIN", IShellIcons::Kind::AppleBinary      },
        { L"SYS", IShellIcons::Kind::AppleSystem      },
        { L"S",   IShellIcons::Kind::AppleSystem      },
        { L"R",   IShellIcons::Kind::AppleRelocatable },
        { L"REL", IShellIcons::Kind::AppleRelocatable },
    };



    for (const auto & type : kTypes)
    {
        if (typeText == type.first)
        {
            return type.second;
        }
    }

    return IShellIcons::Kind::File;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserCells::GetNodeIcon
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> BrowserCells::GetNodeIcon (const TreeNode & node, IShellIcons & icons)
{
    //  A root that is an entry of the desktop's own, as OneDrive, has that
    //  entry's icon, its cloud, rather than its folder's. A tree icon has no
    //  overlays, as in Explorer's navigation pane.
    if (!node.iconId.empty())
    {
        return icons.GetForTreePath (node.iconId, true);
    }

    //  An archive has its file's icon, a zip's zipper, not a folder's.
    if (node.archive)
    {
        return icons.GetForPath (node.location.path, false);
    }

    switch (node.kind)
    {
        case TreeNode::Kind::CassoRoot:     return icons.GetForKind (IShellIcons::Kind::Casso);
        case TreeNode::Kind::ThisPcRoot:    return icons.GetForKind (IShellIcons::Kind::ThisPc);
        case TreeNode::Kind::RecycleBinRoot: return icons.GetForKind (IShellIcons::Kind::RecycleBin);
        case TreeNode::Kind::DiskDirectory: return icons.GetForKind (IShellIcons::Kind::Folder);
        case TreeNode::Kind::CassoLinkConnection:     return icons.GetForKind (node.offline ? IShellIcons::Kind::AppleMachineOffline : IShellIcons::Kind::AppleMachine);
        case TreeNode::Kind::CassoLinkRoot: return icons.GetForKind (IShellIcons::Kind::AppleMachine);
        case TreeNode::Kind::AddLink:       return icons.GetForKind (IShellIcons::Kind::AppleMachine);
        case TreeNode::Kind::AppleDrive:    return icons.GetForKind (IShellIcons::Kind::DiskImage);
        //  Every remaining node is a drive or a host folder; a disk image node
        //  has Casso's floppy.
        case TreeNode::Kind::DiskImage:     return icons.GetForKind (IShellIcons::Kind::DiskImage);
        default:                            return icons.GetForTreePath (node.location.path, true);
    }
}

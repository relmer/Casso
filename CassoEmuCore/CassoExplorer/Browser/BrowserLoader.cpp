#include "Pch.h"

#include "CassoExplorer/Browser/BrowserLoader.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::BrowserLoader
//
////////////////////////////////////////////////////////////////////////////////

BrowserLoader::BrowserLoader (CassoExplorerBrowser & browser)
    : m_browser (browser)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::ReloadAfterNavigation
//
////////////////////////////////////////////////////////////////////////////////

void BrowserLoader::ReloadAfterNavigation()
{
    HRESULT  hr = m_browser.Refresh();



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::LoadHostFolder
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BrowserLoader::LoadHostFolder (const std::wstring & path)
{
    HRESULT  hr    = S_OK;
    size_t   index = 0;



    //  A network folder is read on a thread of its own; meanwhile the list has
    //  a working message, as for a shell folder, and fills when it arrives.
    if (PathResolver::IsNetworkPath (path) && m_hostListings.HasLister())
    {
        if (!m_hostListings.TryTake (CassoExplorerBrowser::s_kHostListKey + path, path, m_hostEntries, hr))
        {
            m_listError = CassoExplorerBrowser::s_kWorkingText;
            return S_OK;
        }
    }
    else
    {
        hr = m_browser.m_fs.EnumerateAllEntries (path, m_hostEntries);
    }

    if (FAILED (hr))
    {
        m_listError = L"This folder could not be read.";
        return hr;
    }

    std::erase_if (m_hostEntries, [this] (const FileSystemEntry & entry) { return !m_folderOptions.IsShown (entry); });

    for (index = 0; index < m_hostEntries.size(); index++)
    {
        const FileSystemEntry &  entry   = m_hostEntries[index];
        bool                     isImage = !entry.isFolder && TreeModel::IsSupportedImage (entry.name);

        m_browser.m_rows.push_back (CatalogModel::FromHostEntry (entry, isImage, index));
        m_browser.m_rows.back().isArchive = !entry.isFolder && !isImage && m_browser.m_tree.IsArchive (CassoExplorerBrowser::JoinPath (path, entry.name));

        //  A folder is listed by the name in its desktop.ini, as in the tree,
        //  read only for a read-only or system folder on this computer.
        if (entry.isFolder && (entry.isReadOnly || entry.isSystem) && !PathResolver::IsNetworkPath (path) && m_browser.m_tree.GetLocalizedNameLookup())
        {
            m_browser.m_rows.back().label = m_browser.m_tree.GetLocalizedNameLookup() (CassoExplorerBrowser::JoinPath (path, entry.name));
        }

        //  The shell's own type name, as Explorer's Type column shows.
        if (m_browser.m_shellIcons != nullptr)
        {
            std::wstring  typeName = m_browser.m_shellIcons->GetTypeName (CassoExplorerBrowser::JoinPath (path, entry.name), entry.isFolder);

            //  A disk image has Casso's type for it, by its sector order.
            if (!typeName.empty() && CatalogModel::GetDiskImageTypeName (entry.name).empty())
            {
                m_browser.m_rows.back().typeText = std::move (typeName);
            }
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::LoadShellFolder
//
//  What the shell lists in a folder that is not on a disk. An item that is a
//  file or folder on one keeps its path, so it opens and previews as it does
//  in its own folder, and a disk image among them opens as an image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BrowserLoader::LoadShellFolder (const std::wstring & id)
{
    HRESULT                                        hr = S_OK;
    std::vector<IShellItemVerbs::ShellFolderItem>  items;



    if (m_browser.m_shellVerbs == nullptr)
    {
        m_listError = L"This folder could not be read.";
        return E_NOTIMPL;
    }

    //  A slow folder, Network above all, reads on a thread of its own; the
    //  list says so meanwhile, as Explorer's does, and fills when it arrives.
    //  A path inside an archive on a local drive is read at once: the read is
    //  quick, and a placeholder would only flash.
    if (m_browser.m_previewer.IsLocalArchivePath (id))
    {
        hr = m_browser.m_shellVerbs->ListShellFolder (id, items);
    }
    else if (!m_browser.m_shellListings.TryTake (CassoExplorerBrowser::s_kListKey + id, id, items, hr))
    {
        m_listError = CassoExplorerBrowser::s_kWorkingText;
        return S_OK;
    }

    if (FAILED (hr))
    {
        m_listError = PathResolver::IsNetworkPath (id) ? id.substr (id.find_last_of (L"\\/") + 1) + L" is unavailable."
                                                       : std::wstring (L"This folder could not be read.");
        return hr;
    }

    //  An archive the shell lists nothing in gets the reason, damaged or locked,
    //  in place of an empty folder.
    if (items.empty() && m_browser.m_previewer.IsLocalArchivePath (id) && m_browser.m_fs.Exists (id))
    {
        m_listError = m_browser.m_previewer.DescribeEmptyArchive (id);
    }

    //  Where Up goes: a folder on a disk opens as one.
    {
        IShellItemVerbs::ShellFolderItem  parent;
        HRESULT                           hrUp = m_browser.m_shellVerbs->GetShellParent (id, parent);

        if (hrUp == S_OK)
        {
            m_shellParent = parent.path.empty() ? Location::MakeShellFolder (parent.id, parent.name) : Location::MakeHostFolder (parent.path);
        }
    }

    for (size_t index = 0; index < items.size(); index++)
    {
        CatalogRow  row;

        row.name         = items[index].name;
        row.typeText     = items[index].typeText;
        row.sizeBytes    = items[index].sizeBytes;
        row.modifiedUnix = items[index].modifiedUnix;
        row.hasModified  = items[index].hasModified;
        row.isDirectory  = items[index].isFolder;
        //  A folder that is a file too has no folder on the disk to open as one.
        row.hostPath     = (items[index].isFolder && items[index].isFile) ? std::wstring() : items[index].path;
        row.shellId      = items[index].id;
        row.folderPath   = items[index].folder;
        row.imagePath    = items[index].imagePath;
        row.imageCatalogIndex = items[index].catalogIndex;
        row.imageDirectory    = items[index].imageDirectory;
        row.isDiskImage  = !row.isDirectory && !row.hostPath.empty() && TreeModel::IsSupportedImage (row.hostPath);
        row.typeText     = (row.isDiskImage && !CatalogModel::GetDiskImageTypeName (row.name).empty()) ? CatalogModel::GetDiskImageTypeName (row.name) : row.typeText;
        row.sourceIndex  = index;
        row.shellOrder   = items[index].shellOrder;
        row.iconId       = items[index].iconId;

        m_browser.m_rows.push_back (std::move (row));
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::LoadRecycleBin
//
//  What the Recycle Bin holds on every drive, with where each item was
//  deleted from and when.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BrowserLoader::LoadRecycleBin()
{
    HRESULT                                     hr = S_OK;
    std::vector<IShellItemVerbs::RecycledItem>  items;



    BAIL_OUT_IF (m_browser.m_shellVerbs == nullptr, S_OK);

    hr = m_browser.m_shellVerbs->ListRecycled (items);

    if (FAILED (hr))
    {
        m_listError = L"The Recycle Bin could not be read.";
        return hr;
    }

    for (size_t index = 0; index < items.size(); index++)
    {
        CatalogRow  row;

        row.name           = items[index].name;
        row.typeText       = items[index].typeText;
        row.sizeBytes      = items[index].sizeBytes;
        row.modifiedUnix   = items[index].modifiedUnix;
        row.hasModified    = items[index].hasModified;
        row.isDirectory    = items[index].isFolder;
        row.recycledId     = items[index].id;
        row.originalFolder = items[index].originalFolder;
        row.deletedUnix    = items[index].deletedUnix;
        row.hasDeleted     = items[index].hasDeleted;
        row.sourceIndex    = index;

        m_browser.m_rows.push_back (std::move (row));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::GetSelectedRecycledIds
//
////////////////////////////////////////////////////////////////////////////////

void BrowserLoader::GetSelectedRecycledIds (std::vector<std::wstring> & outIds) const
{
    outIds.clear();

    for (int row : m_browser.m_selectedRows)
    {
        if (row >= 0 && (size_t) row < m_browser.m_rows.size() && !m_browser.m_rows[(size_t) row].recycledId.empty())
        {
            outIds.push_back (m_browser.m_rows[(size_t) row].recycledId);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::TryGetRowEntry
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserLoader::TryGetRowEntry (int row, FileEntry & outEntry) const
{
    if (!m_browser.IsVolumeLocation() || row < 0 || (size_t) row >= m_browser.m_rows.size() || m_browser.m_rows[(size_t) row].sourceIndex >= m_browser.m_listing.entries.size())
    {
        return false;
    }

    outEntry = m_browser.m_listing.entries[m_browser.m_rows[(size_t) row].sourceIndex];

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::GetAllRecycledIds
//
////////////////////////////////////////////////////////////////////////////////

void BrowserLoader::GetAllRecycledIds (std::vector<std::wstring> & outIds) const
{
    outIds.clear();

    for (const CatalogRow & row : m_browser.m_rows)
    {
        if (!row.recycledId.empty())
        {
            outIds.push_back (row.recycledId);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::GetImageProblem
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserLoader::GetImageProblem (const std::wstring & imagePath)
{
    VolumeListing           listing;
    VolumeKind              kind = VolumeKind::Unknown;
    DiskOperations::Result  result;



    if (CanListImage (imagePath))
    {
        return std::wstring();
    }

    result = m_browser.m_operations.List (TextEncoding::WideToNarrow (imagePath), listing, kind);

    if (result.Succeeded() || result.message.find (s_kNoFileSystem) != std::string::npos)
    {
        return std::wstring();
    }

    return BrowserCells::FormatImageError (imagePath, result.message);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::LoadRoot
//
//  The Casso root's known folders and This PC's drives, as folder rows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BrowserLoader::LoadRoot (const std::wstring & id)
{
    HRESULT       hr    = S_OK;
    size_t        index = 0;
    std::wstring  link;
    int           slot  = 0;
    int           drive = 0;



    //  Opening a link's root starts it, unless the user disconnected it;
    //  until Casso's program runs on the Apple, the list has the link's
    //  state in place of drives.
    if (m_browser.m_links != nullptr && TreeModel::TryParseLinkId (id, link, slot, drive) && !m_browser.m_links->IsStarted (TextEncoding::WideToNarrow (link))
        && !m_browser.m_links->IsHeld (TextEncoding::WideToNarrow (link)))
    {
        hr = m_browser.m_links->Start (TextEncoding::WideToNarrow (link));
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_browser.m_tree.SetLinks (m_browser.m_links->BuildTreeEntries());
    }

    hr = m_browser.m_tree.GetChildren (id, m_rootChildren);
    CHR (hr);

    if (m_rootChildren.empty() && TreeModel::TryParseLinkId (id, link, slot, drive))
    {
        m_browser.m_tree.GetLinkStatus (link, m_listError);
    }

    for (index = 0; index < m_rootChildren.size(); index++)
    {
        FileSystemEntry  entry;

        m_nodes[m_rootChildren[index].id] = m_rootChildren[index];

        entry.name     = m_rootChildren[index].label;
        entry.isFolder = true;

        m_browser.m_rows.push_back (CatalogModel::FromHostEntry (entry, false, index));
        m_browser.m_rows.back().hostPath  = m_rootChildren[index].location.path;
        m_browser.m_rows.back().isMissing = m_rootChildren[index].missing;

        if (m_rootChildren[index].kind == TreeNode::Kind::CassoLinkConnection)
        {
            m_browser.m_rows.back().typeText = L"CassoLink connection";
            m_browser.m_rows.back().offline  = m_rootChildren[index].offline;
        }
        else if (m_rootChildren[index].kind == TreeNode::Kind::AppleDrive)
        {
            m_browser.m_rows.back().typeText = L"Apple II disk drive";
        }

        //  A drive's tile has its name, its type and how full it is.
        if (m_rootChildren[index].kind == TreeNode::Kind::Drive && m_browser.m_shellIcons != nullptr)
        {
            IShellIcons::DriveInfo  drive;

            if (m_browser.m_shellIcons->GetDriveInfo (m_rootChildren[index].location.path, drive))
            {
                CatalogRow &  row = m_browser.m_rows.back();

                row.name      = drive.name.empty() ? row.name : drive.name;
                row.typeText  = drive.typeName.empty() ? row.typeText : drive.typeName;
                row.isDrive   = true;
                row.sizeBytes = drive.totalBytes;
                row.freeBytes = drive.freeBytes;
            }
        }
    }

    if (id == TreeModel::kCassoLinkRootId)
    {
        std::erase_if (m_browser.m_rows, [this] (const CatalogRow & row)
        {
            return row.sourceIndex < m_rootChildren.size() && m_rootChildren[row.sourceIndex].kind == TreeNode::Kind::AddLink;
        });

        AddCassoLinkItems();
    }

    if (m_browser.m_links != nullptr && TreeModel::TryParseLinkId (id, link, slot, drive) && drive == 0 && IsAdtProServer (TextEncoding::WideToNarrow (link)))
    {
        AddAdtProHelpItem();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::RevisitLink
//
//  A Disk II has no signal for a changed disk, so a machine's
//  drives, or a drive, are read again each time the list goes to them, and
//  on a refresh; a reload as a read arrives is the same visit.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserLoader::RevisitLink (const Location & location)
{
    std::wstring   link;
    int            slot    = 0;
    int            drive   = 0;
    bool           parsed  = m_browser.m_links != nullptr && TreeModel::TryParseLinkId (location.path, link, slot, drive);
    std::string    name    = TextEncoding::WideToNarrow (link);
    VolumeListing  listing;
    VolumeKind     kind    = VolumeKind::Unknown;
    HRESULT        result  = S_OK;
    std::string    message;
    bool           revisit = parsed && location != m_linkVisit && m_browser.m_links->IsStarted (name);



    m_linkVisit = parsed ? location : Location();

    if (!revisit)
    {
        return;
    }

    for (int each = 1; each <= CassoLinkSessionTypes::kDriveCount; each++)
    {
        if ((drive == 0 || drive == each) && !m_browser.m_links->IsWritingDisk (name, each))
        {
            m_browser.m_links->Invalidate (name, each);

            if (drive == 0)
            {
                m_browser.m_links->TryGetListing (name, each, std::string(), listing, kind, result, message);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::AddCassoLinkItems
//
//  CassoLink.dsk and the help, after the machines. Each has the path it is
//  written to when it is opened or dragged, in the temporary folder.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserLoader::AddCassoLinkItems()
{
    wchar_t           temp[MAX_PATH] = {};
    DWORD             length         = GetTempPathW (MAX_PATH, temp);
    FileSystemEntry   add;



    add.name = L"Add a CassoLink connection...";
    m_browser.m_rows.push_back (CatalogModel::FromHostEntry (add, false, m_rootChildren.size() + m_browser.m_rows.size()));
    m_browser.m_rows.back().cassoLinkItem = CatalogRow::CassoLinkItem::AddMachine;
    m_browser.m_rows.back().isDirectory   = true;            // no size, and listed with the machines
    m_browser.m_rows.back().typeText.clear();

    for (CatalogRow::CassoLinkItem item : { CatalogRow::CassoLinkItem::BootDisk, CatalogRow::CassoLinkItem::Help })
    {
        FileSystemEntry  entry;
        bool             disk = item == CatalogRow::CassoLinkItem::BootDisk;

        entry.name = disk ? L"CassoLink.dsk" : L"What's CassoLink.md";
        m_browser.m_rows.push_back (CatalogModel::FromHostEntry (entry, disk, m_rootChildren.size() + m_browser.m_rows.size()));
        m_browser.m_rows.back().cassoLinkItem = item;
        m_browser.m_rows.back().sizeBytes     = disk ? NibblizationLayer::kImageByteSize : wcslen (GetCassoLinkHelp());
        m_browser.m_rows.back().hostPath      = (length > 0) ? std::wstring (temp) + entry.name : std::wstring();

        if (!disk && m_browser.m_shellIcons != nullptr && !m_browser.m_shellIcons->GetTypeName (m_browser.m_rows.back().hostPath, false).empty())
        {
            m_browser.m_rows.back().typeText = m_browser.m_shellIcons->GetTypeName (m_browser.m_rows.back().hostPath, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::IsAdtProServer
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserLoader::IsAdtProServer (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_browser.m_links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.transport == LinkTransport::AdtProServer;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::AddAdtProHelpItem
//
//  The server's help, after its folder, a virtual file written to the
//  temporary folder only when it is opened or dragged; nothing is written
//  to the user's Documents for it.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserLoader::AddAdtProHelpItem()
{
    wchar_t           temp[MAX_PATH] = {};
    DWORD             length         = GetTempPathW (MAX_PATH, temp);
    FileSystemEntry   entry;



    entry.name = L"What's an ADTPro-compatible server.md";
    m_browser.m_rows.push_back (CatalogModel::FromHostEntry (entry, false, m_rootChildren.size() + m_browser.m_rows.size()));
    m_browser.m_rows.back().cassoLinkItem = CatalogRow::CassoLinkItem::AdtProHelp;
    m_browser.m_rows.back().sizeBytes     = wcslen (GetAdtProHelp());
    m_browser.m_rows.back().hostPath      = (length > 0) ? std::wstring (temp) + entry.name : std::wstring();

    if (m_browser.m_shellIcons != nullptr && !m_browser.m_shellIcons->GetTypeName (m_browser.m_rows.back().hostPath, false).empty())
    {
        m_browser.m_rows.back().typeText = m_browser.m_shellIcons->GetTypeName (m_browser.m_rows.back().hostPath, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::GetHelpText
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * BrowserLoader::GetHelpText (CatalogRow::CassoLinkItem item)
{
    return (item == CatalogRow::CassoLinkItem::AdtProHelp) ? GetAdtProHelp() : GetCassoLinkHelp();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::PrepareCassoLinkItems
//
//  The boot disk is the universal one, at 115,200 baud on a serial port; a
//  Wi-Fi modem's disk, which dials this PC, is saved from its connection's
//  menu.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserLoader::PrepareCassoLinkItems()
{
    HRESULT            hr    = S_OK;
    std::vector<Byte>  bytes;
    std::string        help;



    for (int row : m_browser.m_selectedRows)
    {
        const CatalogRow *  item  = (row >= 0 && (size_t) row < m_browser.m_rows.size()) ? &m_browser.m_rows[(size_t) row] : nullptr;
        bool                named = false;

        if (item == nullptr || item->cassoLinkItem == CatalogRow::CassoLinkItem::None || item->cassoLinkItem == CatalogRow::CassoLinkItem::AddMachine)
        {
            continue;
        }

        named = !item->hostPath.empty();
        CBREx (named, E_UNEXPECTED);

        if (item->cassoLinkItem == CatalogRow::CassoLinkItem::BootDisk)
        {
            hr = LinkProgram::BuildUniversalBootDisk (LinkShimOptions(), bytes);
            CHR (hr);
        }
        else
        {
            help  = TextEncoding::WideToNarrow (GetHelpText (item->cassoLinkItem));
            bytes = std::vector<Byte> (help.begin(), help.end());
        }

        hr = m_browser.m_operations.GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (item->hostPath), bytes);
        CHR (hr);
    }

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::TryGetSelectedCassoLinkPaths
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserLoader::TryGetSelectedCassoLinkPaths (std::vector<std::wstring> & outPaths) const
{
    outPaths.clear();

    for (int row : m_browser.m_selectedRows)
    {
        bool  item = row >= 0 && (size_t) row < m_browser.m_rows.size() && m_browser.m_rows[(size_t) row].cassoLinkItem != CatalogRow::CassoLinkItem::None
                  && m_browser.m_rows[(size_t) row].cassoLinkItem != CatalogRow::CassoLinkItem::AddMachine;

        if (!item)
        {
            outPaths.clear();
            return false;
        }

        outPaths.push_back (m_browser.m_rows[(size_t) row].hostPath);
    }

    return !outPaths.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::GetAdtProHelp
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * BrowserLoader::GetAdtProHelp()
{
    return L"# What's an ADTPro-compatible server?\r\n"
           L"\r\n"
           L"[ADTPro](https://adtpro.com) is a program for an Apple II that sends and receives whole disks as "
           L"image files, over a serial cable or a network. This connection serves the standard ADTPro client "
           L"on the Apple as ADTPro's server does, from the folder listed under it.\r\n"
           L"\r\n"
           L"## Using it\r\n"
           L"\r\n"
           L"1. Start ADTPro on the Apple, set to this connection's speed.\r\n"
           L"2. Use ADTPro's menu on the Apple as with ADTPro's server:\r\n"
           L"   - **Send** puts a disk from the Apple into the folder as an image.\r\n"
           L"   - **Receive** writes an image from the folder onto a disk in the Apple.\r\n"
           L"   - **Dir** lists the folder, and **CD** moves into a folder inside it.\r\n"
           L"   - **VSDrive** serves Virtual.po and Virtual2.po from the folder as drives the Apple can open.\r\n"
           L"\r\n"
           L"## Notes\r\n"
           L"\r\n"
           L"- Files dropped onto the folder in Casso Explorer land there, ready for **Receive**.\r\n"
           L"- The connection is open while Casso Explorer runs, so the PC is there when the Apple starts ADTPro.\r\n"
           L"- A request for a file or folder outside the folder fails, so the Apple cannot read or write "
           L"anything else on this PC.\r\n"
           L"- A CassoLink connection can do more: browse catalogs, copy single files, and read or write whole "
           L"disks from Casso Explorer. Boot CassoLink.dsk on the Apple for it.\r\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::GetCassoLinkHelp
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * BrowserLoader::GetCassoLinkHelp()
{
    return L"# What's CassoLink?\r\n"
           L"\r\n"
           L"CassoLink moves files and whole disks between **Casso Explorer** and an **Apple II**: a real one on a "
           L"serial cable or a Wi-Fi modem, or one running in Casso.\r\n"
           L"\r\n"
           L"## Connecting an Apple\r\n"
           L"\r\n"
           L"1. **Add the connection.** Choose *Add a CassoLink connection* from See more (**...**), or from the CassoLink "
           L"folder, and pick how the Apple is connected:\r\n"
           L"   - **Serial**: a null modem cable from the Apple //c's port 2, or a Super Serial Card, to a COM port "
           L"or a USB serial adapter.\r\n"
           L"   - **Wi-Fi modem**: a serial Wi-Fi modem on the Apple's port 2.\r\n"
           L"   - **Uthernet II**: the card in a ][+ or //e on this PC's network. Booted from CassoLink.dsk, the Apple "
           L"gets an address by DHCP and is listed under CassoLink without being added.\r\n"
           L"   - **Emulated**: an Apple running in Casso, with its serial port on TCP. A running Casso is listed "
           L"under CassoLink without being added.\r\n"
           L"2. **Start CassoLink on the Apple**, either way:\r\n"
           L"   - Boot **CassoLink.dsk** from the CassoLink folder: drag it into Casso, or write it to a floppy. "
           L"The //c's serial ports, a Super Serial Card in any slot and an Uthernet II are all detected, and the "
           L"link is made on the first one this PC connects to. For a Wi-Fi modem, save the disk from the "
           L"connection's menu instead, so it dials this PC.\r\n"
           L"   - With no disk, on a serial cable: at the Apple's prompt, type `IN#2` (on a Super Serial Card, "
           L"`IN#` and its slot), then press Return; type "
           L"`Ctrl+A 14B`, then press Return; then choose *Bootstrap the Apple* from the connection's menu. The rest "
           L"is typed in over the cable.\r\n"
           L"   - With no disk, on a ][+ or //e: connect this PC's sound output to the Apple's cassette input, and "
           L"choose *Bootstrap from the cassette input* from the connection's menu.\r\n"
           L"3. **Click the connection** under CassoLink. Its drives are listed once the connection is made.\r\n"
           L"\r\n"
           L"## Once connected\r\n"
           L"\r\n"
           L"- Open a drive to browse its catalog, and select a file to preview it.\r\n"
           L"- Drag files to and from a drive to copy them.\r\n"
           L"- Drag a drive to a folder to make an image of the whole disk, or drag an image onto a drive to write "
           L"it to the disk.\r\n"
           L"- After changing the disk in a drive, open the drive again, or press F5, to read the new disk.\r\n"
           L"- Choose *Write CassoLink to disk* from a drive's menu to make a boot disk on the Apple itself.\r\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::LoadImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BrowserLoader::LoadImage (const std::wstring & path, const std::string & directory)
{
    DiskOperations::Result  result;
    HRESULT                 hrReadOnly = S_OK;
    bool                    cached     = directory.empty() && m_browser.m_model.TryGetCachedCatalog (path, m_browser.m_listing, m_browser.m_kind);



    if (!cached)
    {
        result = m_browser.m_operations.List (TextEncoding::WideToNarrow (path), directory, m_browser.m_listing, m_browser.m_kind);

        if (!result.Succeeded())
        {
            m_listError = BrowserCells::FormatImageError (path, result.message);
            return result.hr;
        }

        //  The cache holds an image's catalog, which is its volume directory,
        //  so a subdirectory's listing is read fresh each time.
        if (directory.empty())
        {
            m_browser.m_model.CacheCatalog (path, m_browser.m_listing, m_browser.m_kind);
        }
    }

    m_browser.m_isImage = true;
    CatalogModel::FromListing (m_browser.m_listing, m_browser.m_kind, m_browser.m_rows);

    //  A read-only file refuses every write the runner would make, so the
    //  verbs that write are offered grayed rather than failing at the end.
    hrReadOnly = m_browser.m_fs.GetReadOnlyAttribute (path, m_writeProtected);
    IGNORE_RETURN_VALUE (hrReadOnly, S_OK);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::LoadAppleDrive
//
//  A drive of a linked Apple, read over the link: the list has a working
//  message until the listing arrives, when the window refreshes it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BrowserLoader::LoadAppleDrive (const Location & location)
{
    HRESULT       hr      = S_OK;
    std::wstring  link;
    std::string   message;
    int           slot    = 0;
    int           drive   = 0;
    bool          parsed  = TreeModel::TryParseLinkId (location.path, link, slot, drive) && drive != 0;



    if (m_browser.m_links == nullptr || !parsed)
    {
        m_listError = L"This drive could not be read.";
        return HRESULT_FROM_WIN32 (ERROR_INVALID_DRIVE);
    }

    if (m_browser.m_links->IsWritingDisk (TextEncoding::WideToNarrow (link), drive))
    {
        m_listError = L"The disk is not available because an image is being written to it. Try again after the write finishes.";
        return S_OK;
    }

    if (!m_browser.m_links->TryGetListing (TextEncoding::WideToNarrow (link), drive, location.innerPath, m_browser.m_listing, m_browser.m_kind, hr, message))
    {
        m_listError = CassoExplorerBrowser::s_kWorkingText;
        return S_OK;
    }

    if (FAILED (hr))
    {
        m_listError = BrowserCells::FormatLinkError (hr, message);
        return hr;
    }

    m_browser.m_isAppleDrive = true;
    CatalogModel::FromListing (m_browser.m_listing, m_browser.m_kind, m_browser.m_rows);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::LeaveMissingLocation
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserLoader::LeaveMissingLocation()
{
    Location      location = m_browser.GetLocation();
    std::wstring  path     = location.path;
    size_t        slash    = 0;



    if (location.kind != Location::Kind::HostFolder && location.kind != Location::Kind::DiskImage &&
        location.kind != Location::Kind::DiskDirectory)
    {
        return false;
    }

    if (path.empty() || m_browser.m_fs.Exists (path) || m_browser.GetTreeModel().IsDirectory (path))
    {
        return false;
    }

    do
    {
        slash = path.find_last_of (L'\\');

        if (slash == std::wstring::npos)
        {
            return false;
        }

        //  A drive's root keeps its backslash.
        path = (slash <= 2) ? path.substr (0, slash + 1) : path.substr (0, slash);
    }
    while (!m_browser.GetTreeModel().IsDirectory (path) && path.size() > 3);

    if (!m_browser.GetTreeModel().IsDirectory (path))
    {
        return false;
    }

    m_browser.NavigateToLocation (Location::MakeHostFolder (path));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::TryGetImageLocation
//
//  A disk image row on the disk, by its path, whatever its contents.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserLoader::TryGetImageLocation (int row, Location & outLocation) const
{
    Location      location = m_browser.GetLocation();
    std::wstring  path;



    if (row < 0 || row >= (int) m_browser.m_rows.size() || !m_browser.m_rows[(size_t) row].isDiskImage)
    {
        return false;
    }

    path = (location.kind == Location::Kind::HostFolder)  ? CassoExplorerBrowser::JoinPath (location.path, m_browser.m_rows[(size_t) row].name)
         : (location.kind == Location::Kind::ShellFolder) ? m_browser.m_rows[(size_t) row].hostPath
                                                          : std::wstring();

    if (path.empty())
    {
        return false;
    }

    outLocation = Location::MakeDiskImage (path);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::IsOnMissingDrive
//
//  A tab on a drive that is not attached now is left closed rather than
//  opened on an error. Only a drive letter is checked: a query to a network
//  share could take as long as the network takes to answer.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserLoader::IsOnMissingDrive (const Location & location) const
{
    bool  hostPath = location.kind == Location::Kind::HostFolder
                  || location.kind == Location::Kind::DiskImage
                  || location.kind == Location::Kind::DiskDirectory;



    return hostPath && location.path.size() >= 2 && location.path[1] == L':'
        && !m_browser.m_fs.DirectoryExists (location.path.substr (0, 2) + L"\\");
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader::CanListImage
//
//  Whether an image has a catalog to open, read once and cached; an image
//  with no file system has nothing to open into.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserLoader::CanListImage (const std::wstring & imagePath)
{
    VolumeListing           listing;
    VolumeKind              kind   = VolumeKind::Unknown;
    DiskOperations::Result  result;
    bool                    listed = m_browser.m_model.TryGetCachedCatalog (imagePath, listing, kind);



    if (!listed)
    {
        result = m_browser.m_operations.List (TextEncoding::WideToNarrow (imagePath), listing, kind);
        listed = result.Succeeded();

        if (listed)
        {
            m_browser.m_model.CacheCatalog (imagePath, listing, kind);
        }
    }

    return listed;
}

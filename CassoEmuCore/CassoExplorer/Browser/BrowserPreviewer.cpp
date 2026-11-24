#include "Pch.h"

#include "CassoExplorer/Browser/BrowserPreviewer.h"
#include "CassoExplorer/Browser/BrowserHostInterfaces.h"
#include "CassoExplorer/Browser/BrowserSelection.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "Core/AppleSingleCodec.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/ArchiveProbe.h"
#include "CassoExplorer/Model/DiskMessageText.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "CassoExplorer/Model/HostTextPreview.h"
#include "CassoExplorer/Model/PreviewViewChoices.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Devices/Disk/DiskImageSession.h"
#include "FileTypes/DiskSaveTable.h"
#include "FileTypes/WizardryMaze.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::BrowserPreviewer
//
////////////////////////////////////////////////////////////////////////////////

BrowserPreviewer::BrowserPreviewer (
    const BrowserParts      & parts,
    const BrowserSelection  & selection,
    IBrowserPreviewHost     & host)
    : m_parts     (parts),
      m_selection (selection),
      m_host      (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewAppleDrive
//
//  False for a row that is not a linked Apple's drive.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::PreviewAppleDrive (const CatalogRow & row)
{
    std::wstring   link;
    int            slot    = 0;
    int            drive   = 0;
    VolumeListing  listing;
    VolumeKind     kind    = VolumeKind::Unknown;
    HRESULT        result  = S_OK;
    std::string    message;



    if (m_parts.sources.links == nullptr || !TreeModel::TryParseLinkId (row.hostPath, link, slot, drive) || drive == 0)
    {
        return false;
    }

    m_preview.kind = PreviewContent::Kind::Error;

    if (m_parts.sources.links->IsWritingDisk (TextEncoding::WideToNarrow (link), drive))
    {
        m_preview.message = L"The disk is not available because an image is being written to it.";
    }
    else if (!m_parts.sources.links->TryGetListing (TextEncoding::WideToNarrow (link), drive, std::string(), listing, kind, result, message))
    {
        m_preview.message = CassoExplorerBrowser::s_kWorkingText;
    }
    else if (FAILED (result))
    {
        m_preview.message = DiskMessageText::FormatLinkError (result, message);
    }
    else
    {
        m_preview = PreviewContent();
        PreviewDecoder::RenderCatalog (listing, kind, m_preview);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewCassoLinkItem
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::PreviewCassoLinkItem (const CatalogRow & row)
{
    bool          help = row.cassoLinkItem == CatalogRow::CassoLinkItem::Help || row.cassoLinkItem == CatalogRow::CassoLinkItem::AdtProHelp;
    std::wstring  text = help                                                          ? std::wstring (BrowserLoader::GetHelpText (row.cassoLinkItem))
                       : (row.cassoLinkItem == CatalogRow::CassoLinkItem::AddMachine) ? std::wstring (L"Open it to add a connection to an Apple: a serial cable, a Wi-Fi modem, or a Casso on TCP.\r\n")
                       : std::wstring (L"The CassoLink boot disk.\r\n\r\nBooting it starts CassoLink on the Apple, on each of its serial ports "
                                       L"at 115,200 baud and on an Uthernet II, which gets its address by DHCP. The link is made on the "
                                       L"first one this PC connects to. Drag it into Casso or onto a folder, or write it to a floppy.\r\n");
    size_t        start = 0;
    size_t        end   = 0;



    std::string   bytes = TextEncoding::WideToNarrow (BrowserLoader::GetHelpText (row.cassoLinkItem));
    HRESULT       hr    = S_OK;



    m_preview      = PreviewContent();
    m_preview.kind = PreviewContent::Kind::Text;

    //  The help is written out for Windows' own Markdown preview, where one
    //  is installed; otherwise the preview has the text as it is.
    if (help && m_parts.sources.shellVerbs != nullptr && !row.hostPath.empty())
    {
        hr = m_parts.operations.GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (row.hostPath), std::vector<Byte> (bytes.begin(), bytes.end()));

        if (SUCCEEDED (hr) && m_parts.sources.shellVerbs->HasShellPreview (row.hostPath))
        {
            m_preview.kind     = PreviewContent::Kind::Shell;
            m_preview.hostPath = row.hostPath;
            return;
        }
    }

    while (start < text.size())
    {
        end = text.find (L"\r\n", start);
        end = (end == std::wstring::npos) ? text.size() : end;
        m_preview.lines.push_back (text.substr (start, end - start));
        start = end + 2;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewAppleEntry
//
//  A file on a linked Apple's drive, read over the link and then previewed
//  as a file in an image is.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::PreviewAppleEntry (const std::string & entryPath, const FileEntry & entry)
{
    HRESULT       hr          = S_OK;
    FilePayload   payload;
    std::wstring  link;
    std::string   message;
    int           slot        = 0;
    int           drive       = 0;



    TreeModel::TryParseLinkId (GetLocation().path, link, slot, drive);

    if (!m_parts.sources.links->TryGetFile (TextEncoding::WideToNarrow (link), drive, entryPath, payload, hr, message))
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = CassoExplorerBrowser::s_kWorkingText;
        return;
    }

    if (SUCCEEDED (hr))
    {
        PreviewDecoder::HexForm  hexForm = m_viewState.GetHexForm (GetLocation().path + L"|" + TextEncoding::NarrowToWide (entryPath));

        hr = PreviewViewChoices::Render (m_viewState.GetViewAs(), entry, m_parts.list.kind, payload, hexForm, m_bus, m_preview, m_previewMonitor, m_customArgb,
                                         m_viewState.GetDecodeOptions (GetLocation().path, entryPath));
    }

    if (FAILED (hr) && m_preview.message.empty())
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = message.empty() ? L"This file could not be previewed." : TextEncoding::NarrowToWide (message);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::UpdatePreview
//
//  One selected file in an image previews its contents; one selected disk
//  image in a host folder previews its catalog. Anything else, including a
//  multiple selection, previews nothing.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::UpdatePreview()
{
    const FileEntry         * entry     = nullptr;
    Location                  location  = GetLocation();
    std::wstring              imagePath;
    std::wstring              folder    = location.path;



    m_preview = PreviewContent();
    m_preview.kind = PreviewContent::Kind::Text;

    if (m_selection.TryGetSelectedEntry (entry))
    {
        if (entry->isDirectory)
        {
            m_preview.kind    = PreviewContent::Kind::Error;
            m_preview.message = L"A folder. Open it to see what it contains.";
            return;
        }

        if (m_parts.list.isAppleDrive)
        {
            PreviewAppleEntry (BrowserRowPaths::GetEntryPath (GetLocation(), *entry), *entry);
        }
        else
        {
            PreviewEntry (location.path, BrowserRowPaths::GetEntryPath (GetLocation(), *entry), *entry, m_parts.list.kind);
        }

        return;
    }

    //  CassoLink's own items, and a linked Apple's drive, which previews as
    //  its catalog once it is read.
    if (location.kind == Location::Kind::Root && m_parts.list.selectedRows.size() == 1 && m_parts.list.selectedRows[0] >= 0 && (size_t) m_parts.list.selectedRows[0] < m_parts.list.rows.size())
    {
        const CatalogRow &  row = m_parts.list.rows[(size_t) m_parts.list.selectedRows[0]];

        if (row.cassoLinkItem != CatalogRow::CassoLinkItem::None)
        {
            PreviewCassoLinkItem (row);
            return;
        }

        if (PreviewAppleDrive (row))
        {
            return;
        }
    }

    //  A search's match inside a disk image previews as it does in its image.
    if (m_parts.list.selectedRows.size() == 1 && m_parts.list.selectedRows[0] >= 0 && (size_t) m_parts.list.selectedRows[0] < m_parts.list.rows.size() && !m_parts.list.rows[(size_t) m_parts.list.selectedRows[0]].imagePath.empty())
    {
        PreviewSearchMatch (m_parts.list.rows[(size_t) m_parts.list.selectedRows[0]]);
        return;
    }

    //  A search's result on the disk previews as it does in its own folder.
    if (!m_parts.list.isImage && m_parts.list.selectedRows.size() == 1 && SearchQuery::IsId (location.path) && !m_parts.list.rows[m_parts.list.selectedRows[0]].folderPath.empty())
    {
        folder = m_parts.list.rows[m_parts.list.selectedRows[0]].folderPath;
    }

    //  A file on Home with a file on the disk previews as it does in its folder.
    else if (!m_parts.list.isImage && m_host.IsHomeLocation() && m_parts.list.selectedRows.size() == 1 && !m_parts.list.rows[m_parts.list.selectedRows[0]].hostPath.empty())
    {
        folder = BrowserRowPaths::GetParentFolder (m_parts.list.rows[m_parts.list.selectedRows[0]].hostPath);
    }
    else if (!m_parts.list.isImage && m_host.IsHomeLocation() && m_parts.list.selectedRows.size() == 1 && m_parts.list.rows[m_parts.list.selectedRows[0]].hostPath.empty())
    {
        //  A cloud item has no file here, and its stream is a web page.
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = L"No preview available.";
        return;
    }
    else if (!m_parts.list.isImage && (location.kind == Location::Kind::ShellFolder || m_host.IsHomeLocation()) && m_parts.list.selectedRows.size() == 1)
    {
        PreviewShellItem (m_parts.list.rows[m_parts.list.selectedRows[0]]);
        return;
    }
    else if (m_parts.list.isPartitionList && m_parts.list.selectedRows.size() == 1 && !m_parts.list.rows[m_parts.list.selectedRows[0]].containerPath.empty())
    {
        //  A partition of a card or a card image previews as its volume's catalog.
        PreviewImageCatalog (m_parts.list.rows[m_parts.list.selectedRows[0]].containerPath);
        return;
    }
    else if (!m_parts.list.isImage && location == Location::MakeRoot (TreeModel::kCassoRootId) && m_parts.list.selectedRows.size() == 1
             && !m_parts.list.rows[m_parts.list.selectedRows[0]].isDirectory && !m_parts.list.rows[m_parts.list.selectedRows[0]].hostPath.empty())
    {
        //  An image in the Casso node's list previews as it does in its folder.
        folder = BrowserRowPaths::GetParentFolder (m_parts.list.rows[m_parts.list.selectedRows[0]].hostPath);
    }
    else if (m_parts.list.isImage || location.kind != Location::Kind::HostFolder || m_parts.list.selectedRows.size() != 1)
    {
        return;
    }

    //  An archive previews as what it holds, as a disk image does.
    if (m_parts.list.rows[m_parts.list.selectedRows[0]].isArchive)
    {
        PreviewArchive (BrowserRowPaths::JoinPath (folder, m_parts.list.rows[m_parts.list.selectedRows[0]].name));
        return;
    }

    //  Any other file in a host folder.
    if (!m_parts.list.rows[m_parts.list.selectedRows[0]].isDiskImage)
    {
        PreviewHostFile (folder, m_parts.list.rows[m_parts.list.selectedRows[0]]);
        return;
    }

    imagePath = folder;

    if (!imagePath.empty() && imagePath.back() != L'\\')
    {
        imagePath += L'\\';
    }

    imagePath += m_parts.list.rows[m_parts.list.selectedRows[0]].name;

    PreviewImageCatalog (imagePath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewImageCatalog
//
//  The image's catalog, cached; its details when it has no file system.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::PreviewImageCatalog (const std::wstring & imagePath)
{
    ImageContents  contents;
    VolumeListing  listing;
    VolumeKind     kind     = VolumeKind::Unknown;
    DecodedFile    diskSave;
    bool           hasSave  = false;



    if (!m_parts.model.TryGetCachedCatalog (imagePath, listing, kind))
    {
        HRESULT  hrDescribe = m_parts.catalog.Describe (imagePath, std::string(), contents);

        IGNORE_RETURN_VALUE (hrDescribe, S_OK);

        listing = std::move (contents.listing);
        kind    = contents.kind;

        //  A disk with no file system may be a game's saved game, such as an
        //  Infocom save disk.
        hasSave = !contents.result.Succeeded() && TryDecodeDiskSave (imagePath, diskSave);

        if (hasSave && m_viewState.IsSavedGameShown())
        {
            PreviewDecoder::FromDecoded (diskSave, m_preview);
            m_preview.hasSavedGame   = true;
            m_preview.savedGameShown = true;
            m_preview.savedGameName  = diskSave.viewName;
            return;
        }

        if (!contents.result.Succeeded() && PreviewPartitions (imagePath, contents.layout, contents.partitions))
        {
            return;
        }

        if (!contents.result.Succeeded() && PreviewDecoder::ParseDetails (contents.result.message, m_preview.details))
        {
            m_preview.kind         = PreviewContent::Kind::Details;
            m_preview.hasSavedGame  = hasSave;
            m_preview.savedGameName = diskSave.viewName;
            AddWozDetails (imagePath);
            return;
        }

        if (!contents.result.Succeeded())
        {
            m_preview.kind    = PreviewContent::Kind::Error;
            m_preview.message = contents.error;
            return;
        }

        m_parts.model.CacheCatalog (imagePath, listing, kind);
    }

    if (m_viewState.IsSavedGameShown() && PreviewSavedGame (imagePath, listing, kind))
    {
        m_preview.hasSavedGame   = true;
        m_preview.savedGameShown = true;
        return;
    }

    //  A DOS 3.3 or ProDOS disk may also be a game's saved game, such as an
    //  Ultima IV or Ultima V save disk, whose map is in sectors outside its
    //  files.
    hasSave = (kind == VolumeKind::Dos33 || kind == VolumeKind::ProDos) && TryDecodeDiskSave (imagePath, diskSave);

    if (hasSave && m_viewState.IsSavedGameShown())
    {
        PreviewDecoder::FromDecoded (diskSave, m_preview);
        m_preview.hasSavedGame   = true;
        m_preview.savedGameShown = true;
        m_preview.savedGameName  = diskSave.viewName;
        return;
    }

    PreviewDecoder::RenderCatalog (listing, kind, m_preview);
    AddWozDetails (imagePath);

    m_preview.hasSavedGame  = hasSave || FindSavedGame (listing, kind) != nullptr;
    m_preview.savedGameName = diskSave.viewName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::TryDecodeDiskSave
//
//  The image decoded by DiskSaveTable::DecodeImage, with a companion disk
//  looked for in the same folder. False for an image that can't be read or
//  that no entry accepts.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::TryDecodeDiskSave (const std::wstring & imagePath, DecodedFile & outFile)
{
    HRESULT            hr     = S_OK;
    std::string        narrow = TextEncoding::WideToNarrow (imagePath);
    std::vector<Byte>  bytes;



    hr = m_parts.operations.GetFileIo().ReadAllBytes (narrow, bytes);
    CHR (hr);

    hr = DiskSaveTable::DecodeImage (bytes, narrow, [this, &narrow] (DiskSaveTable::IsFoundFn isCompanion, std::vector<Byte> & outSectors)
                                     {
                                         return DiskSaveTable::FindBeside (m_parts.operations.GetFileIo(), narrow, isCompanion, outSectors);
                                     }, outFile);
    CHR (hr);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::FindSavedGame
//
//  A Wizardry scenario disk's SCENARIO.DATA, Wizardry IV's WERDNA.DATA or
//  Wizardry V's PLAYER.DATA, or null for any other disk.
//
////////////////////////////////////////////////////////////////////////////////

const FileEntry * BrowserPreviewer::FindSavedGame (const VolumeListing & listing, VolumeKind kind)
{
    auto  scenario = std::find_if (listing.entries.begin(), listing.entries.end(),
                                   [] (const FileEntry & entry)
                                   {
                                       return !entry.isDirectory && (_stricmp (entry.name.c_str(), "SCENARIO.DATA") == 0 || _stricmp (entry.name.c_str(), "PLAYER.DATA") == 0
                                                                   || _stricmp (entry.name.c_str(), "WERDNA.DATA") == 0);
                                   });



    return (kind == VolumeKind::Pascal && scenario != listing.entries.end()) ? &*scenario : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewSavedGame
//
//  A Wizardry disk previews as the saved game in its SCENARIO.DATA or
//  PLAYER.DATA: the roster, and the map of the level the party is on. False,
//  with the preview cleared, for any other disk or a file that doesn't decode.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::PreviewSavedGame (const std::wstring & imagePath, const VolumeListing & listing, VolumeKind kind)
{
    const FileEntry                 * scenario = FindSavedGame (listing, kind);
    std::vector<Byte>                 bytes;
    std::vector<Byte>                 sectors;
    std::vector<std::vector<Byte>>    sides;
    SectorDecodeReport                report;
    std::string                       narrow   = TextEncoding::WideToNarrow (imagePath);



    if (scenario == nullptr)
    {
        return false;
    }

    //  Wizardry V's roster has item numbers, and the text for them is
    //  elsewhere on the disk. Its maze is on the other disks.
    if (_stricmp (scenario->name.c_str(), "PLAYER.DATA") == 0)
    {
        HRESULT  read = m_parts.operations.GetFileIo().ReadAllBytes (narrow, bytes);

        IGNORE_RETURN_VALUE (read, S_OK);
        WizardryMaze::FindSides (m_parts.operations.GetFileIo(), narrow, sides);
    }

    if (!bytes.empty())
    {
        HRESULT  loaded = VolumeImage::Load (bytes, narrow, sectors, report);

        IGNORE_RETURN_VALUE (loaded, S_OK);
    }

    PreviewEntry (imagePath, scenario->name, *scenario, kind, sectors, sides);

    if (m_preview.kind != PreviewContent::Kind::Document)
    {
        m_preview = PreviewContent();
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewPartitions
//
//  An image of partitions previews as its partition map and a line for
//  each partition: its label, and its volume and size. False for an image
//  with no partitions.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::PreviewPartitions (const std::wstring & imagePath, PartitionLayoutKind layout, const std::vector<PartitionEntry> & entries)
{
    if (layout == PartitionLayoutKind::None || entries.empty())
    {
        return false;
    }

    m_preview.details.clear();
    m_preview.details.emplace_back (L"Partition map", TextEncoding::NarrowToWide (PartitionLayout::GetLayoutName (layout)));

    for (const PartitionEntry & entry : entries)
    {
        m_preview.details.emplace_back (DiskImageCatalog::GetPartitionLabel (imagePath, entry, false),
                                        TextEncoding::NarrowToWide (PartitionAccess::GetVolumeLabel (entry) + ", " + PartitionAccess::FormatSize (entry.byteCount)));
    }

    m_preview.kind = PreviewContent::Kind::Details;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::AddWozDetails
//
//  A WOZ image's path, for Edit metadata, and for a catalog, the image's WOZ
//  info and metadata as an image with no file system has them in its
//  details, to list above the catalog. Any other image, or a file that can't
//  be read, has neither.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::AddWozDetails (const std::wstring & imagePath)
{
    HRESULT                 hr     = S_OK;
    bool                    isWoz  = _wcsicmp (fs::path (imagePath).extension().wstring().c_str(), L".woz") == 0;
    std::vector<Byte>       bytes;
    WozLoader::Description  woz;
    bool                    parsed = false;
    DetailIterator          analysis;



    CBR (isWoz);

    hr = m_parts.operations.GetFileIo().ReadAllBytes (TextEncoding::WideToNarrow (imagePath), bytes);
    CHR (hr);

    WozLoader::Describe (bytes, woz);
    CBR (woz.isWoz);

    m_preview.wozPath = imagePath;

    //  The analysis after the WOZ sections is about an image with no catalog,
    //  so a catalog has only the WOZ sections.
    if (m_preview.kind == PreviewContent::Kind::Catalog)
    {
        parsed   = PreviewDecoder::ParseDetails (DiskImageSession::DescribeWozChunks (bytes) + DiskImageSession::DescribeContainerHeader (bytes), m_preview.details);
        analysis = std::find_if (m_preview.details.begin(), m_preview.details.end(), [] (const std::pair<std::wstring, std::wstring> & d) { return d.first == kAnalysisHeading && d.second.empty(); });

        m_preview.details.erase (analysis, m_preview.details.end());
        IGNORE_RETURN_VALUE (parsed, false);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::IsLocalArchivePath
//
//  A path on a lettered drive that the shell, not the file system, lists:
//  an archive or a folder inside one.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::IsLocalArchivePath (const std::wstring & id) const
{
    return m_parts.sources.shellVerbs != nullptr && id.size() > 3 && id[1] == L':' && id[2] == L'\\'
        && !m_parts.fs.DirectoryExists (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::DescribeEmptyArchive
//
//  Read whole, up to a size past which a damaged file is more likely than
//  an archive worth probing, and judged by its directory.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserPreviewer::DescribeEmptyArchive (const std::wstring & path) const
{
    std::error_code  error;
    uintmax_t        size  = fs::file_size (fs::path (path), error);
    std::string      bytes;
    std::wstring     name  = path.substr (path.find_last_of (L"\\/") + 1);
    HRESULT          hr    = S_OK;



    if (!error && size > s_kArchiveProbeMaxBytes)
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
//  BrowserPreviewer::ShowShellPreviewBytes
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::ShowShellPreviewBytes()
{
    if (m_preview.kind == PreviewContent::Kind::Shell)
    {
        m_preview.kind = PreviewContent::Kind::Hex;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::GetExtractTarget
//
//  The archive Extract all applies to: the one selected alone in a host
//  folder, or the one open, found as the first part of the open path that is
//  a file. Empty when there is none.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserPreviewer::GetExtractTarget() const
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





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewArchive
//
//  What an archive holds, folders first, in the columns a catalog uses.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::PreviewArchive (const std::wstring & path)
{
    HRESULT                                        hr = S_OK;
    std::vector<IShellItemVerbs::ShellFolderItem>  items;



    if (m_parts.sources.shellVerbs == nullptr)
    {
        return;
    }

    //  One on a network share is read on a thread of its own, with a working
    //  message meanwhile, so an unreachable share never blocks the window.
    if (!PathResolver::IsNetworkPath (path))
    {
        hr = m_parts.sources.shellVerbs->ListShellFolder (path, items);
    }
    else if (!m_parts.shellListings.TryTake (CassoExplorerBrowser::s_kPreviewKey + path, path, items, hr))
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = CassoExplorerBrowser::s_kWorkingText;
        return;
    }

    if (FAILED (hr))
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = PathResolver::IsNetworkPath (path) ? path.substr (path.find_last_of (L"\\/") + 1) + L" is unavailable." : DescribeEmptyArchive (path);
        m_preview.message = m_preview.message.empty() ? L"The archive could not be read" : m_preview.message;
        return;
    }

    if (items.empty() && !DescribeEmptyArchive (path).empty())
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = DescribeEmptyArchive (path);
        return;
    }

    m_preview.kind    = PreviewContent::Kind::Catalog;
    m_preview.archive = true;

    for (const IShellItemVerbs::ShellFolderItem & item : items)
    {
        CatalogRow  row;

        row.name         = item.name;
        row.typeText     = item.typeText;
        row.sizeBytes    = item.sizeBytes;
        row.modifiedUnix = item.modifiedUnix;
        row.hasModified  = item.hasModified;
        row.isDirectory  = item.isFolder;
        row.shellId      = item.id;

        m_preview.rows.push_back (std::move (row));
    }

    std::stable_sort (m_preview.rows.begin(), m_preview.rows.end(), [] (const CatalogRow & a, const CatalogRow & b)
    {
        return (a.isDirectory != b.isDirectory) ? a.isDirectory : StrCmpLogicalW (a.name.c_str(), b.name.c_str()) < 0;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::GetArchivePreviewColumns
//
//  The file list's first columns in their default order: Name, Date
//  modified, Type and Size.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Column> BrowserPreviewer::GetArchivePreviewColumns()
{
    std::vector<DxuiListView::Column>  columns = BrowserCells::GetColumns();



    columns.resize ((std::min) (columns.size(), s_kArchivePreviewColumns));

    //  Fitted to what the archive holds, so the narrow pane shows them all.
    for (DxuiListView::Column & column : columns)
    {
        column.widthDip = 0;
        column.stretch  = false;
    }

    return columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::GetArchivePreviewCells
//
//  A row as the file list draws it inside the archive, icon and all.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> BrowserPreviewer::GetArchivePreviewCells (const CatalogRow & row) const
{
    std::vector<DxuiListView::Cell>  cells = BrowserCells::ToCells (row, Location::MakeShellFolder (row.shellId, row.name), m_parts.sources.shellIcons);



    cells.resize ((std::min) (cells.size(), s_kArchivePreviewColumns));

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::ReadTextPreview
//
//  The host file read, for TryPreviewText. One that cannot be read stays the
//  hex preview, which reports it.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::ReadTextPreview (const std::wstring & path, const std::wstring & name)
{
    std::string  content;
    HRESULT      hr      = m_parts.fs.ReadAllText (path, content);



    if (SUCCEEDED (hr))
    {
        HostTextPreview::TryPreviewText (name, std::span<const Byte> ((const Byte *) content.data(), content.size()), m_preview);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::SetPreviewMonitor
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::SetPreviewMonitor (PicturePreview::Monitor monitor, uint32_t customArgb)
{
    HRESULT  hr = S_OK;



    if (monitor == m_previewMonitor && customArgb == m_customArgb)
    {
        return;
    }

    m_previewMonitor = monitor;
    m_customArgb     = customArgb;

    if (m_preview.kind == PreviewContent::Kind::Picture && m_preview.pictureMode != PicturePreview::Mode::None)
    {
        hr = PicturePreview::Render (m_preview.pictureMode, m_preview.bytes, m_preview.origin, m_bus,
                                     m_preview.bgra, m_preview.width, m_preview.height, GetPictureMonitor(), m_customArgb);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::GetPictureMonitor
//
////////////////////////////////////////////////////////////////////////////////

PicturePreview::Monitor BrowserPreviewer::GetPictureMonitor() const
{
    return CanTogglePictureColor() ? ChooseMonitor (m_previewMonitor, m_preview.pictureMode) : m_previewMonitor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::ChooseMonitor
//
////////////////////////////////////////////////////////////////////////////////

PicturePreview::Monitor BrowserPreviewer::ChooseMonitor (PicturePreview::Monitor chosen, PicturePreview::Mode mode)
{
    PicturePreview::Monitor  monitor = chosen;



    if (mode == PicturePreview::Mode::DoubleHiRes || mode == PicturePreview::Mode::DoubleHiResWindowed || mode == PicturePreview::Mode::DoubleHiResLatched)
    {
        monitor = PicturePreview::Monitor::Color;
    }
    else if (mode == PicturePreview::Mode::DoubleHiResMono && chosen == PicturePreview::Monitor::Color)
    {
        monitor = PicturePreview::Monitor::White;
    }

    return monitor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::CanTogglePictureColor
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::CanTogglePictureColor() const
{
    return m_preview.kind == PreviewContent::Kind::Picture && PicturePreview::IsDoubleHiRes (m_preview.pictureChosen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::TogglePictureColor
//
//  Lasts for this picture: the next picture's mode is chosen afresh.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::TogglePictureColor()
{
    HRESULT  hr = S_OK;



    if (!CanTogglePictureColor())
    {
        return false;
    }

    m_preview.pictureMode = (m_preview.pictureMode != PicturePreview::Mode::DoubleHiResMono) ? PicturePreview::Mode::DoubleHiResMono
                                                                                             : PicturePreview::Mode::DoubleHiRes;

    hr = PicturePreview::Render (m_preview.pictureMode, m_preview.bytes, m_preview.origin, m_bus,
                                 m_preview.bgra, m_preview.width, m_preview.height, GetPictureMonitor(), m_customArgb);

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewShellItem
//
//  A file the shell lists shows its bytes: one on a disk from the file, as in
//  its own folder, and one with no file of its own, such as one inside a zip,
//  read through the shell up to a limit.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::PreviewShellItem (const CatalogRow & row)
{
    HRESULT  hr = S_OK;



    if (row.isDirectory)
    {
        return;
    }

    //  A disk image previews as its catalog, as it does in its folder, one in
    //  a zip by its path through the zip.
    if (row.isDiskImage)
    {
        PreviewImageCatalog (BrowserLoader::GetImagePath (row));
        return;
    }

    if (!row.hostPath.empty())
    {
        m_preview.kind     = PreviewContent::Kind::Hex;
        m_preview.hostPath = row.hostPath;
        return;
    }

    if (m_parts.sources.shellVerbs == nullptr || row.shellId.empty())
    {
        return;
    }

    hr = m_parts.sources.shellVerbs->ReadItemBytes (row.shellId, s_kShellPreviewMaxBytes, m_preview.bytes);

    if (FAILED (hr))
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = L"The file could not be read";
        return;
    }

    m_preview.kind        = PreviewContent::Kind::Hex;
    m_preview.hostCharset = HostTextPreview::GetHostCharset (std::span<const Byte> (m_preview.bytes.data(), (std::min) (m_preview.bytes.size(), HostTextPreview::kTextSampleBytes)));
    m_preview.textFile    = m_preview.hostCharset != HostCharset::None && m_preview.hostCharset != HostCharset::Utf8;

    HostTextPreview::TryPreviewText (row.name, m_preview.bytes, m_preview);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewEntry
//
//  One file in an image, read and rendered: its contents, the details of a
//  file that cannot be read, or why not.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::PreviewEntry (const std::wstring & imagePath, const std::string & entryPath, const FileEntry & entry, VolumeKind kind, std::span<const Byte> diskSectors, std::span<const std::vector<Byte>> otherDisks)
{
    DiskOperations::Result  result;
    FilePayload             payload;
    HRESULT                 hr          = S_OK;
    PreviewDecoder::HexForm hexForm     = PreviewDecoder::HexForm::Auto;
    DecodeOptions           options     = m_viewState.GetDecodeOptions (imagePath, entryPath);



    result = m_parts.operations.Read (TextEncoding::WideToNarrow (imagePath), entryPath, payload, entry.catalogIndex);

    if (!result.Succeeded() && PreviewDecoder::ParseDetails (result.message, m_preview.details))
    {
        m_preview.kind = PreviewContent::Kind::Details;
        return;
    }

    if (!result.Succeeded())
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = DiskMessageText::FormatImageError (imagePath, result.message);
        return;
    }

    hexForm = m_viewState.GetHexForm (imagePath + L"|" + TextEncoding::NarrowToWide (entryPath));
    options.diskSectors = diskSectors;
    options.otherDisks  = otherDisks;
    hr                  = PreviewViewChoices::Render (m_viewState.GetViewAs(), entry, kind, payload, hexForm, m_bus, m_preview, m_previewMonitor, m_customArgb, options);

    //  A double hi-res picture's monitor follows its form, which is known once
    //  it is decoded.
    if (SUCCEEDED (hr) && GetPictureMonitor() != m_previewMonitor)
    {
        hr = PicturePreview::Render (m_preview.pictureMode, m_preview.bytes, m_preview.origin, m_bus,
                                     m_preview.bgra, m_preview.width, m_preview.height, GetPictureMonitor(), m_customArgb);
    }

    if (FAILED (hr) && m_preview.message.empty())
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = L"This file could not be previewed.";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewSearchMatch
//
//  A search's match inside a disk image, found again in its image's catalog
//  by where it was listed.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::PreviewSearchMatch (const CatalogRow & row)
{
    BrowserSearchMatch  match;
    std::wstring        error;



    if (!TryFindSearchMatch (row, match, error))
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = error;
        return;
    }

    if (match.entry.isDirectory)
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = L"A folder. Open it to see what it contains.";
        return;
    }

    PreviewEntry (match.imagePath, match.directory.empty() ? match.entry.name : match.directory + "/" + match.entry.name, match.entry, match.kind);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::TryFindSearchMatch
//
//  A search's match inside a disk image, found again in its image's catalog
//  by where it was listed. False, with why, when the image cannot be read or
//  no longer holds it.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::TryFindSearchMatch (const CatalogRow & row, BrowserSearchMatch & outMatch, std::wstring & outError)
{
    DiskOperations::Result  result;
    VolumeListing           listing;
    VolumeKind              kind  = VolumeKind::Unknown;
    bool                    found = false;



    if (row.imagePath.empty())
    {
        return false;
    }

    //  The volume's own catalog is cached; a directory inside it is read.
    if (!row.imageDirectory.empty() || !m_parts.model.TryGetCachedCatalog (row.imagePath, listing, kind))
    {
        result = m_parts.operations.List (TextEncoding::WideToNarrow (row.imagePath), row.imageDirectory, listing, kind);

        if (!result.Succeeded())
        {
            outError = DiskMessageText::FormatImageError (row.imagePath, result.message);
            return false;
        }

        if (row.imageDirectory.empty())
        {
            m_parts.model.CacheCatalog (row.imagePath, listing, kind);
        }
    }

    for (const FileEntry & entry : listing.entries)
    {
        if (!found && entry.catalogIndex == row.imageCatalogIndex)
        {
            outMatch.imagePath = row.imagePath;
            outMatch.kind      = kind;
            outMatch.directory = row.imageDirectory;
            outMatch.entry     = entry;
            found              = true;
        }
    }

    if (!found)
    {
        outError = L"This file is no longer in its disk image.";
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::GetSelectedSearchMatches
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::GetSelectedSearchMatches (std::vector<BrowserSearchMatch> & outMatches)
{
    BrowserSearchMatch  match;
    std::wstring        error;



    outMatches.clear();

    for (int row : m_parts.list.selectedRows)
    {
        if (row >= 0 && (size_t) row < m_parts.list.rows.size() && TryFindSearchMatch (m_parts.list.rows[(size_t) row], match, error))
        {
            outMatches.push_back (match);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::AreSelectedRowsSearchMatches
//
//  Whether a selection is files a search found inside disk images, which
//  copy from their images.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::AreSelectedRowsSearchMatches() const
{
    bool  any = false;



    for (int row : m_parts.list.selectedRows)
    {
        any = any || (row >= 0 && (size_t) row < m_parts.list.rows.size() && !m_parts.list.rows[(size_t) row].imagePath.empty());
    }

    return any;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::TryPreviewAppleSingle
//
//  A host file that is an AppleSingle container previews as the file it
//  holds. Only a file small enough to be one is read to find out; anything
//  larger shows its bytes as before.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::TryPreviewAppleSingle (const std::wstring & folder, const CatalogRow & row)
{
    static constexpr uint64_t  kLargestContainer = 16 * 1024 * 1024;
    std::string                content;
    AppleSingleFile            file;
    std::string                error;
    std::span<const Byte>      bytes;
    HRESULT                    hr                = S_OK;



    if (row.sizeBytes > kLargestContainer)
    {
        return false;
    }

    hr = m_parts.fs.ReadAllText (BrowserRowPaths::JoinPath (folder, row.name), content);

    if (FAILED (hr))
    {
        return false;
    }

    bytes = std::span<const Byte> ((const Byte *) content.data(), content.size());

    if (!AppleSingleCodec::IsAppleSingle (bytes))
    {
        return false;
    }

    hr = AppleSingleCodec::Decode (bytes, file, error);

    if (FAILED (hr))
    {
        return false;
    }

    PreviewDecoder::RenderAppleSingle (file, m_preview);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewHostFile
//
//  A file exported from an Apple disk with its type in its name previews as
//  it does in its disk. An AppleSingle file previews as its details. Any
//  other file is displayed as its bytes, which the window reads from the
//  file as it draws them, through a Windows preview handler, or as text. A
//  file read whole for View as has the other views of its bytes as well.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::PreviewHostFile (const std::wstring & folder, const CatalogRow & row)
{
    std::wstring             path    = BrowserRowPaths::JoinPath (folder, row.name);
    FilePayload              payload;
    FileEntry                entry;
    bool                     typed   = false;
    bool                     loaded  = false;
    PreviewDecoder::HexForm  hexForm = PreviewDecoder::HexForm::Auto;
    HRESULT                  hr      = S_OK;



    if (row.isDirectory)
    {
        return;
    }

    //  A Full Send's manifest is Casso's record of the folder, not a file
    //  to read here.
    if (_wcsicmp (row.name.c_str(), TextEncoding::NarrowToWide (FullSendManifest::kFileName).c_str()) == 0)
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = L"Casso's record of the disks a Full Send received in this folder. Don't edit, rename or move it: Casso reads it to pass over disks already received.";
        return;
    }

    loaded  = TryReadHostPayload (path, row, payload, entry, typed);
    hexForm = m_viewState.GetHexForm (path);

    if (TryPreviewAppleSingle (folder, row))
    {
        AttachHostViews (entry, payload, loaded, hexForm);
        return;
    }

    if (typed)
    {
        hr = PreviewViewChoices::Render (m_viewState.GetViewAs(), entry, VolumeKind::ProDos, payload, hexForm, m_bus, m_preview, m_previewMonitor, m_customArgb, DecodeOptions());

        if (SUCCEEDED (hr))
        {
            return;
        }

        m_preview = PreviewContent();
    }

    m_preview.kind     = PreviewContent::Kind::Hex;
    m_preview.hostPath = path;

    //  A Windows preview handler, as in Explorer, for a file type with one;
    //  an assembler's source and an exported Apple file are Casso Explorer's
    //  to preview.
    if (m_parts.sources.shellVerbs != nullptr && !HostTextPreview::IsSourceName (row.name) && !HostTextPreview::IsAppleExportName (row.name) && m_parts.sources.shellVerbs->HasShellPreview (m_preview.hostPath))
    {
        m_preview.kind = PreviewContent::Kind::Shell;
    }
    else if (row.sizeBytes <= s_kTextPreviewMaxBytes && !PathResolver::IsNetworkPath (m_preview.hostPath))
    {
        //  A source file, or text the hex view cannot draw, is read whole as
        //  text when it is small and on this computer.
        ReadTextPreview (m_preview.hostPath, row.name);
    }

    AttachHostViews (entry, payload, loaded, hexForm);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::TryReadHostPayload
//
//  A local file up to s_kTextPreviewMaxBytes, read whole: an AppleSingle
//  file's data and resource fork, or the file's bytes. `outTyped` is set for
//  a name with a ProDOS type and aux type in it, which the entry and payload
//  take.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::TryReadHostPayload (const std::wstring & path, const CatalogRow & row, FilePayload & outPayload, FileEntry & outEntry, bool & outTyped)
{
    std::string            content;
    std::span<const Byte>  bytes;
    AppleSingleFile        single;
    std::string            error;
    ParsedHostName         parsed;
    HRESULT                hr      = S_OK;
    bool                   decoded = false;



    outPayload = FilePayload();
    outEntry   = FileEntry();
    outTyped   = false;

    if (row.sizeBytes > s_kTextPreviewMaxBytes || PathResolver::IsNetworkPath (path))
    {
        return false;
    }

    hr = m_parts.fs.ReadAllText (path, content);

    if (FAILED (hr))
    {
        return false;
    }

    bytes   = std::span<const Byte> ((const Byte *) content.data(), content.size());
    hr      = AppleSingleCodec::IsAppleSingle (bytes) ? AppleSingleCodec::Decode (bytes, single, error) : E_FAIL;
    decoded = SUCCEEDED (hr);

    if (decoded)
    {
        outPayload.bytes        = std::move (single.data);
        outPayload.resourceFork = std::move (single.resourceFork);
        return true;
    }

    outPayload.bytes.assign (bytes.begin(), bytes.end());
    outEntry.name     = TextEncoding::WideToNarrow (row.name);
    outEntry.eofBytes = (uint32_t) outPayload.bytes.size();

    if (HostFileNaming::Parse (row.name, parsed) && parsed.hasType && parsed.converted == ParsedHostName::ConvertedKind::None)
    {
        outEntry.name           = parsed.catalogName;
        outEntry.type           = parsed.type;
        outEntry.auxType        = parsed.aux;
        outEntry.hasAuxType     = parsed.hasAux;
        outEntry.loadAddress    = parsed.aux;
        outEntry.hasLoadAddress = parsed.hasAux;
        outPayload.type           = parsed.type;
        outPayload.auxType        = parsed.aux;
        outPayload.hasAuxType     = parsed.hasAux;
        outPayload.loadAddress    = parsed.aux;
        outPayload.hasLoadAddress = parsed.hasAux;
        outTyped                  = true;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::AttachHostViews
//
//  The other views of a host file read whole, and the chosen one in place
//  of the default when it applies.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::AttachHostViews (const FileEntry & entry, const FilePayload & payload, bool loaded, PreviewDecoder::HexForm hexForm)
{
    std::vector<ViewChoice>  choices;
    std::wstring             defaultName;
    PreviewContent           chosen;
    HRESULT                  hr          = E_FAIL;
    bool                     applies     = false;



    if (!loaded)
    {
        return;
    }

    PreviewViewChoices::List (payload, m_preview, choices);
    defaultName = PreviewViewChoices::DescribeDefault (m_preview);
    applies     = std::find (choices.begin(), choices.end(), m_viewState.GetViewAs()) != choices.end();

    if (applies)
    {
        hr = PreviewViewChoices::RenderAs (m_viewState.GetViewAs(), entry, VolumeKind::ProDos, payload, hexForm, m_bus, chosen, m_previewMonitor, m_customArgb);
    }

    if (SUCCEEDED (hr))
    {
        m_preview = std::move (chosen);
    }

    m_preview.viewChoices = std::move (choices);
    m_preview.viewAs      = SUCCEEDED (hr) ? m_viewState.GetViewAs() : ViewChoice::Default;
    m_preview.defaultView = std::move (defaultName);
}

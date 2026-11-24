#include "Pch.h"

#include "CassoExplorer/Browser/BrowserPreviewer.h"
#include "CassoExplorer/Browser/BrowserHostInterfaces.h"
#include "CassoExplorer/Browser/BrowserSearchMatches.h"
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
    BrowserSearchMatches    & searchMatches,
    const BrowserArchives   & archives,
    IBrowserPreviewHost     & host)
    : m_parts         (parts),
      m_selection     (selection),
      m_host          (host),
      m_searchMatches (searchMatches),
      m_archives      (archives)
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

        hr = PreviewViewChoices::Render (m_viewState.GetViewAs(), entry, m_parts.list.kind, payload, hexForm, m_bus, m_preview, m_monitor.GetPreviewMonitor(), m_monitor.GetCustomArgb(),
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
            m_diskImages.PreviewEntry (location.path, BrowserRowPaths::GetEntryPath (GetLocation(), *entry), *entry, m_parts.list.kind);
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
        m_shellItems.PreviewShellItem (m_parts.list.rows[m_parts.list.selectedRows[0]]);
        return;
    }
    else if (m_parts.list.isPartitionList && m_parts.list.selectedRows.size() == 1 && !m_parts.list.rows[m_parts.list.selectedRows[0]].containerPath.empty())
    {
        //  A partition of a card or a card image previews as its volume's catalog.
        m_diskImages.PreviewImageCatalog (m_parts.list.rows[m_parts.list.selectedRows[0]].containerPath);
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
        m_shellItems.PreviewArchive (BrowserRowPaths::JoinPath (folder, m_parts.list.rows[m_parts.list.selectedRows[0]].name));
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

    m_diskImages.PreviewImageCatalog (imagePath);
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



    if (!m_searchMatches.TryFindSearchMatch (row, match, error))
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

    m_diskImages.PreviewEntry (match.imagePath, match.directory.empty() ? match.entry.name : match.directory + "/" + match.entry.name, match.entry, match.kind);
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
        hr = PreviewViewChoices::Render (m_viewState.GetViewAs(), entry, VolumeKind::ProDos, payload, hexForm, m_bus, m_preview, m_monitor.GetPreviewMonitor(), m_monitor.GetCustomArgb(), DecodeOptions());

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
        hr = PreviewViewChoices::RenderAs (m_viewState.GetViewAs(), entry, VolumeKind::ProDos, payload, hexForm, m_bus, chosen, m_monitor.GetPreviewMonitor(), m_monitor.GetCustomArgb());
    }

    if (SUCCEEDED (hr))
    {
        m_preview = std::move (chosen);
    }

    m_preview.viewChoices = std::move (choices);
    m_preview.viewAs      = SUCCEEDED (hr) ? m_viewState.GetViewAs() : ViewChoice::Default;
    m_preview.defaultView = std::move (defaultName);
}

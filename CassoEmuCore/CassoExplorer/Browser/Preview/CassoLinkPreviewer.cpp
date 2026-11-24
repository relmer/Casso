#include "Pch.h"

#include "CassoExplorer/Browser/Preview/CassoLinkPreviewer.h"
#include "CassoExplorer/Browser/BrowserLoader.h"
#include "CassoExplorer/Browser/Preview/PictureMonitorChoice.h"
#include "CassoExplorer/Browser/Preview/PreviewViewState.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/DiskMessageText.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/PreviewViewChoices.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPreviewer::CassoLinkPreviewer
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkPreviewer::CassoLinkPreviewer (const BrowserParts & parts, const PreviewParts & previewParts)
    : m_parts        (parts),
      m_previewParts (previewParts)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPreviewer::PreviewAppleDrive
//
//  False for a row that is not a linked Apple's drive.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkPreviewer::PreviewAppleDrive (const CatalogRow & row)
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

    m_previewParts.preview.kind = PreviewContent::Kind::Error;

    if (m_parts.sources.links->IsWritingDisk (TextEncoding::WideToNarrow (link), drive))
    {
        m_previewParts.preview.message = L"The disk is not available because an image is being written to it.";
    }
    else if (!m_parts.sources.links->TryGetListing (TextEncoding::WideToNarrow (link), drive, std::string(), listing, kind, result, message))
    {
        m_previewParts.preview.message = CassoExplorerBrowser::s_kWorkingText;
    }
    else if (FAILED (result))
    {
        m_previewParts.preview.message = DiskMessageText::FormatLinkError (result, message);
    }
    else
    {
        m_previewParts.preview = PreviewContent();
        PreviewDecoder::RenderCatalog (listing, kind, m_previewParts.preview);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPreviewer::PreviewCassoLinkItem
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkPreviewer::PreviewCassoLinkItem (const CatalogRow & row)
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



    m_previewParts.preview      = PreviewContent();
    m_previewParts.preview.kind = PreviewContent::Kind::Text;

    //  The help is written out for the Windows Markdown preview, where one
    //  is installed; otherwise the preview has the text as it is.
    if (help && m_parts.sources.shellVerbs != nullptr && !row.hostPath.empty())
    {
        hr = m_parts.operations.GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (row.hostPath), std::vector<Byte> (bytes.begin(), bytes.end()));

        if (SUCCEEDED (hr) && m_parts.sources.shellVerbs->HasShellPreview (row.hostPath))
        {
            m_previewParts.preview.kind     = PreviewContent::Kind::Shell;
            m_previewParts.preview.hostPath = row.hostPath;
            return;
        }
    }

    while (start < text.size())
    {
        end = text.find (L"\r\n", start);
        end = (end == std::wstring::npos) ? text.size() : end;
        m_previewParts.preview.lines.push_back (text.substr (start, end - start));
        start = end + 2;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPreviewer::PreviewAppleEntry
//
//  A file on a linked Apple's drive, read over the link and then previewed
//  as a file in an image is.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkPreviewer::PreviewAppleEntry (const std::string & entryPath, const FileEntry & entry)
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
        m_previewParts.preview.kind    = PreviewContent::Kind::Error;
        m_previewParts.preview.message = CassoExplorerBrowser::s_kWorkingText;
        return;
    }

    if (SUCCEEDED (hr))
    {
        PreviewDecoder::HexForm  hexForm = m_previewParts.viewState.GetHexForm (GetLocation().path + L"|" + TextEncoding::NarrowToWide (entryPath));

        hr = PreviewViewChoices::Render (m_previewParts.viewState.GetViewAs(), entry, m_parts.list.kind, payload, hexForm, m_previewParts.bus, m_previewParts.preview, m_previewParts.monitor.GetPreviewMonitor(), m_previewParts.monitor.GetCustomArgb(),
                                         m_previewParts.viewState.GetDecodeOptions (GetLocation().path, entryPath));
    }

    if (FAILED (hr) && m_previewParts.preview.message.empty())
    {
        m_previewParts.preview.kind    = PreviewContent::Kind::Error;
        m_previewParts.preview.message = message.empty() ? L"This file could not be previewed." : TextEncoding::NarrowToWide (message);
    }
}

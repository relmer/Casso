#include "Pch.h"

#include "CassoExplorer/Browser/BrowserPreviewer.h"
#include "Core/AppleSingleCodec.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/ArchiveProbe.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::BrowserPreviewer
//
////////////////////////////////////////////////////////////////////////////////

BrowserPreviewer::BrowserPreviewer (CassoExplorerBrowser & browser)
    : m_browser (browser)
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



    if (m_browser.m_links == nullptr || !TreeModel::TryParseLinkId (row.hostPath, link, slot, drive) || drive == 0)
    {
        return false;
    }

    m_preview.kind = PreviewContent::Kind::Error;

    if (m_browser.m_links->IsWritingDisk (TextEncoding::WideToNarrow (link), drive))
    {
        m_preview.message = L"The disk is not available because an image is being written to it.";
    }
    else if (!m_browser.m_links->TryGetListing (TextEncoding::WideToNarrow (link), drive, std::string(), listing, kind, result, message))
    {
        m_preview.message = CassoExplorerBrowser::s_kWorkingText;
    }
    else if (FAILED (result))
    {
        m_preview.message = BrowserCells::FormatLinkError (result, message);
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
    if (help && m_browser.m_shellVerbs != nullptr && !row.hostPath.empty())
    {
        hr = m_browser.m_operations.GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (row.hostPath), std::vector<Byte> (bytes.begin(), bytes.end()));

        if (SUCCEEDED (hr) && m_browser.m_shellVerbs->HasShellPreview (row.hostPath))
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
    bool          disassemble = m_browser.m_model.HasTabs() && m_browser.m_model.GetActiveTab().disassemble;



    TreeModel::TryParseLinkId (m_browser.GetLocation().path, link, slot, drive);

    if (!m_browser.m_links->TryGetFile (TextEncoding::WideToNarrow (link), drive, entryPath, payload, hr, message))
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = CassoExplorerBrowser::s_kWorkingText;
        return;
    }

    if (SUCCEEDED (hr))
    {
        hr = PreviewDecoder::Render (entry, m_browser.m_kind, payload, disassemble, m_bus, m_preview, m_previewMonitor, m_customArgb);
    }

    if (FAILED (hr) && m_preview.message.empty())
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = message.empty() ? L"This file could not be previewed." : TextEncoding::NarrowToWide (message);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::SetDisassemble
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::SetDisassemble (bool disassemble)
{
    if (!m_browser.m_model.HasTabs())
    {
        return;
    }

    m_browser.m_model.GetActiveTab().disassemble = disassemble;
    UpdatePreview();
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
    Location                  location  = m_browser.GetLocation();
    DiskOperations::Result    result;
    VolumeListing             listing;
    VolumeKind                kind      = VolumeKind::Unknown;
    std::wstring              imagePath;
    std::wstring              folder    = location.path;



    m_preview = PreviewContent();
    m_preview.kind = PreviewContent::Kind::Text;

    if (m_browser.TryGetSelectedEntry (entry))
    {
        if (entry->isDirectory)
        {
            m_preview.kind    = PreviewContent::Kind::Error;
            m_preview.message = L"A folder. Open it to see what it contains.";
            return;
        }

        if (m_browser.m_isAppleDrive)
        {
            PreviewAppleEntry (m_browser.GetEntryPath (*entry), *entry);
        }
        else
        {
            PreviewEntry (location.path, m_browser.GetEntryPath (*entry), *entry, m_browser.m_kind);
        }

        return;
    }

    //  CassoLink's own items, and a linked Apple's drive, which previews as
    //  its catalog once it is read.
    if (location.kind == Location::Kind::Root && m_browser.m_selectedRows.size() == 1 && m_browser.m_selectedRows[0] >= 0 && (size_t) m_browser.m_selectedRows[0] < m_browser.m_rows.size())
    {
        const CatalogRow &  row = m_browser.m_rows[(size_t) m_browser.m_selectedRows[0]];

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
    if (m_browser.m_selectedRows.size() == 1 && m_browser.m_selectedRows[0] >= 0 && (size_t) m_browser.m_selectedRows[0] < m_browser.m_rows.size() && !m_browser.m_rows[(size_t) m_browser.m_selectedRows[0]].imagePath.empty())
    {
        PreviewSearchMatch (m_browser.m_rows[(size_t) m_browser.m_selectedRows[0]]);
        return;
    }

    //  A search's result on the disk previews as it does in its own folder.
    if (!m_browser.m_isImage && m_browser.m_selectedRows.size() == 1 && SearchQuery::IsId (location.path) && !m_browser.m_rows[m_browser.m_selectedRows[0]].folderPath.empty())
    {
        folder = m_browser.m_rows[m_browser.m_selectedRows[0]].folderPath;
    }
    else if (!m_browser.m_isImage && m_browser.IsHomeLocation() && m_browser.m_selectedRows.size() == 1 && m_browser.m_rows[m_browser.m_selectedRows[0]].hostPath.empty())
    {
        //  A cloud item has no file here, and its stream is a web page.
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = L"No preview available.";
        return;
    }
    else if (!m_browser.m_isImage && (location.kind == Location::Kind::ShellFolder || m_browser.IsHomeLocation()) && m_browser.m_selectedRows.size() == 1)
    {
        PreviewShellItem (m_browser.m_rows[m_browser.m_selectedRows[0]]);
        return;
    }
    else if (m_browser.m_isImage || location.kind != Location::Kind::HostFolder || m_browser.m_selectedRows.size() != 1)
    {
        return;
    }

    //  An archive previews as what it holds, as a disk image does.
    if (m_browser.m_rows[m_browser.m_selectedRows[0]].isArchive)
    {
        PreviewArchive (CassoExplorerBrowser::JoinPath (folder, m_browser.m_rows[m_browser.m_selectedRows[0]].name));
        return;
    }

    //  Any other file in a host folder shows its bytes, which the window reads
    //  from the file as it draws them rather than loading them here.
    if (!m_browser.m_rows[m_browser.m_selectedRows[0]].isDiskImage)
    {
        if (!m_browser.m_rows[m_browser.m_selectedRows[0]].isDirectory && TryPreviewAppleSingle (folder, m_browser.m_rows[m_browser.m_selectedRows[0]]))
        {
            return;
        }

        if (!m_browser.m_rows[m_browser.m_selectedRows[0]].isDirectory)
        {
            m_preview.kind     = PreviewContent::Kind::Hex;
            m_preview.hostPath = folder;

            if (!m_preview.hostPath.empty() && m_preview.hostPath.back() != L'\\')
            {
                m_preview.hostPath += L'\\';
            }

            m_preview.hostPath += m_browser.m_rows[m_browser.m_selectedRows[0]].name;

            //  Windows' own preview, as Explorer's, for a file of a type it
            //  previews; an assembler's source and an exported Apple file are
            //  Casso Explorer's to preview.
            if (m_browser.m_shellVerbs != nullptr && !IsSourceName (m_browser.m_rows[m_browser.m_selectedRows[0]].name) && !IsAppleExportName (m_browser.m_rows[m_browser.m_selectedRows[0]].name)
                && m_browser.m_shellVerbs->HasShellPreview (m_preview.hostPath))
            {
                m_preview.kind = PreviewContent::Kind::Shell;
                return;
            }

            //  A source file, or text the hex view cannot draw, is read whole
            //  as text when it is small and on this computer.
            if (m_browser.m_rows[m_browser.m_selectedRows[0]].sizeBytes <= s_kTextPreviewMaxBytes && !PathResolver::IsNetworkPath (m_preview.hostPath))
            {
                ReadTextPreview (m_preview.hostPath, m_browser.m_rows[m_browser.m_selectedRows[0]].name);
            }
        }

        return;
    }

    imagePath = folder;

    if (!imagePath.empty() && imagePath.back() != L'\\')
    {
        imagePath += L'\\';
    }

    imagePath += m_browser.m_rows[m_browser.m_selectedRows[0]].name;

    if (!m_browser.m_model.TryGetCachedCatalog (imagePath, listing, kind))
    {
        result = m_browser.m_operations.List (TextEncoding::WideToNarrow (imagePath), listing, kind);

        if (!result.Succeeded() && PreviewDecoder::ParseDetails (result.message, m_preview.details))
        {
            m_preview.kind = PreviewContent::Kind::Details;
            return;
        }

        if (!result.Succeeded())
        {
            m_preview.kind    = PreviewContent::Kind::Error;
            m_preview.message = BrowserCells::FormatImageError (imagePath, result.message);
            return;
        }

        m_browser.m_model.CacheCatalog (imagePath, listing, kind);
    }

    PreviewDecoder::RenderCatalog (listing, kind, m_preview);
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
    return m_browser.m_shellVerbs != nullptr && id.size() > 3 && id[1] == L':' && id[2] == L'\\'
        && !m_browser.m_fs.DirectoryExists (id);
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

    hr = m_browser.m_fs.ReadAllText (path, bytes);

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
    const Location           & location = m_browser.GetLocation();



    if (location.kind == Location::Kind::HostFolder && m_browser.m_selectedRows.size() == 1
        && m_browser.m_rows[(size_t) m_browser.m_selectedRows[0]].isArchive)
    {
        m_browser.GetSelectedHostPaths (paths);
        return paths.empty() ? std::wstring() : paths[0];
    }

    if (location.kind != Location::Kind::ShellFolder || !IsLocalArchivePath (location.path))
    {
        return std::wstring();
    }

    for (size_t at = location.path.find (L'\\', 3); ; at = location.path.find (L'\\', at + 1))
    {
        prefix = location.path.substr (0, at);

        if (m_browser.m_fs.Exists (prefix) && !m_browser.m_fs.DirectoryExists (prefix))
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



    if (m_browser.m_shellVerbs == nullptr)
    {
        return;
    }

    //  One on a network share is read on a thread of its own, with a working
    //  message meanwhile, so an unreachable share never blocks the window.
    if (!PathResolver::IsNetworkPath (path))
    {
        hr = m_browser.m_shellVerbs->ListShellFolder (path, items);
    }
    else if (!m_browser.m_shellListings.TryTake (CassoExplorerBrowser::s_kPreviewKey + path, path, items, hr))
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
    std::vector<DxuiListView::Cell>  cells = BrowserCells::ToCells (row, Location::MakeShellFolder (row.shellId, row.name), m_browser.m_shellIcons);



    cells.resize ((std::min) (cells.size(), s_kArchivePreviewColumns));

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::GetHostCharset
//
//  A host file is text, and opens showing its characters rather than its
//  bytes, when it has no NUL and few other control codes besides tabs, line
//  breaks, form feeds and DOS's end-of-file mark. Code page 437 has symbols
//  for the rest, such as a pointer for 0x10, so a file with any is DOS text. Bytes
//  above 0x7F that are well-formed UTF-8 are UTF-8 text; otherwise they are
//  DOS text when most of them are DOS line-drawing characters, and Windows
//  text when not.
//
////////////////////////////////////////////////////////////////////////////////

HostCharset BrowserPreviewer::GetHostCharset (std::span<const Byte> sample)
{
    size_t  high     = 0;
    size_t  drawing  = 0;
    size_t  controls = 0;



    if (sample.empty())
    {
        return HostCharset::None;
    }

    for (Byte value : sample)
    {
        if (value == 0)
        {
            return HostCharset::None;
        }

        controls += ((value < 0x20 && value != '\t' && value != '\r' && value != '\n'
                      && value != s_kFormFeed && value != s_kDosEndOfFile) || value == 0x7F) ? 1 : 0;

        if (value >= 0x80)
        {
            high++;
            drawing += (value >= s_kDosDrawingFirst && value <= s_kDosDrawingLast) ? 1 : 0;
        }
    }

    if (controls * s_kTextControlRatio > sample.size())
    {
        return HostCharset::None;
    }

    if (high == 0 && controls == 0)
    {
        return HostCharset::Ascii;
    }

    if (high > 0 && IsUtf8 (sample))
    {
        return HostCharset::Utf8;
    }

    return (controls > 0 || drawing * 2 >= high) ? HostCharset::Dos : HostCharset::Windows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::IsUtf8
//
//  Whether every byte above 0x7F is part of a well-formed UTF-8 sequence. A
//  sequence cut off by the end of the sample counts as well formed.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::IsUtf8 (std::span<const Byte> sample)
{
    size_t  at = 0;



    while (at < sample.size())
    {
        Byte    lead  = sample[at];
        size_t  extra = (lead < 0x80)                  ? 0
                      : (lead >= 0xC2 && lead <= 0xDF) ? 1
                      : (lead >= 0xE0 && lead <= 0xEF) ? 2
                      : (lead >= 0xF0 && lead <= 0xF4) ? 3
                      :                                  SIZE_MAX;

        if (extra == SIZE_MAX)
        {
            return false;
        }

        for (size_t next = 1; next <= extra && at + next < sample.size(); next++)
        {
            if ((sample[at + next] & 0xC0) != 0x80)
            {
                return false;
            }
        }

        at += extra + 1;
    }

    return true;
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
    HRESULT      hr      = m_browser.m_fs.ReadAllText (path, content);



    if (SUCCEEDED (hr))
    {
        TryPreviewText (name, std::span<const Byte> ((const Byte *) content.data(), content.size()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::TryPreviewText
//
//  A file's bytes as lines of text: an assembler's source or an Apple file
//  Casso Explorer exported, in whatever character set it is, or any text in
//  UTF-8, or in UTF-16 when a byte order
//  mark leads it. Anything else, binary above all, stays as it was. A byte
//  order mark is dropped, and tabs are expanded to the next stop of eight.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::TryPreviewText (const std::wstring & name, std::span<const Byte> bytes)
{
    HostCharset   charset = GetHostCharset (bytes.subspan (0, (std::min) (bytes.size(), s_kTextSampleBytes)));
    bool          source  = IsSourceName (name);
    bool          apple   = IsAppleExportName (name);
    bool          wideLe  = bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE;
    bool          wideBe  = bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF;
    std::string   narrow ((const char *) bytes.data(), bytes.size());
    std::wstring  text;
    std::wstring  line;
    bool          isText  = ((source || apple) && charset != HostCharset::None) || charset == HostCharset::Utf8 || wideLe || wideBe;



    if (!isText)
    {
        return false;
    }

    if (wideLe || wideBe)
    {
        for (size_t at = 0; at + 1 < bytes.size(); at += 2)
        {
            text.push_back ((wchar_t) (wideLe ? (bytes[at] | (bytes[at + 1] << 8)) : ((bytes[at] << 8) | bytes[at + 1])));
        }
    }
    else
    {
        text = (charset == HostCharset::Utf8) ? TextEncoding::Utf8ToWide (narrow)
             : (charset == HostCharset::Dos)  ? TextEncoding::NarrowToWide (narrow, s_kDosCodePage)
                                              : TextEncoding::NarrowToWide (narrow, s_kWindowsCodePage);
    }

    if (!text.empty() && text[0] == s_kByteOrderMark)
    {
        text.erase (0, 1);
    }

    m_preview.lines.clear();

    for (size_t at = 0; at <= text.size(); at++)
    {
        wchar_t  ch = (at < text.size()) ? text[at] : L'\n';

        if (ch == L'\r' || ch == L'\n')
        {
            m_preview.lines.push_back (std::move (line));
            line.clear();
            at += (ch == L'\r' && at + 1 < text.size() && text[at + 1] == L'\n') ? 1 : 0;
        }
        else if (ch == L'\t')
        {
            line.append (s_kTabStop - line.size() % s_kTabStop, L' ');
        }
        else
        {
            line.push_back (ch);
        }
    }

    //  A last line break ends the last line rather than starting another.
    if (m_preview.lines.size() > 1 && m_preview.lines.back().empty())
    {
        m_preview.lines.pop_back();
    }

    m_preview.kind      = PreviewContent::Kind::Text;
    m_preview.source    = source;
    m_preview.bytes.clear();
    m_preview.hostPath.clear();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::IsAppleExportName
//
//  A BASIC listing or a text file Casso Explorer exported to the host, by the
//  suffix the export appends to its catalog name.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::IsAppleExportName (const std::wstring & name)
{
    ParsedHostName  parsed;



    return HostFileNaming::Parse (name, parsed) && parsed.converted != ParsedHostName::ConvertedKind::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::IsSourceName
//
//  An assembler's source by its extension, as as65, ca65 and Merlin write
//  theirs.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserPreviewer::IsSourceName (const std::wstring & name)
{
    size_t        dot       = name.rfind (L'.');
    std::wstring  extension = (dot == std::wstring::npos) ? std::wstring() : name.substr (dot);



    return std::any_of (std::begin (s_kpszSourceExtensions), std::end (s_kpszSourceExtensions),
                        [&extension] (const wchar_t * known) { return _wcsicmp (known, extension.c_str()) == 0; });
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



    if (mode == PicturePreview::Mode::DoubleHiRes)
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
    return m_preview.kind == PreviewContent::Kind::Picture
        && (m_preview.pictureChosen == PicturePreview::Mode::DoubleHiRes || m_preview.pictureChosen == PicturePreview::Mode::DoubleHiResMono);
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

    m_preview.pictureMode = (m_preview.pictureMode == PicturePreview::Mode::DoubleHiRes) ? PicturePreview::Mode::DoubleHiResMono
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

    if (!row.hostPath.empty())
    {
        m_preview.kind     = PreviewContent::Kind::Hex;
        m_preview.hostPath = row.hostPath;
        return;
    }

    if (m_browser.m_shellVerbs == nullptr || row.shellId.empty())
    {
        return;
    }

    hr = m_browser.m_shellVerbs->ReadItemBytes (row.shellId, s_kShellPreviewMaxBytes, m_preview.bytes);

    if (FAILED (hr))
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = L"The file could not be read";
        return;
    }

    m_preview.kind        = PreviewContent::Kind::Hex;
    m_preview.hostCharset = GetHostCharset (std::span<const Byte> (m_preview.bytes.data(), (std::min) (m_preview.bytes.size(), s_kTextSampleBytes)));
    m_preview.textFile    = m_preview.hostCharset != HostCharset::None && m_preview.hostCharset != HostCharset::Utf8;

    TryPreviewText (row.name, m_preview.bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer::PreviewEntry
//
//  One file in an image, read and rendered: its contents, the details of a
//  file that cannot be read, or why not.
//
////////////////////////////////////////////////////////////////////////////////

void BrowserPreviewer::PreviewEntry (const std::wstring & imagePath, const std::string & entryPath, const FileEntry & entry, VolumeKind kind)
{
    DiskOperations::Result  result;
    FilePayload             payload;
    HRESULT                 hr          = S_OK;
    bool                    disassemble = m_browser.m_model.HasTabs() && m_browser.m_model.GetActiveTab().disassemble;



    result = m_browser.m_operations.Read (TextEncoding::WideToNarrow (imagePath), entryPath, payload, entry.catalogIndex);

    if (!result.Succeeded() && PreviewDecoder::ParseDetails (result.message, m_preview.details))
    {
        m_preview.kind = PreviewContent::Kind::Details;
        return;
    }

    if (!result.Succeeded())
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = BrowserCells::FormatImageError (imagePath, result.message);
        return;
    }

    hr = PreviewDecoder::Render (entry, kind, payload, disassemble, m_bus, m_preview, m_previewMonitor, m_customArgb);

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
    if (!row.imageDirectory.empty() || !m_browser.m_model.TryGetCachedCatalog (row.imagePath, listing, kind))
    {
        result = m_browser.m_operations.List (TextEncoding::WideToNarrow (row.imagePath), row.imageDirectory, listing, kind);

        if (!result.Succeeded())
        {
            outError = BrowserCells::FormatImageError (row.imagePath, result.message);
            return false;
        }

        if (row.imageDirectory.empty())
        {
            m_browser.m_model.CacheCatalog (row.imagePath, listing, kind);
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

    for (int row : m_browser.m_selectedRows)
    {
        if (row >= 0 && (size_t) row < m_browser.m_rows.size() && TryFindSearchMatch (m_browser.m_rows[(size_t) row], match, error))
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



    for (int row : m_browser.m_selectedRows)
    {
        any = any || (row >= 0 && (size_t) row < m_browser.m_rows.size() && !m_browser.m_rows[(size_t) row].imagePath.empty());
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

    hr = m_browser.m_fs.ReadAllText (CassoExplorerBrowser::JoinPath (folder, row.name), content);

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

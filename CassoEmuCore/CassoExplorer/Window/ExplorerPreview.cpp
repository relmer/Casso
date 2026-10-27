#include "Pch.h"

#include "CassoExplorer/Window/ExplorerPreview.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/RunningCassoRegistry.h"
#include "Seams/ILinkAnnouncements.h"
#include "Seams/Win32FirewallHelper.h"
#include "Net/FirewallMonitor.h"
#include "Net/FirewallText.h"
#include "Devices/Disk/DiskImageStore.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/CassoExplorerAbout.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/CassoExplorerLinkDialog.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/CassoExplorerProperties.h"
#include "CassoExplorer/CassoExplorerRawDialog.h"
#include "CassoExplorer/CassoExplorerOptionsDialog.h"
#include "CassoExplorer/Model/CassoTargeting.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "AssetBootstrap.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Config/FileAssociations.h"
#include "Config/GlobalUserPrefs.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Seams/Win32DiskFileIo.h"
#include "Seams/Win32UserClasses.h"
#include "Config/UserConfigStore.h"
#include "Config/Win32FileSystem.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/MachineConfig.h"
#include "Core/MachineScanner.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ExplorerPreview
//
////////////////////////////////////////////////////////////////////////////////

ExplorerPreview::ExplorerPreview (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetHexEncoding
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexView::TextEncoding ExplorerPreview::GetHexEncoding (bool hostFile, HostCharset charset)
{
    switch (charset)
    {
        case HostCharset::Dos:      return DxuiHexView::TextEncoding::Dos;
        case HostCharset::Windows:  return DxuiHexView::TextEncoding::Windows;
        case HostCharset::Ascii:    return DxuiHexView::TextEncoding::Ascii;
        default:                    break;
    }

    return hostFile ? DxuiHexView::TextEncoding::Ascii : DxuiHexView::TextEncoding::AppleHighBit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetPreviewMonitor
//
//  The monitor chosen in Casso Explorer, or green, the phosphor of Casso's
//  own monitors, when none has been.
//
////////////////////////////////////////////////////////////////////////////////

PicturePreview::Monitor ExplorerPreview::GetPreviewMonitor() const
{
    for (int index = 0; index < PicturePreview::kMonitorCount; index++)
    {
        if (m_window.m_prefs.previewMonitor == CassoExplorerPrefs::kMonitorNames[index])
        {
            return (PicturePreview::Monitor) index;
        }
    }

    return PicturePreview::Monitor::Green;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetPreviewCrtDefaults
//
//  The effects of the Casso theme Casso Explorer is in, as Casso extracts it
//  to its own folder; the retro theme's under Light, Dark and Follow Windows.
//  Null, the effects' own defaults, when Casso has not extracted its themes.
//
////////////////////////////////////////////////////////////////////////////////

const ThemeCrtDefaults * ExplorerPreview::GetPreviewCrtDefaults()
{
    bool             casso = ExplorerLayout::IsCassoThemeName (m_window.m_prefs.theme);
    std::string      name  = casso ? m_window.m_prefs.theme : std::string (CassoExplorerPrefs::kThemeRetroTerminal);
    Win32FileSystem  fs;
    ThemeLoadError   error;
    std::wstring     dir;
    HRESULT          hr    = S_OK;



    if (name != m_crtThemeName)
    {
        m_crtThemeName  = name;
        m_crtTheme      = LoadedTheme();
        dir             = (PathResolver::GetLocalAppDataDir (L"Casso") / L"Themes" / TextEncoding::Utf8ToWide (name)).wstring();
        hr              = ThemeLoader::Load (fs, dir, m_crtTheme, error);
        m_crtThemeValid = SUCCEEDED (hr);
    }

    return m_crtThemeValid ? &m_crtTheme.crtDefaults : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::UpdateCrtPicture
//
//  The picture preview drawn through Casso's CRT pass on the picture's monitor,
//  at the view's drawn size, again only when that size changed
//  unless `force`. A preview has a single frame, so persistence, which blends a
//  frame into the next, is left out. The picture's glow spills past its
//  edges, into a black margin drawn around it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::UpdateCrtPicture (bool force)
{
    const PreviewContent   & preview  = m_window.m_browser.GetPreview();
    int                      width    = 0;
    int                      height   = 0;
    int                      margin   = 0;
    CrtResolved              resolved;
    CrtParams                params;
    std::vector<uint32_t>    drawn;
    HRESULT                  hr       = S_OK;
    auto                     found    = m_window.m_prefs.crtOverrides.end();



    if (preview.kind != PreviewContent::Kind::Picture || preview.bgra.empty() || m_window.GetPopupHost() == nullptr)
    {
        return;
    }

    m_window.m_picture->GetScaledSize (width, height);

    if (width <= 0 || height <= 0 || (!force && width == m_crtWidth && height == m_crtHeight))
    {
        return;
    }

    found    = m_window.m_prefs.crtOverrides.find (CassoExplorerPrefs::kMonitorNames[(int) m_window.m_browser.GetPictureMonitor()]);
    resolved = CrtResolver::Resolve (CrtPresets::GetPreset (PicturePreview::GetPresetIndex (m_window.m_browser.GetPictureMonitor())), GetPreviewCrtDefaults(),
                                               (found != m_window.m_prefs.crtOverrides.end()) ? found->second : CrtOverrides());
    params             = MakeCrtParams (resolved, (float) width, (float) height);
    params.persistence = 0.0f;

    hr = m_crtPicture.Render (m_window.GetPopupHost()->GetDevice(), m_window.GetPopupHost()->GetContext(), preview.bgra, preview.width, preview.height,
                              width, height, params, drawn, margin);

    if (SUCCEEDED (hr))
    {
        m_window.m_picture->SetDisplayImage (std::move (drawn), width + 2 * margin, height + 2 * margin, margin);
        m_crtWidth  = width;
        m_crtHeight = height;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::FillPreview
//
//  The preview's kind picks the widget: a picture in the framebuffer view, a
//  catalog as rows under the list's own columns, everything else as lines.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::FillPreview()
{
    const PreviewContent                          & preview  = m_window.m_browser.GetPreview();
    std::vector<std::vector<DxuiListView::Cell>>    rows;
    bool                                            visible  = m_window.m_prefs.previewVisible;
    bool                                            picture  = preview.kind == PreviewContent::Kind::Picture && !preview.bgra.empty();
    bool                                            hostFile = preview.kind == PreviewContent::Kind::Hex && !preview.hostPath.empty();
    bool                                            current  = hostFile && m_fileBytes.GetPath() == preview.hostPath && m_fileBytes.IsCurrent();
    bool                                            opened   = hostFile && (current || m_fileBytes.Open (preview.hostPath));
    HostCharset                                     charset  = opened ? m_fileBytes.GetCharset() : preview.hostCharset;
    bool                                            error    = preview.kind == PreviewContent::Kind::Error || (hostFile && !opened);
    bool                                            catalog  = preview.kind == PreviewContent::Kind::Catalog;
    bool                                            hex      = preview.kind == PreviewContent::Kind::Hex && (!preview.bytes.empty() || opened);
    bool                                            disasm   = preview.kind == PreviewContent::Kind::Hex && preview.bytes.empty() && !preview.lines.empty();
    bool                                            columns  = preview.kind == PreviewContent::Kind::Catalog && preview.lines.empty();
    bool                                            program  = preview.kind == PreviewContent::Kind::Listing && !preview.bytes.empty();
    bool                                            forms    = picture && m_window.m_browser.CanTogglePictureColor();
    bool                                            shell    = preview.kind == PreviewContent::Kind::Shell;
    int                                             mode     = !visible ? CassoExplorerCommands::kPreviewBarNone
                                                             : hex      ? CassoExplorerCommands::kPreviewBarHex
                                                             : disasm   ? CassoExplorerCommands::kPreviewBarHex
                                                             : program  ? CassoExplorerCommands::kPreviewBarListing
                                                             : forms    ? CassoExplorerCommands::kPreviewBarPicture
                                                                        : CassoExplorerCommands::kPreviewBarNone;
    bool                                            blank    = false;



    UpdateShellPreview (preview);

    if (picture)
    {
        m_window.m_picture->SetFramebuffer (preview.bgra.data(), preview.width, preview.height);
        UpdateCrtPicture (true);
    }
    else if (!(shell && m_window.m_shellPicture))
    {
        m_window.m_picture->Clear();
    }

    //  A catalog of any volume but DOS 3.3 or ProDOS arrives as rows under
    //  sortable columns. Everything else shown as text goes to the text view,
    //  where it wraps and selects by character.
    //  An archive's rows look as the file list would show them inside it.
    if (columns)
    {
        m_window.m_previewList->SetShowHeader      (true);
        m_window.m_previewList->SetMonospace       (!preview.archive);
        m_window.m_previewList->SetExplorerDetails (preview.archive);

        //  An archive's rows move and extend by the keys the file list takes.
        m_window.m_previewList->SetKeyboardColumnNav (preview.archive);
        m_window.m_previewList->SetIconColumn      (preview.archive ? 0 : -1);
        m_window.m_previewList->SetFontDip         (preview.archive ? CassoExplorerWindow::kProseFontDip : 0.0f);
        m_window.m_previewList->SetRowHeightDip    (CassoExplorerWindow::kPreviewRowHeightDip);
        m_window.m_previewList->SetRowHeightPxFn   (preview.archive ? std::function<int (UINT)> (&ExplorerLayout::GetListRowHeightPx) : nullptr);
        m_window.m_previewList->SetColumns         (preview.archive ? CassoExplorerBrowser::GetArchivePreviewColumns() : CassoExplorerBrowser::GetCatalogPreviewColumns());
        m_window.m_previewList->SetTextSelectionColors (!preview.archive);

        for (const CatalogRow & row : preview.rows)
        {
            rows.push_back (preview.archive ? m_window.m_browser.GetArchivePreviewCells (row) : CassoExplorerBrowser::ToCatalogPreviewCells (row));
        }
    }

    m_window.m_previewArchive = columns && preview.archive;
    m_window.m_previewList->SetRows (std::move (rows));
    m_window.m_previewList->ResetAutoFit();
    m_window.m_previewList->UpdateAutoFitFromRows();
    m_window.m_previewList->SetTopRow (0);

    m_window.m_textView->SetRows (BuildTextRows (preview, m_window.m_prefs.lineAddresses, GetSourceColors (m_window.m_theme)));

    //  A new kind of preview brings its own toolbar, which changes the height
    //  left for the content.
    if (mode != m_window.m_previewBarMode)
    {
        m_window.m_previewBarMode = mode;

        if (mode != CassoExplorerCommands::kPreviewBarNone)
        {
            m_window.m_previewToolbar->SetEntries (m_window.m_commands.BuildPreviewToolbarEntries (mode, &m_window.m_searchBox, &m_window.m_goToBox));
        }

        //  The boxes go with the hex view's toolbar, and their focus with it.
        if (mode != CassoExplorerCommands::kPreviewBarHex && (m_window.m_focus == ExplorerInput::Pane::Search || m_window.m_focus == ExplorerInput::Pane::GoTo))
        {
            m_window.m_input.SetFocusPane (ExplorerInput::Pane::Preview);
        }

        m_window.m_layout.RecomputeLayout();
    }

    //  The hex view reads the preview's own bytes where they lie. The source
    //  is re-pointed rather than refilled, so a file of any size costs the
    //  same here as a short one.
    //  A host file is read from the file as the view draws it; an Apple file
    //  is already in memory. Text opens showing only its characters.
    if (!opened)
    {
        m_fileBytes.Close();
    }

    m_previewBytes.SetBytes ((hex && !opened) ? &preview.bytes : nullptr);
    m_window.m_hexView->SetSource (!hex ? nullptr : opened ? (const IDxuiHexSource *) &m_fileBytes : (const IDxuiHexSource *) &m_previewBytes);
    m_window.m_hexView->SetOriginAddress (preview.origin);
    m_window.m_hexView->SetTextEncoding (GetHexEncoding (opened, charset));

    if (hex)
    {
        //  UTF-8 too large to read as text opens on its bytes: the view draws
        //  one character for each byte.
        m_window.m_hexView->SetShowValues (!(opened ? charset != HostCharset::None && charset != HostCharset::Utf8 : preview.textFile));
    }

    //  With nothing selected the pane says so, as Explorer's does.
    blank = m_window.m_list->GetSelectedRows().empty() && !picture && !shell && !error && !hex && !columns && !program;

    m_window.m_previewMessage->SetText (blank                   ? std::wstring (kSelectToPreview)
                                        : !error                ? std::wstring()
                                        : (hostFile && !opened) ? FormatPreviewError (L"The file could not be read. " + ChangePrompt::DescribeError (m_fileBytes.GetError()))
                                                       : FormatPreviewError (preview.message));
    m_window.m_previewMessage->SetVisible (visible && (error || blank));

    //  Explorer's prompt is smaller body text; an error is muted. Both are
    //  centered in the pane.
    m_window.m_previewMessage->SetTextRole (blank ? DxuiTextRole::Body : DxuiTextRole::Muted);
    m_window.m_previewMessage->SetFontDip  (blank ? kPromptFontDip : 0.0f);

    m_window.m_picture->SetVisible (visible && (picture || (shell && m_window.m_shellPicture)));
    m_window.m_previewList->SetVisible (visible && columns);
    m_window.m_textView->SetVisible (visible && !picture && !shell && !error && !hex && !columns && !blank);
    m_window.m_hexView->SetVisible (visible && hex);

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::UpdateShellPreview
//
//  A shell preview is loaded once for its file and reused while the same
//  file is previewed; any other preview, or the pane hidden, unloads it. A
//  load that takes longer than a few seconds is replaced by the bytes.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::UpdateShellPreview (const PreviewContent & preview)
{
    bool      shell      = preview.kind == PreviewContent::Kind::Shell && m_window.m_prefs.previewVisible;
    uint32_t  background = m_window.m_theme->ContentBackground();
    uint32_t  text       = m_window.m_theme->Foreground();



    if (!shell)
    {
        if (!m_window.m_shellPreviewPath.empty())
        {
            KillTimer (m_window.GetHwnd(), CassoExplorerWindow::kShellPreviewTimerId);
            m_window.m_shellPreview.Unload();
            m_window.m_shellPreviewPath.clear();
            m_window.m_shellPicture = false;
        }

        return;
    }

    if (preview.hostPath == m_window.m_shellPreviewPath)
    {
        return;
    }

    m_window.m_shellPreviewPath = preview.hostPath;
    m_window.m_shellPicture     = false;
    m_window.m_picture->Clear();

    m_window.m_shellPreview.Load (m_window.m_shellPreviewPath, m_window.m_previewContent,
                                  RGB ((background >> 16) & 0xFF, (background >> 8) & 0xFF, background & 0xFF),
                                  RGB ((text >> 16) & 0xFF, (text >> 8) & 0xFF, text & 0xFF));

    SetTimer (m_window.GetHwnd(), CassoExplorerWindow::kShellPreviewTimerId, kShellPreviewTimeoutMs, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::OnShellPreviewResult
//
//  A handler's window becomes visible over the pane; a thumbnail is drawn in
//  the picture view, fitted; after a failure the bytes are drawn.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::OnShellPreviewResult()
{
    Win32ShellPreview::Result  result;



    if (m_window.m_shellPreviewPath.empty() || !m_window.m_shellPreview.TakeResult (result))
    {
        return;
    }

    KillTimer (m_window.GetHwnd(), CassoExplorerWindow::kShellPreviewTimerId);

    switch (result.outcome)
    {
        case Win32ShellPreview::Outcome::Shown:
            m_window.m_shellPreview.ShowHost (true);
            break;

        case Win32ShellPreview::Outcome::Picture:
            m_window.m_picture->SetFramebuffer (result.bgra.data(), result.width, result.height);
            m_window.m_picture->SetVisible (m_window.m_prefs.previewVisible);
            m_window.m_shellPicture = true;
            break;

        case Win32ShellPreview::Outcome::Failed:
        default:
            ShowShellPreviewBytes();
            break;
    }

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ShowShellPreviewBytes
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::ShowShellPreviewBytes()
{
    KillTimer (m_window.GetHwnd(), CassoExplorerWindow::kShellPreviewTimerId);
    m_window.m_shellPreview.Unload();
    m_window.m_shellPreviewPath.clear();
    m_window.m_shellPicture = false;

    m_window.m_browser.ShowShellPreviewBytes();
    FillPreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::FileBytes::Open
//
//  Shared for reading, writing and deletion, so previewing a file never gets
//  in the way of another program using it.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::FileBytes::Open (const std::wstring & path)
{
    LARGE_INTEGER  size = {};
    HRESULT        hr   = S_OK;



    Close();

    m_file = CreateFileW (path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    m_error = (m_file == INVALID_HANDLE_VALUE) ? HRESULT_FROM_WIN32 (GetLastError()) : S_OK;

    if (m_file != INVALID_HANDLE_VALUE)
    {
        hr      = Win32DiskFileIo::HydrateIfPlaceholder (m_file);
        m_error = SUCCEEDED (hr) && !GetFileSizeEx (m_file, &size) ? HRESULT_FROM_WIN32 (GetLastError()) : hr;
    }

    if (FAILED (m_error))
    {
        Close();
        return false;
    }

    m_path = path;
    m_size = (uint64_t) size.QuadPart;

    if (!GetFileTime (m_file, nullptr, nullptr, &m_written))
    {
        m_written = {};
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::FileBytes::IsCurrent
//
//  By the path, not the open handle: a save that writes a new file and
//  renames it over this one: the handle is then on the old file.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::FileBytes::IsCurrent() const
{
    WIN32_FILE_ATTRIBUTE_DATA  data  = {};
    bool                       found = !m_path.empty() && GetFileAttributesExW (m_path.c_str(), GetFileExInfoStandard, &data);
    uint64_t                   size  = ((uint64_t) data.nFileSizeHigh << 32) | data.nFileSizeLow;



    return found && size == m_size && CompareFileTime (&data.ftLastWriteTime, &m_written) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::FileBytes::Close
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::FileBytes::Close()
{
    if (m_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle (m_file);
    }

    m_file       = INVALID_HANDLE_VALUE;
    m_size       = 0;
    m_windowBase = 0;

    m_path.clear();
    m_window.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::FileBytes::GetCharset
//
////////////////////////////////////////////////////////////////////////////////

HostCharset ExplorerPreview::FileBytes::GetCharset() const
{
    std::vector<uint8_t>  sample ((size_t) (std::min) ((uint64_t) kTextSampleBytes, m_size));



    if (sample.empty())
    {
        return HostCharset::None;
    }

    ReadAt (0, sample);

    return CassoExplorerBrowser::GetHostCharset (sample);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::FileBytes::ReadBytes
//
//  The window starts a little before the bytes asked for, so scrolling back
//  a few rows does not move it again straight away.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::FileBytes::ReadBytes (uint64_t offset, std::span<uint8_t> out) const
{
    uint64_t  end = offset + out.size();



    if (out.size() > kWindowBytes / 2)
    {
        ReadAt (offset, out);
        return;
    }

    if (m_window.empty() || offset < m_windowBase || end > m_windowBase + m_window.size())
    {
        m_windowBase = (offset > kWindowBytes / 4) ? (offset - kWindowBytes / 4) : 0;
        m_window.resize ((size_t) (std::min) ((uint64_t) kWindowBytes, m_size - (std::min) (m_windowBase, m_size)));

        ReadAt (m_windowBase, m_window);
    }

    if (end > m_windowBase + m_window.size())
    {
        ReadAt (offset, out);
        return;
    }

    std::copy_n (m_window.begin() + (ptrdiff_t) (offset - m_windowBase), out.size(), out.begin());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::FileBytes::ReadAt
//
//  Bytes the file no longer has, because it shrank while shown, read as zero.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::FileBytes::ReadAt (uint64_t offset, std::span<uint8_t> out) const
{
    LARGE_INTEGER  position = {};
    DWORD          got      = 0;



    std::fill (out.begin(), out.end(), (uint8_t) 0);

    if (m_file == INVALID_HANDLE_VALUE || out.empty())
    {
        return;
    }

    position.QuadPart = (LONGLONG) offset;

    //  A failed seek or read leaves the zeros already there.
    if (SetFilePointerEx (m_file, position, nullptr, FILE_BEGIN) &&
        !ReadFile (m_file, out.data(), (DWORD) out.size(), &got, nullptr))
    {
        return;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::IsHexPreviewShowing
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::IsHexPreviewShowing() const
{
    return m_window.m_prefs.previewVisible && m_window.m_hexView->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::IsTextPreviewShowing
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::IsTextPreviewShowing() const
{
    return m_window.m_prefs.previewVisible && m_window.m_textView->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetSourceColors
//
//  The background is dark when its luminance is under half. An address and
//  bytes, a listing's columns, are drawn in the muted text color.
//
////////////////////////////////////////////////////////////////////////////////

SourceSyntax::Colors ExplorerPreview::GetSourceColors (const IDxuiTheme * theme)
{
    SourceSyntax::Colors  colors;
    uint32_t              page   = (theme != nullptr) ? theme->ContentBackground() : 0u;
    uint32_t              luma   = ((page >> 16) & 0xFF) * 299 + ((page >> 8) & 0xFF) * 587 + (page & 0xFF) * 114;
    bool                  dark   = luma < s_kDarkLumaLimit;



    if (theme == nullptr)
    {
        return colors;
    }

    colors.mnemonic  = dark ? 0xFF8CBCF0 : 0xFF0000FF;
    colors.directive = dark ? 0xFFC586C0 : 0xFFAF00DB;
    colors.symbol    = dark ? 0xFF4EC9B0 : 0xFF2B91AF;
    colors.number    = dark ? 0xFFB5CEA8 : 0xFF098658;
    colors.string    = dark ? 0xFFD69D85 : 0xFFA31515;
    colors.comment   = dark ? 0xFF57A64A : 0xFF008000;
    colors.address   = theme->ForegroundMuted();
    colors.bytes     = theme->ForegroundMuted();

    return colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::BuildTextRows
//
//  A listing that is not a valid program ends with a blank row and the
//  warning that says why.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTextView::Row> ExplorerPreview::BuildTextRows (const PreviewContent & preview, bool lineAddresses, const SourceSyntax::Colors & colors)
{
    std::vector<DxuiTextView::Row>  rows;
    DxuiTextView::Row               warning;
    std::vector<Word>               addresses;



    if (lineAddresses && preview.kind == PreviewContent::Kind::Listing)
    {
        addresses = GetLineAddresses (preview.bytes, preview.integerBasic);
    }

    if (!addresses.empty())
    {
        for (size_t i = 0; i < preview.lines.size(); i++)
        {
            DxuiTextView::Row  row { SplitLineNumber (preview.lines[i]) };

            row.cells.insert (row.cells.begin(), (i < addresses.size()) ? std::format (L"${:04X}", addresses[i]) : std::wstring());
            rows.push_back (std::move (row));
        }
    }
    else if (preview.kind == PreviewContent::Kind::Details)
    {
        for (const std::pair<std::wstring, std::wstring> & field : preview.details)
        {
            rows.push_back (DxuiTextView::Row { { field.first, field.second } });
        }
    }
    else if (preview.source)
    {
        //  An assembler's source has its line numbers, right aligned in a
        //  column of their own, as an editor's gutter has them, and its syntax
        //  colored by the grammar of the assembler it was written for, as the
        //  debugger colors it.
        size_t                     digits    = std::to_wstring ((std::max) (preview.lines.size(), (size_t) 1)).size();
        SourceSyntax::Assembler    assembler = SourceSyntax::DetectAssembler (preview.lines);
        SourceSyntax::Listing      listing   = SourceSyntax::DetectListing (preview.lines);

        for (size_t i = 0; i < preview.lines.size(); i++)
        {
            DxuiTextView::Row  row { { std::format (L"{:>{}}", i + 1, digits), preview.lines[i] } };

            if (colors.mnemonic != 0)
            {
                for (const SourceSyntax::Run & run : SourceSyntax::GetLineRuns (preview.lines[i], assembler, listing))
                {
                    row.spans.push_back ({ 1, run.start, run.length, colors.Get (run.token) });
                }
            }

            rows.push_back (std::move (row));
        }
    }
    else
    {
        for (const std::wstring & line : preview.lines)
        {
            rows.push_back (DxuiTextView::Row { (preview.kind == PreviewContent::Kind::Listing) ? SplitLineNumber (line)
                                                                                               : std::vector<std::wstring> { line } });
        }
    }

    if (!preview.warning.empty())
    {
        warning.cells.push_back (preview.warning);
        warning.warning = true;

        rows.push_back (DxuiTextView::Row());
        rows.push_back (warning);
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetLineAddresses
//
//  Where each line of a BASIC program starts in memory.
//
//  An Applesoft line begins with a pointer to the next one, and a pointer is
//  an absolute address, so the first line's pointer less the second line's
//  offset in the file is the address the program loads at.
//
//  An Integer BASIC line begins with its length, and the program sits against
//  HIMEM, which DOS 3.3 on a 48K machine leaves at $9600. The addresses assume
//  that HIMEM, since the file does not record one.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Word> ExplorerPreview::GetLineAddresses (const std::vector<Byte> & program, bool integerBasic)
{
    std::vector<size_t>  starts;
    std::vector<Word>    addresses;
    size_t               offset    = 0;
    Word                 firstLink = 0;
    Word                 base      = 0;



    if (integerBasic)
    {
        while (offset < program.size() && program[offset] != 0 && offset + program[offset] <= program.size())
        {
            starts.push_back (offset);
            offset += program[offset];
        }

        base = (Word) (kIntegerBasicHimem - program.size());
    }

    while (!integerBasic && offset + 4 <= program.size())
    {
        Word    link = (Word) (program[offset] | (program[offset + 1] << 8));
        size_t  end  = offset + 4;

        if (link == 0)
        {
            break;
        }

        while (end < program.size() && program[end] != 0)
        {
            end++;
        }

        if (end >= program.size())
        {
            break;
        }

        if (starts.empty())
        {
            firstLink = link;
        }

        starts.push_back (offset);
        offset = end + 1;

        if (starts.size() == 1)
        {
            base = (Word) (firstLink - offset);
        }
    }

    for (size_t start : starts)
    {
        addresses.push_back ((Word) (base + start));
    }

    return addresses;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SplitLineNumber
//
//  A listing line's number and the statement after the spaces that follow it,
//  or the whole line as one cell when it does not start with a number.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ExplorerPreview::SplitLineNumber (const std::wstring & line)
{
    size_t  first = line.find_first_not_of (L' ');
    size_t  end   = std::wstring::npos;
    size_t  text  = std::wstring::npos;



    if (first != std::wstring::npos && iswdigit (line[first]))
    {
        end  = line.find_first_not_of (L"0123456789", first);
        text = (end == std::wstring::npos) ? std::wstring::npos : line.find_first_not_of (L' ', end);
    }

    if (text == std::wstring::npos || text == end)
    {
        return { line };
    }

    return { line.substr (0, end), line.substr (text) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetSelectedArchiveIds
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ExplorerPreview::GetSelectedArchiveIds() const
{
    const std::vector<CatalogRow> &  rows = m_window.m_browser.GetPreview().rows;
    std::vector<std::wstring>        ids;



    for (int row : m_window.m_previewList->GetSelectedRows())
    {
        if (row >= 0 && (size_t) row < rows.size() && !rows[(size_t) row].shellId.empty())
        {
            ids.push_back (rows[(size_t) row].shellId);
        }
    }

    return ids;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::RouteArchiveMouse
//
//  An archive's files in the preview act as the file list's do: a press that
//  moves drags them out, for the shell to extract where they land, and a
//  right-click opens the shell's menu for them, Extract All among its verbs.
//  Selection itself is the list's. True when the event is taken here.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::RouteArchiveMouse (const DxuiMouseEvent & ev, POINT point)
{
    RECT                       bounds = m_window.m_previewList->GetBounds();
    DxuiMouseEvent             local  = CassoExplorerWindow::ToLocal (ev, bounds);
    int                        row    = -1;
    HRESULT                    hr     = S_OK;
    POINT                      screen = point;
    std::vector<std::wstring>  ids;



    if (ev.kind == DxuiMouseEventKind::Down && CassoExplorerWindow::Contains (bounds, point))
    {
        row = m_window.m_previewList->HitTestRow (local.positionDip.x, local.positionDip.y);
        m_window.m_input.SetFocusPane (ExplorerInput::Pane::Preview);

        if (row >= 0 && ev.button == DxuiMouseButton::Right)
        {
            //  A right press on a row outside the selection selects it alone.
            if (!m_window.m_previewList->IsRowSelected (row))
            {
                m_window.m_previewList->ClickRow (row, false, false);
            }

            m_archiveMenu = true;
            return true;
        }

        if (row >= 0 && ev.button == DxuiMouseButton::Left)
        {
            m_archiveDrag = true;
            m_archivePress     = point;
        }

        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Up && m_archiveMenu && ev.button == DxuiMouseButton::Right)
    {
        m_archiveMenu = false;
        ids                = GetSelectedArchiveIds();

        ClientToScreen (m_window.GetHwnd(), &screen);

        if (!ids.empty())
        {
            ShowArchiveContextMenu (point.x, point.y);
        }

        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Up)
    {
        m_archiveDrag = false;
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Move && m_archiveDrag
        && (abs (point.x - m_archivePress.x) > GetSystemMetrics (SM_CXDRAG)
         || abs (point.y - m_archivePress.y) > GetSystemMetrics (SM_CYDRAG)))
    {
        DxuiMouseEvent  release;

        m_archiveDrag = false;
        ids                = GetSelectedArchiveIds();

        //  The list lets go first, or it would take the move as extending
        //  the selection once the drag is over.
        release.kind   = DxuiMouseEventKind::Up;
        release.button = DxuiMouseButton::Left;
        m_window.m_previewList->OnMouse (release);

        if (!ids.empty())
        {
            hr = m_window.m_shellVerbs.DragItems (ids, DROPEFFECT_COPY);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::CopyArchiveSelection
//
//  The shell's data object goes on the clipboard, which keeps the archive's
//  folder open for a paste after the list has moved on.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::CopyArchiveSelection (bool cut)
{
    std::vector<std::wstring>  ids = GetSelectedArchiveIds();
    HRESULT                    hr  = S_OK;



    if (ids.empty())
    {
        return;
    }

    hr = m_window.m_shellVerbs.PlaceOnClipboard (m_window.GetHwnd(), ids, cut);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::DeleteArchiveSelection
//
//  An archive has no Recycle Bin, so its files are deleted for good, after
//  Windows asks to confirm, as File Explorer deletes them. The archive is
//  then read again for the preview.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::DeleteArchiveSelection()
{
    std::vector<std::wstring>  ids = GetSelectedArchiveIds();
    HRESULT                    hr  = S_OK;



    if (ids.empty())
    {
        return;
    }

    hr = m_window.m_shellVerbs.DeletePermanently (m_window.GetHwnd(), ids);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_window.m_folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ShowArchiveContextMenu
//
//  The file list's menu for an archive's files: the row of Cut, Copy and
//  Delete, Open, and the shell's full menu under Show more options, Extract
//  All among it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::ShowArchiveContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>                   items;
    std::vector<std::shared_ptr<const DxuiCommand>>  iconCommands;
    POINT                                            screen = { x, y };
    const CassoExplorerActions::Verb                 iconVerbs[] =
    {
        CassoExplorerActions::Verb::Cut,
        CassoExplorerActions::Verb::Copy,
        CassoExplorerActions::Verb::Delete,
    };



    m_window.m_menuCommands.clear();
    ClientToScreen (m_window.GetHwnd(), &screen);

    for (CassoExplorerActions::Verb verb : iconVerbs)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->id       = (int) verb;
        command->label    = ExplorerMenus::GetVerbLabel (verb);
        command->glyph    = ExplorerMenus::GetVerbGlyph (verb);
        command->menuSvg  = m_window.m_menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (verb));
        command->dispatch = [this, verb]()
        {
            if (verb == CassoExplorerActions::Verb::Delete)
            {
                DeleteArchiveSelection();
            }
            else
            {
                CopyArchiveSelection (verb == CassoExplorerActions::Verb::Cut);
            }
        };
        iconCommands.push_back (command);
        m_window.m_menuCommands.push_back (std::move (command));
    }

    items.push_back (DxuiPopupMenuItem::ForIconRow (std::move (iconCommands)));
    items.push_back (DxuiPopupMenuItem::ForSeparator());

    m_window.m_menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::Open), [this]()
    {
        if (!m_window.m_previewList->GetSelectedRows().empty())
        {
            OpenArchiveRow (m_window.m_previewList->GetSelectedRows()[0]);
        }
    });

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    m_window.m_menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, screen]()
    {
        HRESULT  hr = m_window.m_shellVerbs.ShowShellMenu (m_window.GetHwnd(), GetSelectedArchiveIds(), screen, {});

        IGNORE_RETURN_VALUE (hr, S_OK);
    });
    m_window.m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
    m_window.m_menuCommands.back()->menuSvg   = m_window.m_menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (CassoExplorerActions::Verb::MoreOptions));

    DxuiContextMenu::Show (*m_window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::OpenArchiveRow
//
//  A folder inside the archive opens in the file list; a file opens as the
//  shell opens it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::OpenArchiveRow (int row)
{
    const std::vector<CatalogRow> &  rows = m_window.m_browser.GetPreview().rows;
    HRESULT                          hr   = S_OK;



    if (row < 0 || (size_t) row >= rows.size() || rows[(size_t) row].shellId.empty())
    {
        return;
    }

    if (rows[(size_t) row].isDirectory)
    {
        m_window.m_browser.NavigateToLocation (Location::MakeShellFolder (rows[(size_t) row].shellId, rows[(size_t) row].name));
        m_window.m_listPane.FillList();
        return;
    }

    hr = m_window.m_shellVerbs.OpenShellItem (m_window.GetHwnd(), rows[(size_t) row].shellId);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::FormatPreviewError
//
//  "No preview", and three lines below it, why. An image refused for its size
//  has a whole sentence about the file for the command line; the preview
//  has only the reason. A read under way is no failure, and its message
//  has no heading.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerPreview::FormatPreviewError (const std::wstring & message)
{
    std::wstring  reason = message;



    if (message.find (L"800K images") != std::wstring::npos)
    {
        reason = L"800K images are not supported yet";
    }

    if (message == CassoExplorerBrowser::s_kWorkingText)
    {
        return message;
    }

    return L"No preview\n\n\n" + reason;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SetPreviewZoom
//
//  The text and hex previews share one zoom, kept within the range the
//  preferences allow.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SetPreviewZoom (int percent)
{
    int  clamped = (std::max) (CassoExplorerPrefs::kMinPreviewZoom, (std::min) (percent, CassoExplorerPrefs::kMaxPreviewZoom));



    m_window.m_prefs.previewZoom = clamped;

    m_window.m_textView->SetZoom ((float) clamped / 100.0f);
    m_window.m_hexView->SetZoom  ((float) clamped / 100.0f);
    m_window.m_picture->SetZoom  ((float) clamped / 100.0f);
    UpdateCrtPicture (false);

    m_window.m_layout.FillStatus();
    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SetHexColumns
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SetHexColumns (int columns)
{
    m_window.m_hexView->SetColumns (columns);
    m_window.m_prefs.hexColumns = columns;

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SetHexFormat
//
//  Choosing a format shows the values it applies to.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SetHexFormat (const char * format)
{
    m_window.m_prefs.hexFormat     = format;
    m_window.m_prefs.hexShowValues = true;

    m_window.m_hexView->SetShowValues  (true);
    m_window.m_hexView->SetValueFormat (ParseHexFormat (m_window.m_prefs.hexFormat));

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SetHexShowValues
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SetHexShowValues (bool show)
{
    m_window.m_prefs.hexShowValues = show;
    m_window.m_hexView->SetShowValues (show);

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ParseHexFormat
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexView::ValueFormat ExplorerPreview::ParseHexFormat (const std::string & name)
{
    if (name == CassoExplorerPrefs::kHexFormatSigned)
    {
        return DxuiHexView::ValueFormat::Signed;
    }

    if (name == CassoExplorerPrefs::kHexFormatUnsigned)
    {
        return DxuiHexView::ValueFormat::Unsigned;
    }

    return DxuiHexView::ValueFormat::Hex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GetPreviewStopIndex
//
//  A command's place among the hex view toolbar's stops, or -1 while that
//  toolbar is not showing.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerPreview::GetPreviewStopIndex (int commandId) const
{
    std::vector<int>  ids = CassoExplorerCommands::GetPreviewToolbarCommandIds (true);



    if (m_window.m_previewBarMode != CassoExplorerCommands::kPreviewBarHex)
    {
        return -1;
    }

    for (size_t i = 0; i < ids.size(); i++)
    {
        if (ids[i] == commandId)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SetHexGrouping
//
//  A grouping the row cannot divide is refused by the view, and a refused
//  one is not written to the preferences either.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SetHexGrouping (int grouping)
{
    if (!m_window.m_hexView->SetGrouping (grouping))
    {
        return;
    }

    m_window.m_prefs.hexGrouping   = grouping;
    m_window.m_prefs.hexShowValues = true;

    m_window.m_hexView->SetShowValues (true);

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::GoToTyped
//
//  An address matches the offset column, which counts from the file's load
//  address when it has one; an offset moves from the caret. A target that
//  does not parse or is outside the file marks the box in error, and one
//  that is inside it moves the caret there and returns focus to the bytes.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::GoToTyped (const std::wstring & text)
{
    int64_t   origin = (int64_t) m_window.m_hexView->GetOriginAddress();
    int64_t   caret  = origin + (int64_t) m_window.m_hexView->GetCaret();
    int64_t   count  = (m_window.m_hexView->GetSource() != nullptr) ? (int64_t) m_window.m_hexView->GetSource()->GetByteCount() : 0;
    int64_t   target = 0;
    int64_t   last   = 0;



    if (!IsHexPreviewShowing())
    {
        return;
    }

    if (!CassoExplorerActions::TryParseGoTo (text, caret, target, last) || target < origin || last >= origin + count)
    {
        m_window.m_goToBox.SetError (true);
        m_window.Invalidate();
        return;
    }

    m_window.m_goToBox.SetText  (L"");
    m_window.m_goToBox.SetError (false);
    m_window.m_hexView->GoToOffset ((uint64_t) (target - origin));

    //  A range selects from its first address to its last.
    if (last > target)
    {
        m_window.m_hexView->ExtendSelectionTo ((uint64_t) (last - origin));
        m_window.m_hexView->EnsureByteVisible ((uint64_t) (target - origin));
    }

    m_window.m_input.SetFocusPane (ExplorerInput::Pane::Preview);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::OnSearchChanged
//
//  Each edit searches again from the start of the current match, so typing
//  more of a term keeps the match it has while it still fits. Hex digits find
//  bytes; other text finds characters with or without the high bit, since
//  Apple II text is stored both ways.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::OnSearchChanged (const std::wstring & text)
{
    if (!CassoExplorerActions::TryParseSearch (text, m_window.m_findBytes, m_findIsText))
    {
        m_window.m_findBytes.clear();
        m_window.Invalidate();
        return;
    }

    m_findTyped = text;
    FindNext (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::FindNext
//
//  Searches forward from just past the selection, wrapping at the end. An
//  incremental search starts at the selection itself and says nothing when
//  the term is not found, since the user is still typing it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::FindNext (bool incremental)
{
    const IDxuiHexSource *  source = m_window.m_hexView->GetSource();
    uint64_t                start  = 0;
    uint64_t                found  = 0;



    if (!IsHexPreviewShowing() || m_window.m_findBytes.empty() || source == nullptr)
    {
        return;
    }

    //  The search reads the whole file through the source, not just the part
    //  the view has drawn.
    start = m_window.m_hexView->HasSelection() ? m_window.m_hexView->GetSelectionFirst() + (incremental ? 0 : 1) : m_window.m_hexView->GetCaret();
    found = CassoExplorerActions::FindInSource ([source] (uint64_t offset, std::span<uint8_t> out) { source->ReadBytes (offset, out); },
                                          source->GetByteCount(), m_window.m_findBytes, m_findIsText, start);

    if (found == CassoExplorerActions::kNotFoundOffset)
    {
        if (!incremental)
        {
            m_window.ShowMessage (std::format (L"{} isn't in this file.", m_findTyped).c_str(), MB_ICONINFORMATION);
        }

        return;
    }

    m_window.m_hexView->SelectByte        ((uint64_t) found, m_findIsText ? DxuiHexView::Column::Text : DxuiHexView::Column::Hex);
    m_window.m_hexView->ExtendSelectionTo ((uint64_t) (found + m_window.m_findBytes.size() - 1));
    m_window.m_hexView->EnsureByteVisible ((uint64_t) found);

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ShowPictureContextMenu
//
//  Color and Monochrome for a double hi-res picture, one of them checked;
//  the choice lasts for this picture.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::ShowPictureContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;
    bool                            color = m_window.m_browser.GetPreview().pictureMode == PicturePreview::Mode::DoubleHiRes;



    m_window.m_menuCommands.clear();

    for (bool asColor : { true, false })
    {
        m_window.m_menus.AddMenuCommand (items, asColor ? L"&Color" : L"&Monochrome", [this, asColor]() { ShowPictureForm (asColor); });

        m_window.m_menuCommands.back()->radio     = true;
        m_window.m_menuCommands.back()->isChecked = [color, asColor]() { return color == asColor; };
    }

    DxuiContextMenu::Show (*m_window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ShowPictureForm
//
//  A double hi-res picture drawn in color or as its bits, on the monitor that
//  form is drawn on; the form already drawn is left as it is, so one of the
//  two is always the one drawn.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::ShowPictureForm (bool color)
{
    if ((m_window.m_browser.GetPreview().pictureMode == PicturePreview::Mode::DoubleHiRes) != color && m_window.m_browser.TogglePictureColor())
    {
        FillPreview();
        m_window.Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::ShowTextContextMenu
//
//  Copy and Select all, which reach the text view as the focused control.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::ShowTextContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    for (int id : { (int) CassoExplorerCommands::kCopy, (int) CassoExplorerCommands::kSelectAll })
    {
        std::shared_ptr<const DxuiCommand>  command = m_window.m_commands.Find (id);

        if (command != nullptr)
        {
            items.push_back (DxuiPopupMenuItem::ForCommand (command));
        }
    }

    DxuiContextMenu::Show (*m_window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::IsArchiveLocation
//
//  Whether the list shows the inside of an archive: a shell folder whose path
//  lies on a lettered drive but is no folder on it.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreview::IsArchiveLocation() const
{
    const Location &  location = m_window.m_browser.GetLocation();



    return location.kind == Location::Kind::ShellFolder && location.path.size() > 3 && location.path[1] == L':'
        && m_window.m_context.fs != nullptr && !m_window.m_context.fs->DirectoryExists (location.path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview::SelectMonitor
//
//  The picture preview drawn again on the monitor, in the custom monitor's
//  saved color for that one, which is saved for the next start, apart from
//  Casso's own.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreview::SelectMonitor (PicturePreview::Monitor monitor)
{
    m_window.m_prefs.previewMonitor = CassoExplorerPrefs::kMonitorNames[(int) monitor];
    m_window.m_browser.SetPreviewMonitor (monitor, m_window.m_prefs.previewCustomArgb);
    FillPreview();
    m_window.Invalidate();
}

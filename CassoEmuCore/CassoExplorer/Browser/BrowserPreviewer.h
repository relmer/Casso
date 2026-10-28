#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserTypes.h"

#include "CassoExplorer/Model/PreviewDecoder.h"
#include "Core/MemoryBus.h"

class CassoExplorerBrowser;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer
//
//  The preview of the selection: a catalog entry, a host file, text in
//  its character set, an archive's files, a shell item, a search match, a
//  linked Apple's drive or entry, and an AppleSingle file.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserPreviewer
{
public:
    explicit BrowserPreviewer (CassoExplorerBrowser & browser);

    //  How much of a file with no file of its own, such as one inside a zip,
    //  is read for its preview.
    static constexpr size_t  s_kShellPreviewMaxBytes = 16 * 1024 * 1024;

    //  An archive's preview as the file list would show it opened: the list's
    //  first columns, with its icons.
    static constexpr size_t                    s_kArchivePreviewColumns = 4;
    static std::vector<DxuiListView::Column>   GetArchivePreviewColumns ();
    std::vector<DxuiListView::Cell>            GetArchivePreviewCells   (const CatalogRow & row) const;

    //  Which character set a file's first bytes are text in, if any, and how
    //  many are looked at.
    static constexpr size_t  s_kTextSampleBytes      = 64 * 1024;
    static constexpr Byte    s_kFormFeed             = 0x0C;
    static constexpr Byte    s_kDosEndOfFile         = 0x1A;
    static constexpr Byte    s_kDosDrawingFirst      = 0xB0;    // code page 437's shades and box drawing
    static constexpr Byte    s_kDosDrawingLast       = 0xDF;
    static constexpr size_t  s_kTextControlRatio     = 100;     // at most one byte in this many a DOS symbol
    static HostCharset       GetHostCharset (std::span<const Byte> sample);
    static bool              IsUtf8         (std::span<const Byte> sample);

    //  Whether a file is an assembler's source by its extension, which
    //  previews as text with its line numbers.
    static bool              IsSourceName   (const std::wstring & name);

    //  Whether a file is a BASIC listing or text Casso Explorer exported, by
    //  its suffix, which previews as lines of text even in plain ASCII.
    static bool              IsAppleExportName (const std::wstring & name);

    //  The largest host file that previews as text rather than in the hex view.
    static constexpr size_t  s_kTextPreviewMaxBytes = 1024 * 1024;
    void  PreviewShellItem   (const CatalogRow & row);
    void  PreviewArchive     (const std::wstring & path);
    bool  IsLocalArchivePath (const std::wstring & id) const;

    //  Why an archive the shell lists nothing in is empty, by its name;
    //  empty when it has nothing in it.
    std::wstring  DescribeEmptyArchive (const std::wstring & path) const;

    //  The largest archive read whole for the reason it lists nothing.
    static constexpr uintmax_t  s_kArchiveProbeMaxBytes = 64ull * 1024 * 1024;

    void  ShowShellPreviewBytes ();
    void  SetDisassemble   (bool disassemble);

    void                     SetPreviewMonitor (PicturePreview::Monitor monitor, uint32_t customArgb = PicturePreview::kDefaultCustomArgb);

    PicturePreview::Monitor  GetPictureMonitor () const;

    static PicturePreview::Monitor  ChooseMonitor (PicturePreview::Monitor chosen, PicturePreview::Mode mode);

    bool  TogglePictureColor ();
    bool  CanTogglePictureColor () const;

    std::wstring  GetExtractTarget () const;

    void  GetSelectedSearchMatches (std::vector<BrowserSearchMatch> & outMatches);
    bool  AreSelectedRowsSearchMatches () const;
    bool  TryFindSearchMatch       (const CatalogRow & row, BrowserSearchMatch & outMatch, std::wstring & outError);
    void     PreviewAppleEntry (const std::string & entryPath, const FileEntry & entry);
    bool     PreviewAppleDrive (const CatalogRow & row);
    void     PreviewCassoLinkItem (const CatalogRow & row);

    bool     TryPreviewAppleSingle (const std::wstring & folder, const CatalogRow & row);
    void     ReadTextPreview       (const std::wstring & path, const std::wstring & name);
    bool     TryPreviewText        (const std::wstring & name, std::span<const Byte> bytes);

    static constexpr const wchar_t *  s_kpszSourceExtensions[] = { L".a65", L".asm", L".s", L".inc", L".a", L".src", L".mac", L".ca65" };
    static constexpr unsigned         s_kDosCodePage           = 437;
    static constexpr unsigned         s_kWindowsCodePage       = 1252;
    static constexpr wchar_t          s_kByteOrderMark         = 0xFEFF;
    static constexpr size_t           s_kTabStop               = 8;
    void     PreviewEntry          (const std::wstring & imagePath, const std::string & entryPath, const FileEntry & entry, VolumeKind kind);
    void     PreviewSearchMatch    (const CatalogRow & row);
    void     UpdatePreview();

private:
    friend class CassoExplorerBrowser;

    CassoExplorerBrowser  & m_browser;

    MemoryBus                           m_bus;
    PicturePreview::Monitor             m_previewMonitor = PicturePreview::Monitor::Color;
    uint32_t                            m_customArgb     = PicturePreview::kDefaultCustomArgb;
    PreviewContent                      m_preview;
};

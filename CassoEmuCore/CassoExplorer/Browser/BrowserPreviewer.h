#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Browser/BrowserTypes.h"
#include "CassoExplorer/Browser/Preview/IPreviewRefresh.h"
#include "CassoExplorer/Browser/Preview/PictureMonitorChoice.h"
#include "CassoExplorer/Browser/Preview/PreviewParts.h"
#include "CassoExplorer/Browser/Preview/PreviewViewState.h"
#include "CassoExplorer/Model/BrowserModel.h"

#include "CassoExplorer/Model/PreviewDecoder.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Core/MemoryBus.h"

class BrowserSelection;
class IBrowserPreviewHost;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer
//
//  The preview of the selection: a catalog entry, a host file, text in
//  its character set, an archive's files, a shell item, a search match, a
//  linked Apple's drive or entry, and an AppleSingle file.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserPreviewer : private IPreviewRefresh
{
public:
    BrowserPreviewer (const BrowserParts & parts, const BrowserSelection & selection, IBrowserPreviewHost & host);

    //  How much of a file with no file of its own, such as one inside a zip,
    //  is read for its preview.
    static constexpr size_t  s_kShellPreviewMaxBytes = 16 * 1024 * 1024;

    //  An archive's preview as the file list would show it opened: the list's
    //  first columns, with its icons.
    static constexpr size_t                    s_kArchivePreviewColumns = 4;
    static std::vector<DxuiListView::Column>   GetArchivePreviewColumns ();
    std::vector<DxuiListView::Cell>            GetArchivePreviewCells   (const CatalogRow & row) const;

    //  The largest host file that previews as text rather than in the hex view.
    static constexpr size_t  s_kTextPreviewMaxBytes = 1024 * 1024;
    void  PreviewShellItem   (const CatalogRow & row);
    void  PreviewArchive     (const std::wstring & path);
    bool  IsLocalArchivePath (const std::wstring & id) const;

    //  Why an archive the shell lists nothing in is empty, by its name;
    //  empty when it has nothing in it.
    std::wstring              DescribeEmptyArchive (const std::wstring & path) const;
    void                      AddWozDetails        (const std::wstring & imagePath);
    void                      PreviewImageCatalog  (const std::wstring & imagePath);
    bool                      PreviewPartitions    (const std::wstring & imagePath, PartitionLayoutKind layout, const std::vector<PartitionEntry> & entries);
    bool                      PreviewSavedGame     (const std::wstring & imagePath, const VolumeListing & listing, VolumeKind kind);
    static const FileEntry  * FindSavedGame        (const VolumeListing & listing, VolumeKind kind);
    bool                      TryDecodeDiskSave    (const std::wstring & imagePath, DecodedFile & outFile);

    using DetailIterator = std::vector<std::pair<std::wstring, std::wstring>>::iterator;

    static constexpr const wchar_t *  kAnalysisHeading = L"Casso analysis";

    //  The largest archive read whole for the reason it lists nothing.
    static constexpr uintmax_t  s_kArchiveProbeMaxBytes = 64ull * 1024 * 1024;

    //  A shell preview that failed or took too long is replaced by the file's
    //  bytes.
    void  ShowShellPreviewBytes ();

    const PreviewContent  & GetPreview () const { return m_preview; }

    //  The archive Extract all applies to, selected or open; empty for none.
    std::wstring  GetExtractTarget () const;

    void  GetSelectedSearchMatches     (std::vector<BrowserSearchMatch> & outMatches);
    bool  AreSelectedRowsSearchMatches () const;
    bool  TryFindSearchMatch           (const CatalogRow & row, BrowserSearchMatch & outMatch, std::wstring & outError);
    void  PreviewAppleEntry            (const std::string & entryPath, const FileEntry & entry);
    bool  PreviewAppleDrive            (const CatalogRow & row);
    void  PreviewCassoLinkItem         (const CatalogRow & row);

    bool     TryPreviewAppleSingle (const std::wstring & folder, const CatalogRow & row);
    void     PreviewHostFile       (const std::wstring & folder, const CatalogRow & row);
    bool     TryReadHostPayload    (const std::wstring & path, const CatalogRow & row, FilePayload & outPayload, FileEntry & outEntry, bool & outTyped);
    void     AttachHostViews       (const FileEntry & entry, const FilePayload & payload, bool loaded, PreviewDecoder::HexForm hexForm);
    void     ReadTextPreview       (const std::wstring & path, const std::wstring & name);

    void  PreviewEntry       (const std::wstring & imagePath, const std::string & entryPath, const FileEntry & entry, VolumeKind kind, std::span<const Byte> diskSectors = {}, std::span<const std::vector<Byte>> otherDisks = {});
    void  PreviewSearchMatch (const CatalogRow & row);

    //  The preview made again for the selection, as when a slow read arrives,
    //  and as PreviewViewState does after a choice changes.
    void  UpdatePreview      () override;

    //  The choices made for the files previewed.
    PreviewViewState        & GetViewState ()       { return m_viewState; }
    const PreviewViewState  & GetViewState () const { return m_viewState; }

    //  The monitor a picture preview is drawn for.
    PictureMonitorChoice        & GetMonitorChoice ()       { return m_monitor; }
    const PictureMonitorChoice  & GetMonitorChoice () const { return m_monitor; }

private:
    Location  GetLocation () const { return m_parts.model.HasTabs() ? m_parts.model.GetActiveTab().location : Location(); }

    BrowserParts              m_parts;
    const BrowserSelection  & m_selection;
    IBrowserPreviewHost     & m_host;

    MemoryBus                           m_bus;
    PreviewContent                      m_preview;
    PreviewViewState                    m_viewState      { *this };
    PictureMonitorChoice                m_monitor        { m_preview, m_bus };
    PreviewParts                        m_previewParts   { m_preview, m_bus, m_viewState, m_monitor };
};

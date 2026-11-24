#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Browser/BrowserTypes.h"
#include "CassoExplorer/Browser/Preview/DiskImagePreviewer.h"
#include "CassoExplorer/Browser/Preview/IPreviewRefresh.h"
#include "CassoExplorer/Browser/Preview/PictureMonitorChoice.h"
#include "CassoExplorer/Browser/Preview/PreviewParts.h"
#include "CassoExplorer/Browser/Preview/PreviewViewState.h"
#include "CassoExplorer/Browser/Preview/ShellItemPreviewer.h"
#include "CassoExplorer/Model/BrowserModel.h"

#include "CassoExplorer/Model/PreviewDecoder.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Core/MemoryBus.h"

class BrowserArchives;
class BrowserSearchMatches;
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
    BrowserPreviewer (const BrowserParts & parts, const BrowserSelection & selection, BrowserSearchMatches & searchMatches, const BrowserArchives & archives, IBrowserPreviewHost & host);

    //  The largest host file that previews as text rather than in the hex view.
    static constexpr size_t  s_kTextPreviewMaxBytes = 1024 * 1024;

    const PreviewContent  & GetPreview () const { return m_preview; }

    void  PreviewAppleEntry    (const std::string & entryPath, const FileEntry & entry);
    bool  PreviewAppleDrive    (const CatalogRow & row);
    void  PreviewCassoLinkItem (const CatalogRow & row);

    bool     TryPreviewAppleSingle (const std::wstring & folder, const CatalogRow & row);
    void     PreviewHostFile       (const std::wstring & folder, const CatalogRow & row);
    bool     TryReadHostPayload    (const std::wstring & path, const CatalogRow & row, FilePayload & outPayload, FileEntry & outEntry, bool & outTyped);
    void     AttachHostViews       (const FileEntry & entry, const FilePayload & payload, bool loaded, PreviewDecoder::HexForm hexForm);
    void     ReadTextPreview       (const std::wstring & path, const std::wstring & name);

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

    //  The preview of a file or archive the shell lists.
    ShellItemPreviewer        & GetShellItems ()       { return m_shellItems; }
    const ShellItemPreviewer  & GetShellItems () const { return m_shellItems; }

private:
    Location  GetLocation () const { return m_parts.model.HasTabs() ? m_parts.model.GetActiveTab().location : Location(); }

    BrowserParts              m_parts;
    const BrowserSelection  & m_selection;
    IBrowserPreviewHost     & m_host;
    BrowserSearchMatches    & m_searchMatches;
    const BrowserArchives   & m_archives;

    MemoryBus                           m_bus;
    PreviewContent                      m_preview;
    PreviewViewState                    m_viewState      { *this };
    PictureMonitorChoice                m_monitor        { m_preview, m_bus };
    PreviewParts                        m_previewParts   { m_preview, m_bus, m_viewState, m_monitor };
    DiskImagePreviewer                  m_diskImages     { m_parts, m_previewParts };
    ShellItemPreviewer                  m_shellItems     { m_parts, m_previewParts, m_archives, m_diskImages };
};

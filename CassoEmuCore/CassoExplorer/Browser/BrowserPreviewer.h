#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Browser/Preview/CassoLinkPreviewer.h"
#include "CassoExplorer/Browser/Preview/DiskImagePreviewer.h"
#include "CassoExplorer/Browser/Preview/HostFilePreviewer.h"
#include "CassoExplorer/Browser/Preview/IPreviewRefresh.h"
#include "CassoExplorer/Browser/Preview/PictureMonitorChoice.h"
#include "CassoExplorer/Browser/Preview/PreviewParts.h"
#include "CassoExplorer/Browser/Preview/PreviewViewState.h"
#include "CassoExplorer/Browser/Preview/ShellItemPreviewer.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "Core/MemoryBus.h"

class BrowserArchives;
class BrowserSearchMatches;
class BrowserSelection;
class IBrowserPreviewHost;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserPreviewer
//
//  The preview of the selection. UpdatePreview calls the previewer for the
//  kind of row selected: a disk image or a file in one, a shell item or an
//  archive, a host file, a search match, or a CassoLink drive or item.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserPreviewer : private IPreviewRefresh
{
public:
    BrowserPreviewer (const BrowserParts      & parts,
                      const BrowserSelection  & selection,
                      BrowserSearchMatches    & searchMatches,
                      const BrowserArchives   & archives,
                      IBrowserPreviewHost     & host);

    //  The preview made again for the selection, as when a slow read arrives,
    //  and as PreviewViewState does after a choice changes.
    void  UpdatePreview () override;

    const PreviewContent  & GetPreview () const { return m_preview; }

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
    void      PreviewSearchMatch (const CatalogRow & row);
    Location  GetLocation        () const { return m_parts.model.HasTabs() ? m_parts.model.GetActiveTab().location : Location(); }

    BrowserParts              m_parts;
    const BrowserSelection  & m_selection;
    IBrowserPreviewHost     & m_host;
    BrowserSearchMatches    & m_searchMatches;
    const BrowserArchives   & m_archives;

    PreviewContent                      m_preview;
    MemoryBus                           m_bus;
    PreviewViewState                    m_viewState      { *this };
    PictureMonitorChoice                m_monitor        { m_preview, m_bus };
    PreviewParts                        m_previewParts   { m_preview, m_bus, m_viewState, m_monitor };
    DiskImagePreviewer                  m_diskImages     { m_parts, m_previewParts };
    ShellItemPreviewer                  m_shellItems     { m_parts, m_previewParts, m_archives, m_diskImages };
    HostFilePreviewer                   m_hostFiles      { m_parts, m_previewParts };
    CassoLinkPreviewer                  m_cassoLink      { m_parts, m_previewParts };
};

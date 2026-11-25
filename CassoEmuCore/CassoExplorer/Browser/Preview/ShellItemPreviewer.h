#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Browser/Preview/IImagePreviews.h"
#include "CassoExplorer/Browser/Preview/PreviewParts.h"

class BrowserArchives;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemPreviewer
//
//  The preview of a file or archive the shell lists: a file's bytes, an
//  archive's files in the columns of the file list, and a disk image in a zip
//  as its catalog.
//
////////////////////////////////////////////////////////////////////////////////

class ShellItemPreviewer
{
public:
    ShellItemPreviewer (const BrowserParts    & parts,
                        const PreviewParts    & previewParts,
                        const BrowserArchives & archives,
                        IImagePreviews        & images);

    //  How much of a file inside a zip, or another file with no path on the
    //  disk, is read for its preview.
    static constexpr size_t  kShellPreviewMaxBytes = 16 * 1024 * 1024;

    //  An archive's preview in the columns of the file list when the archive is
    //  open: its first columns, with their icons.
    static constexpr size_t                    kArchivePreviewColumns = 4;
    static std::vector<DxuiListView::Column>   GetArchivePreviewColumns ();
    std::vector<DxuiListView::Cell>            GetArchivePreviewCells   (const CatalogRow & row) const;

    void  PreviewShellItem (const CatalogRow & row);
    void  PreviewArchive   (const std::wstring & path);

    //  A shell preview that failed or took too long is replaced by the file's
    //  bytes.
    void  ShowShellPreviewBytes ();

private:
    BrowserParts             m_parts;
    PreviewParts             m_previewParts;
    const BrowserArchives  & m_archives;
    IImagePreviews         & m_images;
};

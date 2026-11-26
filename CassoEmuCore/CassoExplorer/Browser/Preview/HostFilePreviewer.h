#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Browser/Preview/PreviewParts.h"
#include "CassoExplorer/Model/PreviewDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostFilePreviewer
//
//  The preview of a file in a host folder: an AppleSingle file as the file it
//  contains, a file with its Apple type in its name as it previews in its
//  disk, and any other file as its bytes, a Windows preview or text.
//
////////////////////////////////////////////////////////////////////////////////

class HostFilePreviewer
{
public:
    HostFilePreviewer (const BrowserParts & parts, const PreviewParts & previewParts);

    //  The largest host file that previews as text rather than in the hex view.
    static constexpr size_t  kTextPreviewMaxBytes = 1024 * 1024;

    void  PreviewHostFile (const std::wstring & folder, const CatalogRow & row);

private:
    bool  TryPreviewAppleSingle (const std::wstring & folder, const CatalogRow & row);
    bool  TryReadHostPayload    (const std::wstring & path, const CatalogRow & row, FilePayload & outPayload, FileEntry & outEntry, bool & outTyped);
    void  AttachHostViews       (const FileEntry & entry, const FilePayload & payload, bool loaded, PreviewDecoder::HexForm hexForm);
    void  ReadTextPreview       (const std::wstring & path, const std::wstring & name);

    BrowserParts  m_parts;
    PreviewParts  m_previewParts;
};

#include "Pch.h"

#include "CassoExplorer/Model/ImageFileSearchSources.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "CassoExplorer/Model/PreviewSaveChoices.h"
#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageFileSearchSources::ImageFileSearchSources
//
////////////////////////////////////////////////////////////////////////////////

ImageFileSearchSources::ImageFileSearchSources (DiskOperations & operations, const std::string & image, const std::string & catalogPath, const FileEntry & entry, VolumeKind kind) :
    m_operations  (operations),
    m_image       (image),
    m_catalogPath (catalogPath),
    m_entry       (entry),
    m_kind        (kind)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageFileSearchSources::ContainsText
//
////////////////////////////////////////////////////////////////////////////////

bool ImageFileSearchSources::ContainsText (const std::wstring & lowerText)
{
    if (!m_isRead)
    {
        m_isRead  = true;
        m_hasText = ReadText (m_operations, m_image, m_catalogPath, m_entry, m_kind, m_text);
    }

    return m_hasText && m_text.find (lowerText) != std::wstring::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageFileSearchSources::ReadText
//
////////////////////////////////////////////////////////////////////////////////

bool ImageFileSearchSources::ReadText (DiskOperations & operations, const std::string & image, const std::string & catalogPath, const FileEntry & entry, VolumeKind kind, std::wstring & outLowerText)
{
    FilePayload             payload;
    DiskOperations::Result  result;



    outLowerText.clear();

    if (entry.isDirectory || (entry.hasEofBytes && entry.eofBytes > kLargestFile))
    {
        return false;
    }

    result = operations.Read (image, catalogPath, payload, entry.catalogIndex);

    return result.Succeeded() && GetPayloadText (entry, kind, payload, outLowerText);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageFileSearchSources::GetPayloadText
//
////////////////////////////////////////////////////////////////////////////////

bool ImageFileSearchSources::GetPayloadText (const FileEntry & entry, VolumeKind kind, const FilePayload & payload, std::wstring & outLowerText)
{
    PreviewContent  content;
    MemoryBus       bus;
    HRESULT         hr      = S_OK;
    bool            isText  = false;



    outLowerText.clear();

    if (entry.isDirectory || payload.bytes.size() > kLargestFile)
    {
        return false;
    }

    hr = PreviewDecoder::Render (entry, kind, payload, PreviewDecoder::HexForm::Bytes, bus, content);

    isText = SUCCEEDED (hr) && (content.kind == PreviewContent::Kind::Text     || content.kind == PreviewContent::Kind::Listing
                             || content.kind == PreviewContent::Kind::Document || content.kind == PreviewContent::Kind::Grid
                             || content.kind == PreviewContent::Kind::Variables);

    if (!isText)
    {
        return false;
    }

    outLowerText = PreviewSaveChoices::GetViewText (content);
    std::transform (outLowerText.begin(), outLowerText.end(), outLowerText.begin(), [] (wchar_t ch) { return (wchar_t) towlower (ch); });

    return true;
}

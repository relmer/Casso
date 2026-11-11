#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/ISearchSources.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageFileSearchSources
//
//  A file in a disk image's contents for a search with File contents on:
//  the text its preview draws, read the first time a term tests it. A
//  BASIC program is searched detokenized, Apple text as text, and a
//  document, table or VAR file as the text of its view. A binary file, a
//  picture or a file larger than kLargestFile has no text to search.
//
////////////////////////////////////////////////////////////////////////////////

class ImageFileSearchSources : public ISearchSources
{
public:
    ImageFileSearchSources (DiskOperations & operations, const std::string & image, const std::string & catalogPath, const FileEntry & entry, VolumeKind kind);

    bool                       ContainsText (const std::wstring & lowerText) override;
    Lookup                     GetProperty  (const std::wstring &, std::wstring &) override { return Lookup::UnknownProperty; }
    std::vector<std::wstring>  GetKinds     () override                                     { return {}; }
    std::wstring               GetTypeText  () override                                     { return {}; }

    //  The file's searchable text, lowercased; false for a file with none.
    static bool  ReadText (DiskOperations & operations, const std::string & image, const std::string & catalogPath, const FileEntry & entry, VolumeKind kind, std::wstring & outLowerText);

    static constexpr uint32_t  kLargestFile = 1024 * 1024;

private:
    DiskOperations  & m_operations;
    std::string       m_image;
    std::string       m_catalogPath;
    FileEntry         m_entry;
    VolumeKind        m_kind;
    bool              m_isRead  = false;
    bool              m_hasText = false;
    std::wstring      m_text;
};

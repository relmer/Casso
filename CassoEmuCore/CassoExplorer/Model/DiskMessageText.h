#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMessageText
//
//  The text for a disk image or a linked drive that fails to open, in the
//  tree, the list and the preview.
//
////////////////////////////////////////////////////////////////////////////////

class DiskMessageText
{
public:
    static constexpr const wchar_t *  kNoFilesystemText     = L"can't be previewed because it doesn't have a recognized file system.";
    static constexpr const wchar_t *  kDamagedPartitionText = L"is damaged: it ends past the end of the image.";

    static std::wstring  FormatImageError        (const std::wstring & imagePath, const std::string & message);
    static std::wstring  FormatLinkError         (HRESULT result, const std::string & message);
    static std::wstring  ShortenPaths            (const std::wstring & message);
    static std::wstring  BreakAfterFirstSentence (std::wstring text);
};

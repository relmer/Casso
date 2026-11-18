#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/PreviewDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewSaveChoices
//
//  The file types the preview's Save as offers, and the text of the
//  current view, which Save as and Copy write.
//
//  The types, in order: the current view, as a PNG picture or as text as
//  displayed; the file as Get copies it, with the extension of Get's host
//  name for it; the file's original bytes; and an AppleSingle file, with
//  the file's type and resource fork. A type with an extension already in
//  the list is left out, since the save dialog's choice is read back from
//  the extension of the name picked.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewSaveChoices
{
public:
    enum class Kind { ViewPicture, ViewText, Converted, Original, AppleSingle };

    struct Choice
    {
        Kind          kind = Kind::Original;
        std::wstring  label;        // "PNG picture"
        std::wstring  extension;    // "png", without the dot
    };

    //  `convertedName` is Get's host name for the file.
    static void  Build (const PreviewContent & preview, const std::wstring & convertedName, std::vector<Choice> & outChoices);

    //  The choice whose extension the picked name has, or the original bytes
    //  for any other extension.
    static Kind  Match (const std::vector<Choice> & choices, const std::filesystem::path & picked);

    //  The view as text: lines as drawn, a hex dump of 16 bytes to a line, a
    //  document's paragraphs, or a table's cells separated by tabs. Lines
    //  end in CR LF.
    static std::wstring  GetViewText (const PreviewContent & preview);

    static constexpr size_t           kHexBytesPerLine = 16;
    static constexpr const wchar_t *  kLineEnd         = L"\r\n";
};

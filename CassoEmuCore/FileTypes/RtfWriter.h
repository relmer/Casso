#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RtfWriter
//
//  A document as RTF 1.5, with only bold, italic, underline, the four
//  alignments, tabs and paragraph ends. A character outside ASCII is a \u
//  escape of its UTF-16 unit, with ? for readers without Unicode.
//
////////////////////////////////////////////////////////////////////////////////

class RtfWriter
{
public:
    static void  Write (const std::vector<Paragraph> & paragraphs, std::vector<Byte> & outBytes);

private:
    static void  AppendText (const std::wstring & text, std::string & inOutRtf);
};

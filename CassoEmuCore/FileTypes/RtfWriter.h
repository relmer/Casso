#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RtfWriter
//
//  A document as RTF 1.5, with only bold, italic, underline, three colors,
//  the four alignments, tabs, PNG pictures and paragraph ends. A character outside ASCII is a \u escape of its
//  UTF-16 unit, with ? for readers without Unicode.
//
////////////////////////////////////////////////////////////////////////////////

class RtfWriter
{
public:
    static void  Write (const std::vector<Paragraph> & paragraphs, std::vector<Byte> & outBytes);

private:
    static constexpr int  s_kTwipsPerPixel = 15;     // 96 pixels an inch

    static void  AppendText    (const std::wstring & text, std::string & inOutRtf);
    static void  AppendPicture (const Paragraph & paragraph, std::string & inOutRtf);
};

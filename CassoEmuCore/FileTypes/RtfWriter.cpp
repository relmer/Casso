#include "Pch.h"

#include "FileTypes/RtfWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RtfWriter::Write
//
//  Each run sets every style it has, so a run is styled the same whichever
//  run is before it.
//
////////////////////////////////////////////////////////////////////////////////

void RtfWriter::Write (const std::vector<Paragraph> & paragraphs, std::vector<Byte> & outBytes)
{
    std::string  rtf = "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Courier New;}}\r\n";



    for (const Paragraph & paragraph : paragraphs)
    {
        switch (paragraph.alignment)
        {
            case Alignment::Center:    rtf += "\\pard\\qc ";  break;
            case Alignment::Right:     rtf += "\\pard\\qr ";  break;
            case Alignment::Justified: rtf += "\\pard\\qj ";  break;
            default:                   rtf += "\\pard\\ql ";  break;
        }

        for (const Run & run : paragraph.runs)
        {
            rtf += run.bold      ? "\\b "   : "\\b0 ";
            rtf += run.italic    ? "\\i "   : "\\i0 ";
            rtf += run.underline ? "\\ul "  : "\\ulnone ";

            AppendText (run.text, rtf);
        }

        rtf += "\\par\r\n";
    }

    rtf += "}\r\n";

    outBytes.assign (rtf.begin(), rtf.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  RtfWriter::AppendText
//
////////////////////////////////////////////////////////////////////////////////

void RtfWriter::AppendText (const std::wstring & text, std::string & inOutRtf)
{
    for (wchar_t c : text)
    {
        if (c == L'\\' || c == L'{' || c == L'}')
        {
            inOutRtf += '\\';
            inOutRtf += (char) c;
        }
        else if (c == L'\t')
        {
            inOutRtf += "\\tab ";
        }
        else if (c < 0x20)
        {
            inOutRtf += ' ';
        }
        else if (c < 0x80)
        {
            inOutRtf += (char) c;
        }
        else
        {
            inOutRtf += std::format ("\\u{}?", (int) (int16_t) c);
        }
    }
}

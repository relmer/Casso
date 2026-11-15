#include "Pch.h"

#include "FileTypes/RtfWriter.h"
#include "CassoExplorer/Model/PicturePreview.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RtfWriter::Write
//
//  Each run sets every style and its color, so a run is styled the same
//  whichever run is before it. A picture is a PNG at 96 pixels an inch.
//
////////////////////////////////////////////////////////////////////////////////

void RtfWriter::Write (const std::vector<Paragraph> & paragraphs, std::vector<Byte> & outBytes)
{
    std::string  rtf = "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Courier New;}}"
                       "{\\colortbl ;\\red0\\green128\\blue0;\\red192\\green0\\blue0;\\red128\\green128\\blue128;}\r\n";



    for (const Paragraph & paragraph : paragraphs)
    {
        switch (paragraph.alignment)
        {
            case Alignment::Center:    rtf += "\\pard\\qc ";  break;
            case Alignment::Right:     rtf += "\\pard\\qr ";  break;
            case Alignment::Justified: rtf += "\\pard\\qj ";  break;
            default:                   rtf += "\\pard\\ql ";  break;
        }

        if (!paragraph.picture.empty())
        {
            AppendPicture (paragraph, rtf);
        }

        for (const Run & run : paragraph.runs)
        {
            rtf += std::format ("\\cf{} ", (int) run.color);
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
//  RtfWriter::AppendPicture
//
////////////////////////////////////////////////////////////////////////////////

void RtfWriter::AppendPicture (const Paragraph & paragraph, std::string & inOutRtf)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  png;



    hr = PicturePreview::EncodePng (paragraph.picture, paragraph.width, paragraph.height, png);

    if (SUCCEEDED (hr))
    {
        inOutRtf += std::format ("{{\\pict\\pngblip\\picw{}\\pich{}\\picwgoal{}\\pichgoal{} ",
                                 paragraph.width, paragraph.height, paragraph.width * s_kTwipsPerPixel, paragraph.height * s_kTwipsPerPixel);

        for (size_t i = 0; i < png.size(); i++)
        {
            inOutRtf += std::format ("{:02x}", png[i]);

            if (i % 64 == 63)
            {
                inOutRtf += "\r\n";
            }
        }

        inOutRtf += "}";
    }
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

#include "Pch.h"

#include "AppleFileName.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ToDisplay
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleFileName::ToDisplay (const std::string & name)
{
    static constexpr wchar_t  s_kControlPictures = 0x2400;   // U+2400 SYMBOL FOR NULL
    static constexpr wchar_t  s_kDeletePicture   = 0x2421;   // U+2421 SYMBOL FOR DELETE
    std::wstring              text;



    text.reserve (name.size());

    for (char raw : name)
    {
        unsigned  ch = (unsigned) (unsigned char) raw & 0x7Fu;

        if (ch < 0x20u)
        {
            text += (wchar_t) (s_kControlPictures + ch);
        }
        else if (ch == 0x7Fu)
        {
            text += s_kDeletePicture;
        }
        else
        {
            text += (wchar_t) ch;
        }
    }

    return text;
}

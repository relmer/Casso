#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileName
//
//  Turns a file name read from a DOS 3.3 or ProDOS directory into text for
//  the host to show. The bytes are Apple's character set, not host text: a
//  code-page conversion is as wrong for them as widening each char, which
//  sign-extends a high-bit byte into U+FF80..U+FFFF.
//
////////////////////////////////////////////////////////////////////////////////

class AppleFileName
{
public:
    //  Clears each byte's high bit and draws a control character as its
    //  Unicode control picture, so an inverse or control character in a name
    //  stays visible instead of vanishing or moving the text.
    static std::wstring  ToDisplay (const std::string & name);
};

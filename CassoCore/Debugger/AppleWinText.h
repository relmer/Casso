#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinText
//
//  The words of an AppleWin-style command line: split on blanks, upper
//  cased, and joined back.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinText
{
public:
    using Tokens = std::vector<std::string>;

    static Tokens       Split   (const std::string & text);
    static std::string  ToUpper (const std::string & text);
    static std::string  Join    (const Tokens & tokens, size_t first);
};

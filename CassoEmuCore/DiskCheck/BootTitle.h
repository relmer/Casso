#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootTitle
//
//  A program's title from the text screen it draws once booted. A line is
//  the title when a line with COPYRIGHT or (C) is within three lines under
//  it, or when it is the one line of the screen's first four that isn't
//  blank. The punctuation at its ends is dropped, and its letters are as
//  drawn. A screen in a graphics mode has no title.
//
////////////////////////////////////////////////////////////////////////////////

class BootTitle
{
public:
    static std::string  Find (const std::vector<std::string> & rows, bool isTextMode);

    //  A title as a name for its disk: each colon is a hyphen with one
    //  space on each side, and a title with no small letters is in title
    //  case, as The Witness - An Interlogic Mystery from
    //  THE WITNESS: AN INTERLOGIC MYSTERY. A Roman numeral and DOS are left
    //  in capitals.
    static std::string  ToName (const std::string & title);

private:
    static constexpr size_t  s_kCopyrightReach = 3;
    static constexpr size_t  s_kTopRows        = 4;
    static constexpr size_t  s_kMinLetters     = 2;

    static bool         IsBlank      (const std::string & row);
    static std::string  ToTitleCase  (const std::string & word, bool isFirst);
    static bool         HasCopyright (const std::string & row);
    static std::string  Trim         (const std::string & row);
};

#include "Pch.h"

#include "Debugger/AppleWinText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinText::Split
//
////////////////////////////////////////////////////////////////////////////////

AppleWinText::Tokens AppleWinText::Split (const std::string & text)
{
    Tokens              tokens;
    std::istringstream  stream (text);
    std::string         token;



    while (stream >> token)
    {
        tokens.push_back (token);
    }

    return tokens;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinText::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinText::ToUpper (const std::string & text)
{
    std::string  upper (text);



    for (char & ch : upper)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    return upper;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinText::Join
//
//  Tokens from first onward, space-separated, in their original case.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinText::Join (const Tokens & tokens, size_t first)
{
    std::string  joined;



    for (size_t i = first; i < tokens.size(); ++i)
    {
        joined += (joined.empty() ? "" : " ") + tokens[i];
    }

    return joined;
}

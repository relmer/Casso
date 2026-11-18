#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineQuoting
//
//  One argument written into a Windows command line so CommandLineToArgvW,
//  and the C runtime's argv parsing, return exactly that argument.
//
////////////////////////////////////////////////////////////////////////////////

class CommandLineQuoting
{
public:
    //  The argument in quotes, with each quote escaped and the backslashes
    //  before a quote or the closing quote doubled.
    static std::wstring  QuoteArgument (const std::wstring & argument);
};

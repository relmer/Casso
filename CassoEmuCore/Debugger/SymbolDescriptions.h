#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolDescriptions
//
//  A line saying what a shipped ROM symbol is -- a soft switch, a zero-page
//  location, a Monitor or Applesoft routine -- for a tooltip.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolDescriptions
{
public:
    //  Nothing for a name the shipped tables do not have.
    static const char *  Find (const std::string & name);
};

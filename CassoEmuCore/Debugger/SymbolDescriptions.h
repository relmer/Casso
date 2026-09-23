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

    //  Which of the names an address carries belongs to an access in this
    //  direction. One soft-switch address is two switches -- $C000 read is
    //  the keyboard, $C000 written turns 80STORE off -- and the shipped
    //  descriptions say which each one is, in their opening word. An address
    //  with one name keeps it whichever way it is touched.
    static std::string   ChooseByDirection (const std::vector<std::string> & names, bool isWrite);

    //  What touching this switch does, with the leading "Read:", "Write:" or
    //  "Read or write:" dropped: the instruction already shows the direction,
    //  and the pane has room for the action, not for saying it twice. Empty
    //  for a symbol with no shipped description.
    static std::string   GetAction (const std::string & name);
};

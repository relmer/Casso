#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  CountedNoun
//
//  A count with the thing it counts: "1 block", "5 blocks", "0 errors".
//
//  THE UNIT BELONGS WITH THE NUMBER. A message that prints the count alone
//  leaves the reader working out what was counted, and one that prints
//  "1 block(s)" tells them the program could not work out which it was. The
//  tree carried twenty of the second kind before this existed.
//
//  English only, and deliberately so: the rule here is "add s unless the count
//  is one", which is wrong in most other languages and would have to be
//  replaced wholesale rather than extended if Casso were ever translated. It
//  is also wrong for an irregular plural, so a unit whose plural is not formed
//  with s cannot go through this.
//
////////////////////////////////////////////////////////////////////////////////

class CountedNoun
{
public:
    //  `singular` is the unit's singular form: "block", "sector", "byte".
    static std::string  Of (long long count, const char * singular);
};

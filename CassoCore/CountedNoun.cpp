#include "Pch.h"

#include "CountedNoun.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CountedNoun::Of
//
//  Zero takes the plural, which is what English does: "0 errors", not
//  "0 error". Only exactly one is singular.
//
////////////////////////////////////////////////////////////////////////////////

std::string CountedNoun::Of (long long count, const char * singular)
{
    std::string  text = std::to_string (count) + " " + singular;



    if (count != 1)
    {
        text += "s";
    }

    return text;
}

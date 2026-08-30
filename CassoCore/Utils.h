#pragma once





//  Windows spells this one itself, identically, in winnt.h. Ours is here for
//  the translation units that never see a Windows header -- the emulator core
//  is meant to build without one -- so it defers rather than competing.
#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#endif





////////////////////////////////////////////////////////////////////////////////
//
//  Utils
//
//  Helpers small enough that a class of their own would be ceremony. A
//  namespace rather than a class because there is no shared state for a class
//  to hold; the tree already does this for CrtPresets, RepoCheckout and
//  ChromeMetrics.
//
////////////////////////////////////////////////////////////////////////////////

namespace Utils
{
    //  Picks the word that agrees with `count`. BOTH FORMS ARE SPELLED OUT by
    //  the caller, which is the whole point: a helper that synthesized the
    //  plural could only ever add an s, so "index"/"indices" and "match"/
    //  "matches" would have to route around it, and nothing in the signature
    //  would warn you. It is also the shape that survives translation, where
    //  the two forms become two resource ids rather than one plus a rule.
    //
    //  Only exactly one is singular, so zero takes the plural: "0 errors",
    //  which is what English does.
    inline const char *  GetSingularOrPluralForm (long long    count,
                                                  const char * singular,
                                                  const char * plural)
    {
        return (count == 1) ? singular : plural;
    }
}

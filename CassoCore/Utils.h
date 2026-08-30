#pragma once





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
    //  Picks the word that agrees with `count`. THE CALLER SUPPLIES BOTH FORMS,
    //  which is the point: a helper that derived the plural could only append
    //  an s, so "index"/"indices" and "match"/"matches" would have to route
    //  around it, and the signature would give no warning. It is also the form
    //  that survives translation, where the two forms become two resource ids
    //  rather than one plus a rule.
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

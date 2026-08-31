#include "Pch.h"
#include "Utils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  UtilsTests
//
//  The agreement rule every user-visible count goes through. Small enough to
//  state completely: one takes the singular, everything else takes the plural.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (UtilsTests)
{
public:

    TEST_METHOD (GetSingularOrPluralForm_OneIsSingular)
    {
        Assert::AreEqual ("block",  Utils::GetSingularOrPluralForm (1, "block", "blocks"));
        Assert::AreEqual ("sector", Utils::GetSingularOrPluralForm (1, "sector", "sectors"));
    }

    //  ZERO TAKES THE PLURAL, which is what English does and what the (s)
    //  spelling this replaced could not express: "0 errors", never "0 error".
    TEST_METHOD (GetSingularOrPluralForm_ZeroTakesThePlural)
    {
        Assert::AreEqual ("errors", Utils::GetSingularOrPluralForm (0, "error", "errors"));
    }

    TEST_METHOD (GetSingularOrPluralForm_EverythingElseTakesThePlural)
    {
        Assert::AreEqual ("blocks", Utils::GetSingularOrPluralForm (2, "block", "blocks"));
        Assert::AreEqual ("blocks", Utils::GetSingularOrPluralForm (280, "block", "blocks"));
    }

    //  A negative count is not a thing any caller should produce, but it must
    //  not read as a singular if one ever does.
    TEST_METHOD (GetSingularOrPluralForm_NegativeTakesThePlural)
    {
        Assert::AreEqual ("sectors", Utils::GetSingularOrPluralForm (-1, "sector", "sectors"));
    }

    //  THE POINT OF TAKING BOTH FORMS. A helper that added an s could only
    //  ever produce "indexs" and "matchs" here, and nothing in its signature
    //  would have warned the caller. Casso has no irregular unit today; this
    //  test is what makes adding one a non-event.
    TEST_METHOD (GetSingularOrPluralForm_HandlesPluralsNotFormedWithS)
    {
        Assert::AreEqual ("indices", Utils::GetSingularOrPluralForm (3, "index", "indices"));
        Assert::AreEqual ("matches", Utils::GetSingularOrPluralForm (2, "match", "matches"));
        Assert::AreEqual ("index",   Utils::GetSingularOrPluralForm (1, "index", "indices"));
    }

    //  A multi-word unit is one unit. The CMOS fill line counts "CMOS slots",
    //  and the caller supplies both forms rather than relying on a rule for
    //  where in the phrase an s would land.
    TEST_METHOD (GetSingularOrPluralForm_HandlesAMultiWordUnit)
    {
        Assert::AreEqual ("CMOS slots", Utils::GetSingularOrPluralForm (46, "CMOS slot", "CMOS slots"));
        Assert::AreEqual ("CMOS slot",  Utils::GetSingularOrPluralForm (1, "CMOS slot", "CMOS slots"));
    }
};

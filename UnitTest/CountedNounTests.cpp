#include "Pch.h"
#include "CountedNoun.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CountedNounTests
//
//  The plural rule every user-visible count goes through. Small enough to
//  state completely: one is singular, everything else takes the s.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (CountedNounTests)
{
public:

    TEST_METHOD (Of_OneIsSingular)
    {
        Assert::AreEqual (std::string ("1 block"),  CountedNoun::Of (1, "block"));
        Assert::AreEqual (std::string ("1 sector"), CountedNoun::Of (1, "sector"));
    }

    //  ZERO TAKES THE PLURAL, which is what English does and what the (s)
    //  spelling this replaced could not express: "0 errors", never "0 error".
    TEST_METHOD (Of_ZeroTakesThePlural)
    {
        Assert::AreEqual (std::string ("0 errors"), CountedNoun::Of (0, "error"));
    }

    TEST_METHOD (Of_EverythingElseTakesThePlural)
    {
        Assert::AreEqual (std::string ("2 blocks"),   CountedNoun::Of (2, "block"));
        Assert::AreEqual (std::string ("280 blocks"), CountedNoun::Of (280, "block"));
    }

    //  A count is not always a bare word: the CMOS fill line counts "CMOS
    //  slots", and the s belongs on the end of the phrase rather than inside
    //  it.
    TEST_METHOD (Of_PutsTheSAtTheEndOfAMultiWordUnit)
    {
        Assert::AreEqual (std::string ("46 CMOS slots"), CountedNoun::Of (46, "CMOS slot"));
        Assert::AreEqual (std::string ("1 CMOS slot"),   CountedNoun::Of (1, "CMOS slot"));
    }

    //  A negative count is not a thing any caller should produce, but it must
    //  not read as a singular if one ever does.
    TEST_METHOD (Of_NegativeTakesThePlural)
    {
        Assert::AreEqual (std::string ("-1 sectors"), CountedNoun::Of (-1, "sector"));
    }
};
